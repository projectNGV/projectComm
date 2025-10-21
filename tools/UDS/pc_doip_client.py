import sys
import os
import struct # 0x2A 요청 데이터 생성을 위해 추가
import time
import queue
import threading
from functools import partial

import tkinter as tk
from tkinter import scrolledtext, font as tkFont, filedialog

# UDSonCAN 및 DoIPClient 라이브러리 임포트
from doipclient import DoIPClient
from doipclient.connectors import DoIPClientUDSConnector
from udsoncan.client import Client
from udsoncan.exceptions import *
from udsoncan.configs import default_client_config
# Request와 services를 임포트 (0x2A, 0x19 등 직접 구성 시 필요)
from udsoncan import DidCodec, services, Request 

from typing import Any, Tuple, Dict, Optional

from ota.ota_main import run_ota_process


# --- 설정값 ---
RPI_HOST = '192.168.50.2' # 실제 라즈베리파이 IP
ECU_LOGICAL_ADDRESS = 0x1000 # ECU 논리 주소 (필요시 수정)

# --- ✨ [추가] 참조 스크립트의 상수 ---
TOF_SENSOR_DID = 0x1000
ULTRASONIC_SENSOR_DID = 0x2000
ULTRASONIC_RIGHT_SENSOR_DID = 0x2001
ULTRASONIC_REAR_SENSOR_DID = 0x2002
TOF_INVALID_VALUE = 0xFFFFFF
US_INVALID_VALUE = 0xFFFF # 초음파 에러 값 (가정)


current_session_type = 0x01  # 0x01: Default, 0x03: Extended


# --- DID 및 Codec 정의 ---
DID_DESCRIPTIONS: Dict[int, Tuple[str, Optional[DidCodec]]] = {
    TOF_SENSOR_DID: ("레이저 센서 거리 (mm)", DidCodec('3s')),
    ULTRASONIC_SENSOR_DID: ("초음파(좌) 센서 거리 (x0.1 cm)", DidCodec('>H')),
    0x2001: ("초음파(우) 센서 거리 (x0.1 cm)", DidCodec('>H')),
    0x2002: ("초음파(후) 센서 거리 (x0.1 cm)", DidCodec('>H')),
    0x3000: ("AEB 기능 플래그", DidCodec('B')),
    0xF186: ("현재 세션 정보", DidCodec('B')),
    0xF187: ("ECU 부품 번호", DidCodec('20s')),
    0xF18C: ("ECU 시리얼 번호", DidCodec('20s')),
    0xF190: ("차대번호 (VIN)", DidCodec('18s')),
    0xF192: ("ECU 공급업체 정보", DidCodec('20s')),
    0xF193: ("ECU 제조 날짜", DidCodec('11s')),
    0xF1A0: ("지원 DID 목록", DidCodec('22s'))
}

# UDSonCAN 클라이언트 설정
uds_config = {
    'data_identifiers': {did: codec for did, (_, codec) in DID_DESCRIPTIONS.items() if codec is not None},
    'p2_timeout': 10,
    'p2_star_timeout': 15,
    'request_timeout': 30, # 넉넉하게 5초
}

# --- 전역 변수 ---
task_queue = queue.Queue() # GUI 버튼 -> worker 스레드
stop_event = threading.Event() # 모든 스레드 종료 신호

# --- ✨ [수정] 실시간 0x2A 수신용 전역 변수 ---
periodic_data_queue = queue.Queue() # Listener 스레드 -> GUI 스레드

# 수신 리스너 스레드 제어
stop_listener_event = threading.Event()
g_listener_thread = None
is_listener_active = False # 리스너 모드 활성화 여부

# 개별 센서 0x2A 시작/중지 상태
is_laser_periodic_on = False
is_us_left_periodic_on = False

# 비활성화할 버튼 목록
g_all_buttons = []


def print_hex(prefix: str, data: bytes):
    """바이트 데이터를 보기 쉬운 16진수 문자열로 콘솔에 출력합니다."""
    if not isinstance(data, bytes):
        print(f"{prefix} [ Invalid data type: {type(data)} ]")
        return
    hex_str = ' '.join([f'0x{byte:02X}' for byte in data])
    print(f"{prefix} [ {hex_str} ]")

# --- 통신 처리 백그라운드 스레드 ---
def communication_thread(client, root_window):
    """
    [기존] GUI의 버튼 요청을 처리하는 스레드 (task_queue 담당)
    (0x2A 시작/중지 요청도 이 스레드가 처리)
    """
    print("✅ [PY_THREAD_Worker] UDS 워커 스레드 시작. GUI 요청 대기 중...")
    while not stop_event.is_set():
        try:
            task = task_queue.get(timeout=10)
            
            task_name = task['function'].__name__
            print(f"\n[PY_THREAD_Worker] 🔄 작업 시작: {task_name} (Args: {task.get('args', [])})")
            
            func = task['function']
            args = task.get('args', [])
            kwargs = task.get('kwargs', {})
            
            func(client, *args, **kwargs)

        except queue.Empty:
            continue
        except Exception as e:
            # 리스너 스레드가 활성화되면, client 요청은 실패(타임아웃)할 수 있음
            if not is_listener_active:
                print(f"❌ [PY_THREAD_Worker] 스레드 레벨 에러: {e}")
                update_result_text(f"[스레드 에러] {e}")
            else:
                print(f"ℹ️ [PY_THREAD_Worker] 리스너 활성 중 작업({task_name}) 실행 시도.")


# --- ✨ [수정] 0x2A 주기적 데이터 수신 스레드 ---
def uds_listener_thread(conn, data_queue, stop_event):
    """
    [신규] ECU가 0x2A에 의해 주기적으로 보내는 0x62 응답을
    수신 대기(wait_frame)하고 파싱하여 GUI 큐로 보낸다.
    """
    print("✅ [PY_THREAD_Listener] 👂 주기적 데이터 수신 스레드 시작...")
    
    while not stop_event.is_set():
        try:
            payload = conn.wait_frame(timeout=10) 
            
            if payload:
                # 0x62 = ReadDataBy(Periodic)Identifier Positive Response
                if payload[0] == 0x62: 
                    if len(payload) < 3: continue 
                    
                    received_did = int.from_bytes(payload[1:3], 'big')
                    
                    # [수정] data_bytes_all로 모든 데이터를 우선 받음
                    data_bytes_all = payload[3:]
                    display_text = ""
                    
                    if received_did == TOF_SENSOR_DID:
                        # [수정] 3바이트 길이 확인 및 슬라이싱
                        if len(data_bytes_all) >= 3:
                            # ToF 데이터(3바이트)만 정확히 잘라냄
                            tof_data = data_bytes_all[:3] 
                            numeric_value = int.from_bytes(tof_data, 'big')
                            
                            if numeric_value == TOF_INVALID_VALUE:
                                display_text = "유효하지 않음 (Timeout)"
                            else:
                                display_text = f"{numeric_value} mm"
                            data_queue.put({"did": TOF_SENSOR_DID, "display": display_text})
                        
                    elif received_did == ULTRASONIC_SENSOR_DID:
                        # [수정] 2바이트 길이 확인 및 슬라이싱
                        if len(data_bytes_all) >= 2:
                             # 초음파 데이터(2바이트)만 정확히 잘라냄
                            us_data = data_bytes_all[:2]
                            numeric_value = int.from_bytes(us_data, 'big')
                            
                            if numeric_value == US_INVALID_VALUE:
                                display_text = "유효하지 않음"
                            else:
                                distance_cm = numeric_value / 10.0
                                display_text = f"{distance_cm:.1f} cm"
                            data_queue.put({"did": ULTRASONIC_SENSOR_DID, "display": display_text})

        except Exception as e:
            if not stop_event.is_set():
                print(f"❌ [PY_THREAD_Listener] 에러: {e}")
            break
            
    print("👋 [PY_THREAD_Listener] ⏹️ 주기적 데이터 수신 스레드 종료.")


# GUI 결과창을 업데이트하는 함수 (스레드 안전)
def update_result_text(text):
    print(f"[PY_UI_Result] {text}")
    result_text.config(state=tk.NORMAL)
    result_text.delete(1.0, tk.END)
    result_text.insert(tk.END, text)
    result_text.config(state=tk.DISABLED)

# --- 각 버튼에 연결될 '요청 생성' 함수들 ---

# (기존 함수들: 1회성 읽기, 세션 변경, DTC, AEB, OTA 등)
def request_session_change(session_type):
    print(f"[PY_REQ] ➡️  작업 요청: ChangeSession (Type: 0x{session_type:02X})")
    task_queue.put({
        'function': _session_change_handler,
        'args': [session_type]
    })

def request_did_read(did):
    print(f"[PY_REQ] ➡️  작업 요청: Read DID (0x{did:04X})")
    task_queue.put({
        'function': _did_read_handler,
        'args': [did]
    })

def request_all_ultrasonic_data():
    print(f"[PY_REQ] ➡️  작업 요청: Read All Ultrasonic")
    task_queue.put({
        'function': _all_ultrasonic_handler
    })
    
def request_dtc_data():
    print(f"[PY_REQ] ➡️  작업 요청: Read DTC")
    task_queue.put({
        'function': _dtc_read_handler
    })

def request_aeb_write(is_on):
    print(f"[PY_REQ] ➡️  작업 요청: Write AEB (IsOn: {is_on})")
    task_queue.put({
        'function': _aeb_write_handler,
        'args': [is_on]
    })

def request_routine_start(rid):
    print(f"[PY_REQ] ➡️  작업 요청: Start Routine (RID: 0x{rid:04X})")
    task_queue.put({
        'function': _routine_start_handler,
        'args': [rid]
    })

def select_ota_file():
    filepath = filedialog.askopenfilename(
        title="OTA 펌웨어 파일 선택",
        filetypes=[("Hex files", "*.hex")]
    )
    if filepath:
        print(f"[PY_REQ] ➡️  작업 요청: OTA (File: {filepath})")
        update_result_text(f"[*] OTA 파일 선택됨: {filepath}")
        request_ota(filepath)

def request_ota(filepath: str):
    task_queue.put({
        'function': run_ota_process,
        'args': [filepath, update_result_text]
    })

# --- ✨ [신규] 0x2A 주기적 전송 요청 함수 ---
def request_periodic_did(did, subfunction):
    """[신규] 작업 큐에 0x2A (주기적 전송 시작/중지) 요청을 추가합니다."""
    action = "Start" if subfunction == 0x01 else "Stop"
    print(f"[PY_REQ] ➡️  작업 요청: Periodic {action} (DID: 0x{did:04X}, Sub: 0x{subfunction:02X})")
    task_queue.put({
        'function': _periodic_did_handler,
        'args': [did, subfunction]
    })

# --- 실제 통신을 수행하는 핸들러 함수들 (통신 스레드에서 실행됨) ---

# (기존 핸들러들)
def _session_change_handler(client, session_type):
    global current_session_type  # ← 추가

    session_name = "Extended" if session_type == 0x03 else "Default"
    update_result_text(f"[*] {session_name} 세션 요청 전송 중...")
    try:
        response = client.change_session(session_type)
        if response.positive:
            current_session_type = session_type  # ✅ 세션 상태 저장
            update_result_text(f"[+] {session_name} 세션으로 성공적으로 전환되었습니다.")
        else:
            update_result_text(f"[-] 세션 전환 실패: {response.code_name}")
    except Exception as e:
        print(f"❌ [PY_THREAD_Worker] 핸들러 에러 (_session_change_handler): {e}")
        update_result_text(f"[!] 에러: {e}")

def _all_ultrasonic_handler(client):
    update_result_text("[*] 모든 초음파 센서 데이터 요청 중...")
    # ... (기존 v1.2 코드와 동일) ...
    sensors_to_query = [(0x2000, "좌측"), (0x2001, "우측"), (0x2002, "후방")]
    results = []
    has_error = False
    for did, name in sensors_to_query:
        try:
            response = client.read_data_by_identifier(did)
            if response.positive:
                raw_payload = response.original_payload
                if len(raw_payload) > 3:
                    data_bytes = raw_payload[3:]
                    numeric_value = int.from_bytes(data_bytes, 'big', signed=False)
                    distance_cm = numeric_value / 10.0
                    results.append(f"   - {name}: {distance_cm:.1f} cm")
                else:
                    results.append(f"   - {name}: 데이터 길이 오류")
                    has_error = True
            else:
                results.append(f"   - {name}: 응답 오류 ({response.code_name})")
                has_error = True
        except Exception as e:
            results.append(f"   - {name}: 통신 에러 ({e})")
            has_error = True
            break
    final_text = ("[+] 모든 초음파 센서 값:\n" if not has_error else "[!] 일부 센서에서 오류가 발생했습니다:\n") + "\n".join(results)
    update_result_text(final_text)

# pc진단기 코드.txt의 _dtc_read_handler 함수를 아래와 같이 수정하세요.

def _dtc_read_handler(client):
    update_result_text(f"[*] DTC 정보 요청 전송 중...")
    try:
        # 0x19 0x02 0xFF (reportDTCByStatusMask, mask=ALL)
        req = Request(service=services.ReadDTCInformation, subfunction=0x02, data=b'\xFF')
        response = client.send_request(req)
        
        if response.positive:
            if len(response.data) < 3:
                 update_result_text("[+] DTC 정보 수신 (응답 데이터 짧음)")
                 return
            
            dtc_count = response.data[2]
            if dtc_count == 0:
                update_result_text("[+] 고장 코드가 없습니다.")
            else:
                dtc_list = []
                dtc_records_data = response.data[3:]
                record_size = 4 # 3 bytes DTC + 1 byte Status
                
                for i in range(dtc_count):
                    start_index = i * record_size
                    record = dtc_records_data[start_index : start_index + record_size]
                    if len(record) == record_size:
                        dtc_id = int.from_bytes(record[0:3], 'big')
                        dtc_status = record[3]
                        dtc_list.append(f"   - ID: 0x{dtc_id:06X}, 상태: 0x{dtc_status:02X}")
                        
                update_result_text(f"[+] 감지된 고장 코드 ({dtc_count}개):\n" + "\n".join(dtc_list))

        else:
            update_result_text(f"[-] ECU 부정 응답: {response.code_name}")
    except Exception as e:
        print(f"❌ [PY_THREAD_Worker] 핸들러 에러 (_dtc_read_handler): {e}")
        update_result_text(f"[!] 에러: {e}")

def _aeb_write_handler(client, is_on):
    # ... (기존 v1.2 코드와 동일) ...
    global current_session_type
    if current_session_type == 0x01:
        update_result_text("⚠️ Default Session에서는 실행될 수 없습니다.")
        return
    
    did_to_write = 0x3000
    data_to_write = 0x01 if is_on else 0x00
    state_text = "ON" if is_on else "OFF"
    update_result_text(f"[*] AEB 기능 {state_text} 요청 전송 중 (DID: 0x{did_to_write:04X})...")
    try:
        response = client.write_data_by_identifier(did=did_to_write, value=data_to_write)
        if response.positive:
            update_result_text(f"[+] AEB 기능이 성공적으로 {state_text} 되었습니다.")
        else:
            update_result_text(f"[-] AEB 기능 설정 실패: {response.code_name}")
    except Exception as e:
        update_result_text(f"[!] 에러 발생: {e}")

def _did_read_handler(client, did):
    # ... (기존 v1.2 코드와 동일, 파싱 로직 개선) ...
    description, codec = DID_DESCRIPTIONS.get(did, ("알 수 없는 DID", None))
    update_result_text(f"[*] DID 0x{did:04X} ({description}) 데이터 요청 중...")
    try:
        response = client.read_data_by_identifier(did)
        if response.positive:
            display_value = ""
            raw_payload = response.original_payload
            if len(raw_payload) > 3:
                data_bytes = raw_payload[3:]
                if did == TOF_SENSOR_DID:
                    numeric_value = int.from_bytes(data_bytes, 'big')
                    if numeric_value == TOF_INVALID_VALUE:
                        display_value = "유효하지 않음 (Timeout)"
                    else:
                        display_value = f"{numeric_value} mm"
                elif did in [ULTRASONIC_SENSOR_DID, ULTRASONIC_RIGHT_SENSOR_DID, ULTRASONIC_REAR_SENSOR_DID]:
                    numeric_value = int.from_bytes(data_bytes, 'big')
                    if numeric_value == US_INVALID_VALUE:
                        display_value = "유효하지 않음"
                    else:
                        distance_cm = numeric_value / 10.0
                        display_value = f"{distance_cm:.1f} cm"
                elif did == 0xF1A0:
                    display_value = f"\n[파싱된 목록]\n{parse_supported_dids(data_bytes)}"
                else:
                    try:
                        display_value = data_bytes.decode('utf-8', errors='ignore').strip('\x00')
                    except UnicodeDecodeError:
                        display_value = f"Hex: {data_bytes.hex()}"
            else:
                display_value = "수신된 데이터가 너무 짧습니다."
            update_result_text(f"[+] {description} (0x{did:04X}):\n   - {display_value}")
        else:
            update_result_text(f"[-] DID 0x{did:04X} 읽기 실패: {response.code_name}")
    except Exception as e:
        update_result_text(f"[!] 에러: {e}")

def _routine_start_handler(client, rid):
    # ... (기존 v1.2 코드와 동일) ...
    global current_session_type  # ← 추가
    if current_session_type == 0x01:
        update_result_text("⚠️ Default Session에서는 실행될 수 없습니다.")
        return
    
    if rid == 0x0001: routine_name = "모터 정회전"
    elif rid == 0x0002: routine_name = "모터 역회전"
    else: routine_name = f"알 수 없는 루틴 (ID: 0x{rid:04X})"
    update_result_text(f"[*] '{routine_name}' 테스트 루틴 시작 요청 중...")
    try:
        response = client.routine_control(rid, 1)
        if response.positive:
            update_result_text(f"[+] '{routine_name}' 테스트가 성공적으로 시작되었습니다.")
        else:
            update_result_text(f"[-] 루틴 시작 실패: {response.code_name}")
    except Exception as e:
        update_result_text(f"[!] 에러 발생: {e}")

# --- ✨ [신규] 0x2A 주기적 전송 요청 핸들러 ---
def _periodic_did_handler(client, did, subfunction):
    """[신규] 0x2A (ReadDataByPeriodicIdentifier) 서비스를 요청합니다."""
    action_str = "시작" if subfunction == 0x01 else "중지"
    update_result_text(f"[*] DID 0x{did:04X} 주기적 전송 {action_str} 요청 중...")
    
    try:
        # UDS 0x2A 요청 페이로드: [DID_High, DID_Low] (Subfunction은 data가 아님)
        did_as_bytes = struct.pack('>H', did) # 예: 0x1000 -> b'\x10\x00'
        
        req = Request(
            service=services.ReadDataByPeriodicIdentifier, # 0x2A
            subfunction=subfunction, # 0x01 (start) 또는 0x02 (stop)
            data=did_as_bytes
        )
        
        response = client.send_request(req)

        if response.positive:
            update_result_text(f"[+] DID 0x{did:04X} 주기적 전송 {action_str} 성공.")
        else:
            update_result_text(f"[-] DID 0x{did:04X} {action_str} 실패: {response.code_name}")

    except Exception as e:
        print(f"❌ [PY_THREAD_Worker] 핸들러 에러 (_periodic_did_handler): {e}")
        update_result_text(f"[!] 에러: {e}")
        

def parse_supported_dids(did_bytes):
    # ... (기존 v1.2 코드와 동일) ...
    did_list = []
    for i in range(0, len(did_bytes), 2):
        did_chunk = did_bytes[i:i+2]
        if len(did_chunk) == 2:
            did_value = int.from_bytes(did_chunk, 'big')
            description, _ = DID_DESCRIPTIONS.get(did_value, ("알 수 없는 DID", None))
            did_list.append(f"   - 0x{did_value:04X}: {description}")
    return "\n".join(did_list)


# --- ✨ [신규] GUI 헬퍼 함수 (버튼 제어) ---

def set_all_buttons_state(state):
    """[신규] g_all_buttons 목록의 모든 버튼 상태를 변경합니다."""
    for btn in g_all_buttons:
        if btn: # 버튼이 None이 아닌지 확인
            btn.config(state=state)

def flush_connection(conn):
    """[신규] conn 버퍼에 남아있을 수 있는 0x62 응답을 비웁니다."""
    print("...수신 버퍼를 정리하는 중...")
    try:
        while conn.wait_frame(timeout=10) is not None:
            pass
    except Exception:
        pass # 타임아웃 예외는 정상
    print("...버퍼 정리 완료.")


# --- ✨ [수정] 실시간 폴링 스레드 제어 함수 (개별) ---

def toggle_listener_mode(conn):
    """
    [신규] '실시간 수신 모드' 마스터 토글.
    리스너 스레드를 시작/중지하고 다른 모든 버튼을 비활성화/활성화합니다.
    """
    global is_listener_active, g_listener_thread
    global is_laser_periodic_on, is_us_left_periodic_on
    
    is_listener_active = not is_listener_active
    
    if is_listener_active:
        # --- 리스너 모드 시작 ---
        print("[PY_UI] ▶️ 실시간 수신 모드 시작. 다른 버튼을 비활성화합니다.")
        stop_listener_event.clear()
        
        # [중요] g_uds_client가 사용하는 conn 객체를 리스너 스레드에 넘김
        g_listener_thread = threading.Thread(
            target=uds_listener_thread, 
            args=(conn, periodic_data_queue, stop_listener_event), 
            daemon=True
        )
        g_listener_thread.start()
        
        set_all_buttons_state(tk.DISABLED) # 다른 모든 버튼 비활성화
        
        # 이 버튼들만 활성화
        listener_toggle_button.config(text="실시간 수신 모드 중지 ⏹️", relief=tk.SUNKEN)
        laser_poll_toggle_button.config(state=tk.NORMAL)
        us_left_poll_toggle_button.config(state=tk.NORMAL)
        update_result_text("[*] 실시간 수신 모드가 시작되었습니다.\n"
                           "이제 개별 센서의 주기적 전송을 시작할 수 있습니다.")
        
    else:
        # --- 리스너 모드 중지 ---
        print("[PY_UI] ⏹️ 실시간 수신 모드 중지. 모든 버튼을 활성화합니다.")
        
        # 1. (혹시 모르니) 모든 주기적 전송 중지 요청
        if is_laser_periodic_on:
            request_periodic_did(TOF_SENSOR_DID, 0x02) # Stop
            is_laser_periodic_on = False
        if is_us_left_periodic_on:
            request_periodic_did(ULTRASONIC_SENSOR_DID, 0x02) # Stop
            is_us_left_periodic_on = False
            
        time.sleep(0.1) # 중지 요청이 전송될 시간
            
        # 2. 리스너 스레드 중지
        stop_listener_event.set()
        if g_listener_thread and g_listener_thread.is_alive():
            g_listener_thread.join(timeout=10)
            
        # 3. [중요] 리스너가 점유했던 conn 버퍼 플러시
        flush_connection(conn)
            
        # 4. 모든 버튼 원래대로 복구
        set_all_buttons_state(tk.NORMAL)
        
        listener_toggle_button.config(text="실시간 수신 모드 시작 ▶️", relief=tk.RAISED)
        # 주기적 토글 버튼들 비활성화 및 리셋
        laser_poll_toggle_button.config(state=tk.DISABLED, text="레이저 주기적 시작 ▶️", relief=tk.RAISED)
        us_left_poll_toggle_button.config(state=tk.DISABLED, text="초음파(좌) 주기적 시작 ▶️", relief=tk.RAISED)
        rt_laser_label.config(text="레이저: --- mm")
        rt_us_l_label.config(text="초음파(좌): --- cm")
        
        update_result_text("[*] 실시간 수신 모드가 중지되었습니다.")
        

def toggle_laser_periodic():
    """[수정] '레이저 주기적' 버튼 토글. 스레드가 아닌 0x2A 요청을 전송."""
    global is_laser_periodic_on
    is_laser_periodic_on = not is_laser_periodic_on
    
    if is_laser_periodic_on:
        request_periodic_did(TOF_SENSOR_DID, 0x01) # Start
        laser_poll_toggle_button.config(text="레이저 주기적 중지 ⏹️", relief=tk.SUNKEN)
    else:
        request_periodic_did(TOF_SENSOR_DID, 0x02) # Stop
        laser_poll_toggle_button.config(text="레이저 주기적 시작 ▶️", relief=tk.RAISED)
        rt_laser_label.config(text="레이저: --- mm") # 중지 시 리셋

def toggle_us_left_periodic():
    """[수정] '초음파(좌) 주기적' 버튼 토글. 스레드가 아닌 0x2A 요청을 전송."""
    global is_us_left_periodic_on
    is_us_left_periodic_on = not is_us_left_periodic_on
    
    if is_us_left_periodic_on:
        request_periodic_did(ULTRASONIC_SENSOR_DID, 0x01) # Start
        us_left_poll_toggle_button.config(text="초음파(좌) 주기적 중지 ⏹️", relief=tk.SUNKEN)
    else:
        request_periodic_did(ULTRASONIC_SENSOR_DID, 0x02) # Stop
        us_left_poll_toggle_button.config(text="초음파(좌) 주기적 시작 ▶️", relief=tk.RAISED)
        rt_us_l_label.config(text="초음파(좌): --- cm") # 중지 시 리셋
        

# --- ✨ [수정] 주기적 데이터 큐 처리 함수 (GUI 스레드) ---
def process_periodic_data_queue():
    """
    [수정] 100ms마다 GUI 스레드에서 실행되며,
    'uds_listener_thread'가 보내온 데이터를 라벨에 업데이트합니다.
    """
    try:
        while not periodic_data_queue.empty():
            item = periodic_data_queue.get_nowait()
            did = item.get("did")
            display = item.get("display", "N/A")
            
            if did == TOF_SENSOR_DID:
                rt_laser_label.config(text=f"레이저: {display}")
            elif did == ULTRASONIC_SENSOR_DID:
                rt_us_l_label.config(text=f"초음파(좌): {display}")
            
    except queue.Empty:
        pass
    finally:
        window.after(100, process_periodic_data_queue)


# --- GUI 생성 ---
window = tk.Tk()
window.title("ECU 진단 툴 (v1.3)")
window.geometry("500x800")
window.configure(bg="#f0f0f0")

default_font = tkFont.nametofont("TkDefaultFont")
default_font.configure(family="맑은 고딕", size=9)

#session_status_label = tk.Label(window, text="현재 세션: 확인 중...", font=("맑은 고딕", 9, "italic"), bg="#f0f0f0", fg="blue")
#session_status_label.pack(pady=(5, 0))

# --- ✨ [수정] 1. 실시간 센서 데이터 그룹 ---
rt_frame = tk.LabelFrame(window, text=" 실시간 센서 데이터 (0x2A 주기적 수신) ", padx=10, pady=5, bg="#e0e0ff")
rt_frame.pack(pady=5, padx=10, fill="x")

# ✨ [신규] 마스터 토글 버튼
listener_toggle_button = tk.Button(rt_frame, text="실시간 수신 모드 시작 ▶️", font=("맑은 고딕", 9, "bold"))
listener_toggle_button.grid(row=0, column=0, columnspan=2, padx=5, pady=5, sticky="ew")

# ✨ [수정] 개별 0x2A 시작/중지 토글 버튼 (기본 비활성화)
laser_poll_toggle_button = tk.Button(rt_frame, text="레이저 주기적 시작 ▶️", command=toggle_laser_periodic, state=tk.DISABLED)
laser_poll_toggle_button.grid(row=1, column=0, padx=5, pady=5, sticky="ew")

us_left_poll_toggle_button = tk.Button(rt_frame, text="초음파(좌) 주기적 시작 ▶️", command=toggle_us_left_periodic, state=tk.DISABLED)
us_left_poll_toggle_button.grid(row=1, column=1, padx=5, pady=5, sticky="ew")

# 실시간 데이터 표시 라벨
rt_label_font = ("Consolas", 12)
rt_laser_label = tk.Label(rt_frame, text="레이저: --- mm", font=rt_label_font, bg="#e0e0ff", anchor="w")
rt_laser_label.grid(row=2, column=0, columnspan=2, padx=5, pady=2, sticky="ew")

rt_us_l_label = tk.Label(rt_frame, text="초음파(좌): --- cm", font=rt_label_font, bg="#e0e0ff", anchor="w")
rt_us_l_label.grid(row=3, column=0, columnspan=2, padx=5, pady=2, sticky="ew")

rt_frame.grid_columnconfigure(0, weight=1)
rt_frame.grid_columnconfigure(1, weight=1)


# --- 2. (1회성) 센서 데이터 그룹 ---
sensor_frame = tk.LabelFrame(window, text=" 1회성 센서 데이터 읽기 (0x22) ", padx=10, pady=5, bg="#f0f0f0")
sensor_frame.pack(pady=5, padx=10, fill="x")

laser_button = tk.Button(sensor_frame, text="레이저 (1회)", command=lambda: request_did_read(TOF_SENSOR_DID))
laser_button.grid(row=0, column=0, padx=5, pady=5, sticky="ew")
left_us_button = tk.Button(sensor_frame, text="초음파(좌) (1회)", command=lambda: request_did_read(ULTRASONIC_SENSOR_DID))
left_us_button.grid(row=0, column=1, padx=5, pady=5, sticky="ew")
right_us_button = tk.Button(sensor_frame, text="초음파(우) (1회)", command=lambda: request_did_read(ULTRASONIC_RIGHT_SENSOR_DID))
right_us_button.grid(row=0, column=2, padx=5, pady=5, sticky="ew")
rear_us_button = tk.Button(sensor_frame, text="초음파(후) (1회)", command=lambda: request_did_read(ULTRASONIC_REAR_SENSOR_DID))
rear_us_button.grid(row=0, column=3, padx=5, pady=5, sticky="ew")
all_us_button = tk.Button(sensor_frame, text="초음파(모두) (1회)", command=request_all_ultrasonic_data, font=("맑은 고딕", 9, "bold"))
all_us_button.grid(row=0, column=4, padx=5, pady=5, sticky="ew")

for i in range(5): sensor_frame.grid_columnconfigure(i, weight=1)
g_all_buttons.extend([laser_button, left_us_button, right_us_button, rear_us_button, all_us_button])

# --- 3. ECU 정보 그룹 ---
info_frame = tk.LabelFrame(window, text=" ECU 정보 ", padx=10, pady=5, bg="#f0f0f0")
info_frame.pack(pady=5, padx=10, fill="x")
part_number_button = tk.Button(info_frame, text="부품 번호", command=lambda: request_did_read(0xF187))
part_number_button.grid(row=0, column=0, padx=5, pady=5, sticky="ew")
serial_number_button = tk.Button(info_frame, text="시리얼 번호", command=lambda: request_did_read(0xF18C))
serial_number_button.grid(row=0, column=1, padx=5, pady=5, sticky="ew")
vin_button = tk.Button(info_frame, text="차대번호(VIN)", command=lambda: request_did_read(0xF190))
vin_button.grid(row=0, column=2, padx=5, pady=5, sticky="ew")
mfg_date_button = tk.Button(info_frame, text="제조 날짜", command=lambda: request_did_read(0xF193))
mfg_date_button.grid(row=1, column=0, padx=5, pady=5, sticky="ew")
supplier_button = tk.Button(info_frame, text="공급업체", command=lambda: request_did_read(0xF192))
supplier_button.grid(row=1, column=1, padx=5, pady=5, sticky="ew")
supported_dids_button = tk.Button(info_frame, text="지원 DID 목록", command=lambda: request_did_read(0xF1A0))
supported_dids_button.grid(row=1, column=2, padx=5, pady=5, sticky="ew")
for i in range(3): info_frame.grid_columnconfigure(i, weight=1)
g_all_buttons.extend([part_number_button, serial_number_button, vin_button, mfg_date_button, supplier_button, supported_dids_button])

# --- 4. 진단 및 제어 그룹 ---
control_frame = tk.LabelFrame(window, text=" 진단 및 제어 ", padx=10, pady=5, bg="#f0f0f0")
control_frame.pack(pady=5, padx=10, fill="x")
dtc_button = tk.Button(control_frame, text="DTC 정보 읽기", command=request_dtc_data)
dtc_button.grid(row=0, column=0, columnspan=2, padx=5, pady=5, sticky="ew")
aeb_on_button = tk.Button(control_frame, text="AEB 기능 ON", command=lambda: request_aeb_write(True))
aeb_on_button.grid(row=0, column=2, padx=5, pady=5, sticky="ew")
aeb_off_button = tk.Button(control_frame, text="AEB 기능 OFF", command=lambda: request_aeb_write(False))
aeb_off_button.grid(row=0, column=3, padx=5, pady=5, sticky="ew")
motor_forward_button = tk.Button(control_frame, text="모터 정회전 테스트", command=lambda: request_routine_start(0x0001))
motor_forward_button.grid(row=2, column=0, columnspan=2, padx=5, pady=5, sticky="ew")
motor_reverse_button = tk.Button(control_frame, text="모터 역회전 테스트", command=lambda: request_routine_start(0x0002))
motor_reverse_button.grid(row=2, column=2, columnspan=2, padx=5, pady=5, sticky="ew")
session_extended_button = tk.Button(control_frame, text="세션 시작 (Ext)", command=lambda: request_session_change(0x03))
session_extended_button.grid(row=1, column=0, columnspan=2, padx=5, pady=5, sticky="ew")
session_default_button = tk.Button(control_frame, text="세션 종료 (Def)", command=lambda: request_session_change(0x01))
session_default_button.grid(row=1, column=2, columnspan=2, padx=5, pady=5, sticky="ew")
for i in range(4): control_frame.grid_columnconfigure(i, weight=1)
g_all_buttons.extend([dtc_button, aeb_on_button, aeb_off_button, motor_forward_button, motor_reverse_button, session_extended_button, session_default_button])

# --- 5. OTA 그룹 ---
ota_frame = tk.LabelFrame(window, text=" OTA 업데이트 ", padx=10, pady=5, bg="#f0f0f0")
ota_frame.pack(pady=5, padx=10, fill="x")
ota_button = tk.Button(ota_frame, text="펌웨어 파일 선택 (OTA)", command=select_ota_file)
ota_button.pack(fill="x", padx=5, pady=5)
g_all_buttons.append(ota_button)


# --- 결과 텍스트 창 ---
result_text = scrolledtext.ScrolledText(window, height=8, font=("Consolas", 14))
result_text.pack(pady=10, padx=10, fill="both", expand=True)
result_text.insert(tk.END, "ECU에 연결 중...")
result_text.config(state=tk.DISABLED)

# --- 메인 실행 로직 ---
if __name__ == "__main__":
    print("[PY_MAIN] 🏁 프로그램 시작")
    doip_client = None
    comm_thread = None
    g_uds_client = None
    
    try:
        # 1. DoIP 클라이언트 생성
        update_result_text(f"[*] DoIP 서버 [{RPI_HOST}]에 연결 시도 중...")
        print(f"[PY_MAIN] 🔌 DoIP 서버 [{RPI_HOST}] 연결 시도...")
        
        doip_client = DoIPClient(RPI_HOST, ECU_LOGICAL_ADDRESS, auto_reconnect_tcp=True)
        # [중요] conn 객체를 UDS Client와 리스너 스레드가 공유해야 함
        conn = DoIPClientUDSConnector(doip_client)
        
        update_result_text("🤝 DoIP 소켓 연결 성공!")
        print("[PY_MAIN] 🤝 DoIP 소켓 연결 성공!")

        # 2. UDSonCAN 클라이언트를 'with' 구문으로 감싸서 생성
        with Client(conn, config=uds_config) as g_uds_client:
            print("[PY_MAIN] 🚗 UDS Client 생성 완료. 스레드에 공유합니다.")

            # 3. GUI 버튼 워커 스레드 시작
            comm_thread = threading.Thread(
                target=communication_thread, 
                args=(g_uds_client, window), 
                daemon=True
            )
            comm_thread.start()
            
            # 4. ✨ [신규] 리스너 모드 토글 버튼에 conn 객체를 인자로 연결
            listener_toggle_button.config(command=partial(toggle_listener_mode, conn))

            # 5. GUI 큐 폴링 시작
            window.after(100, process_periodic_data_queue)

            # 6. GUI 메인 루프 시작
            window.mainloop()

    except Exception as e:
        print(f"❌ [PY_MAIN] 💥 메인 스레드 심각한 오류: {e}")
        update_result_text(f"\n❌ 클라이언트 실행 중 심각한 오류 발생: {e}")
        time.sleep(5)
        
    finally:
        # 7. 프로그램 종료 시 자원 정리
        print("👋 [PY_MAIN] 프로그램을 종료합니다.")
        
        # 모든 스레드에 종료 신호 전송
        stop_event.set() 
        stop_listener_event.set() 
        
        if comm_thread:
            comm_thread.join(timeout=10)
            
        if g_listener_thread and g_listener_thread.is_alive():
            g_listener_thread.join(timeout=10)
                
        if doip_client:
            doip_client.close()
            print("🔌 [PY_MAIN] DoIP 연결이 종료되었습니다.")