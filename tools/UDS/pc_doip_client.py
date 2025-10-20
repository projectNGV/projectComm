# pc_doip_client.py 파일 상단
import sys
import os

# 현재 파일의 경로를 기준으로 상위 폴더(tools)의 경로를 시스템 경로에 추가
current_dir = os.path.dirname(os.path.abspath(__file__))
parent_dir = os.path.dirname(current_dir) # tools 폴더
sys.path.append(parent_dir)

import tkinter as tk
from tkinter import scrolledtext, font as tkFont, filedialog
import threading
import time
import queue

# UDSonCAN 및 DoIPClient 라이브러리 임포트
from doipclient import DoIPClient
from doipclient.connectors import DoIPClientUDSConnector
from udsoncan.client import Client
from udsoncan.exceptions import *
from udsoncan.configs import default_client_config
from udsoncan import DidCodec, services, Request

from typing import Any, Tuple, Dict, Optional


from ota.ota_main import run_ota_process

# --- 설정값 ---
RPI_HOST = '192.168.137.10' # 실제 라즈베리파이 IP
ECU_LOGICAL_ADDRESS = 0x1000 # ECU 논리 주소 (필요시 수정)



# --- DID 및 Codec 정의 ---
# 각 DID에 대한 설명과 데이터 형식을 정의합니다.
DID_DESCRIPTIONS: Dict[int, Tuple[str, Optional[DidCodec]]] = {
    0x1000: ("레이저 센서 거리 (mm)", DidCodec('3s')),
    
    0x2000: ("초음파(좌) 센서 거리 (x0.1 cm)", DidCodec('>H')),
    0x2001: ("초음파(우) 센서 거리 (x0.1 cm)", DidCodec('>H')),
    0x2002: ("초음파(후) 센서 거리 (x0.1 cm)", DidCodec('>H')),
    0x3000: ("AEB 기능 플래그", DidCodec('B')),             # 1-byte Unsigned Int (Write)
    0xF186: ("현재 세션 정보", DidCodec('B')),

    0xF187: ("ECU 부품 번호", DidCodec('20s')),
    0xF18C: ("ECU 시리얼 번호", DidCodec('20s')),
    0xF190: ("차대번호 (VIN)", DidCodec('18s')),
    0xF192: ("ECU 공급업체 정보", DidCodec('20s')),
    0xF193: ("ECU 제조 날짜", DidCodec('11s')),

    0xF1A0: ("지원 DID 목록", DidCodec('22s'))        # Binary data to be parsed manually
}

# UDSonCAN 클라이언트 설정
uds_config = {
    'data_identifiers': {did: codec for did, (_, codec) in DID_DESCRIPTIONS.items() if codec is not None},
    'p2_timeout': 10,
    'p2_star_timeout': 15,
}

# --- 전역 변수 ---
# GUI 스레드와 통신 스레드 간의 안전한 데이터 교환을 위해 Queue를 사용합니다.
task_queue = queue.Queue()
# 백그라운드 스레드 제어를 위한 이벤트
stop_event = threading.Event()

def print_hex(prefix: str, data: bytes):
    """바이트 데이터를 보기 쉬운 16진수 문자열로 콘솔에 출력합니다."""
    if not isinstance(data, bytes):
        print(f"{prefix} [ Invalid data type: {type(data)} ]")
        return
    hex_str = ' '.join([f'0x{byte:02X}' for byte in data])
    print(f"{prefix} [ {hex_str} ]")

# --- 통신 처리 백그라운드 스레드 ---
def communication_thread(doip_client, conn, root_window):
    """
    UDSonCAN 클라이언트를 관리하고, GUI의 요청을 받아 처리하는 스레드
    """
    with Client(conn, config=uds_config, request_timeout=15) as client:
        print("✅ UDS 클라이언트 시작. GUI 요청 대기 중...")
        while not stop_event.is_set():
            try:
                # task_queue에서 작업(요청)을 기다림 (최대 0.1초)
                task = task_queue.get(timeout=0.1)
                
                func = task['function']
                args = task.get('args', [])
                kwargs = task.get('kwargs', {})
                
                # 실제 통신 함수 실행
                func(client, *args, **kwargs)

            except queue.Empty:
                # 큐가 비어있으면 그냥 계속 대기
                continue
            except Exception as e:
                update_result_text(f"[스레드 에러] {e}")

# GUI 결과창을 업데이트하는 함수 (스레드 안전)
def update_result_text(text):
    result_text.config(state=tk.NORMAL)
    result_text.delete(1.0, tk.END)
    result_text.insert(tk.END, text)
    result_text.config(state=tk.DISABLED)

# --- 각 버튼에 연결될 '요청 생성' 함수들 ---
# 이 함수들은 실제 통신을 하지 않고, 통신 스레드가 처리할 '작업'을 큐에 넣기만 합니다.

def request_session_change(session_type):
    task_queue.put({
        'function': _session_change_handler,
        'args': [session_type]
    })

def request_did_read(did):
    task_queue.put({
        'function': _did_read_handler,
        'args': [did]
    })

def request_all_ultrasonic_data():
    task_queue.put({
        'function': _all_ultrasonic_handler
    })
    
def request_dtc_data():
    task_queue.put({
        'function': _dtc_read_handler
    })

def request_aeb_write(is_on):
    task_queue.put({
        'function': _aeb_write_handler,
        'args': [is_on]
    })

def request_routine_start(rid):
    task_queue.put({
        'function': _routine_start_handler,
        'args': [rid]
    })


# ✨ OTA 파일 선택 및 요청 함수 추가
def select_ota_file():
    filepath = filedialog.askopenfilename(
        title="OTA 펌웨어 파일 선택",
        filetypes=[("Hex files", "*.hex")]
    )
    if filepath:
        update_result_text(f"[*] OTA 파일 선택됨: {filepath}")
        request_ota(filepath)

def request_ota(filepath: str):
    task_queue.put({
        'function': run_ota_process,
        'args': [filepath, update_result_text]
    })

# --- 실제 통신을 수행하는 핸들러 함수들 (통신 스레드에서 실행됨) ---

def _session_change_handler(client, session_type):
    session_name = "Extended" if session_type == 0x03 else "Default"
    update_result_text(f"[*] {session_name} 세션 요청 전송 중...")
    try:
        response = client.change_session(session_type)
        print_hex("ECU -> PC (Raw Response)", response.original_payload)
        if response.positive:
            update_result_text(f"[+] {session_name} 세션으로 성공적으로 전환되었습니다.")
        else:
            update_result_text(f"[-] 세션 전환 실패: {response.code_name}")
    except Exception as e:
        update_result_text(f"[!] 에러: {e}")



def _all_ultrasonic_handler(client):
    update_result_text("[*] 모든 초음파 센서 데이터 요청 중...")
    sensors_to_query = [(0x2000, "좌측"), (0x2001, "우측"), (0x2002, "후방")]
    results = []
    has_error = False
    for did, name in sensors_to_query:
        try:
            response = client.read_data_by_identifier(did)
            print_hex("ECU -> PC (Raw Response)", response.original_payload)
            if response.positive:
                distance_cm = response.data[did] / 10.0
                results.append(f"  - {name}: {distance_cm:.1f} cm")
            else:
                results.append(f"  - {name}: 응답 오류 ({response.code_name})")
                has_error = True
        except Exception as e:
            results.append(f"  - {name}: 통신 에러 ({e})")
            has_error = True
            break
    final_text = ("[+] 모든 초음파 센서 값:\n" if not has_error else "[!] 일부 센서에서 오류가 발생했습니다:\n") + "\n".join(results)
    update_result_text(final_text)

# pc진단기 코드.txt의 _dtc_read_handler 함수를 아래와 같이 수정하세요.

def _dtc_read_handler(client):
    update_result_text(f"[*] DTC 정보 요청 전송 중...")
    try:
        # reportDTCByStatusMask (0x02) / statusMask=0xFF (모든 상태)
        response = client.read_dtc_information(subfunction=0x02, status_mask=0xFF) 
        print_hex("ECU -> PC (Raw Response)", response.original_payload)
        if response.positive:
            if not response.service_data.dtcs:
                update_result_text("[+] 고장 코드가 없습니다.")
            else:
                # ▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼ 이 부분을 수정합니다 ▼▼▼▼▼▼▼▼▼▼▼▼▼▼▼
                # dtc.status 객체에서 .mask 속성 접근을 제거합니다.
                dtc_list = [f"  - ID: 0x{dtc.id:06X}, 상태: 0x{dtc.status:02X}" for dtc in response.service_data.dtcs]
                # ▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲▲

                update_result_text("[+] 감지된 고장 코드:\n" + "\n".join(dtc_list))
        else:
            update_result_text(f"[-] ECU 부정 응답: {response.code_name}")
    except Exception as e:
        update_result_text(f"[!] 에러: {e}")


# 기존 _aeb_write_handler 함수를 아래 코드로 완전히 교체하세요.

def _aeb_write_handler(client, is_on): # 1. is_on 매개변수를 추가하여 값을 전달받습니다.
    """
    AEB 기능(DID 0x3000)을 켜거나 끄는 요청을 처리하는 핸들러 함수.
    UDS Service 0x2E (WriteDataByIdentifier)를 사용합니다.
    """
    # 2. 코드 상단의 DID_DESCRIPTIONS에 정의된 값(0x3000)과 일치시킵니다.
    did_to_write = 0x3000
    
    # 3. 변수명 오타를 수정합니다. (data_to_wirte -> data_to_write)
    data_to_write = 0x01 if is_on else 0x00
    state_text = "ON" if is_on else "OFF"

    update_result_text(f"[*] AEB 기능 {state_text} 요청 전송 중 (DID: 0x{did_to_write:04X})...")
    
    try:
        # udsoncan 클라이언트의 write_data_by_identifier 함수를 호출하여 실제 요청을 보냅니다.
        response = client.write_data_by_identifier(did=did_to_write, value=data_to_write)
        print_hex("ECU -> PC (Raw Response)", response.original_payload)

        # ECU의 응답을 확인하고 결과를 GUI에 표시합니다.
        if response.positive:
            update_result_text(f"[+] AEB 기능이 성공적으로 {state_text} 되었습니다.")
        else:
            # ECU가 요청을 거부한 경우 (예: 조건 불충분)
            update_result_text(f"[-] AEB 기능 설정 실패: {response.code_name}")

    except Exception as e:
        # 통신 오류 등 예외가 발생한 경우
        update_result_text(f"[!] 에러 발생: {e}")

# pc진단기 코드.txt 파일에서 _did_read_handler 함수를 아래 내용으로 통째로 교체하세요.
# 기존 _did_read_handler 함수를 아래 코드로 잠시 교체해서 테스트해보세요.

# 기존 _did_read_handler 함수를 아래의 '수동 파싱 최종 버전'으로 교체하세요.

def _did_read_handler(client, did):
    """
    단일 DID 값을 읽어 GUI에 표시하는 핸들러 함수 (수동 파싱 최종 버전)
    """
    description, codec = DID_DESCRIPTIONS.get(did, ("알 수 없는 DID", None))
    update_result_text(f"[*] DID 0x{did:04X} ({description}) 데이터 요청 중...")
    
    try:
        response = client.read_data_by_identifier(did)
        print_hex("ECU -> PC (Raw Response)", response.original_payload)

        if response.positive:
            display_value = ""
            
            # ▼▼▼▼▼▼▼▼▼▼▼▼▼▼ 라이브러리 내부 문제를 우회하는 수동 파싱 로직 ▼▼▼▼▼▼▼▼▼▼▼▼▼▼
            
            # 1. 수신된 원본 바이트 페이로드를 가져옵니다.
            raw_payload = response.original_payload
            
            # 2. UDS 응답 [SID(1), DID(2), 데이터(N)] 에서 데이터 부분만 잘라냅니다.
            #    데이터는 4번째 바이트(인덱스 3)부터 시작합니다.
            if len(raw_payload) > 3:
                data_bytes = raw_payload[3:]
                
                # 3. DID에 따라 데이터를 올바르게 해석합니다.
                if did == 0x1000: # 레이저 센서 (3바이트 정수)
                    # Big-Endian 형식의 3바이트를 부호 없는 정수로 변환합니다.
                    numeric_value = int.from_bytes(data_bytes, 'big', signed=False)
                    display_value = f"{numeric_value} mm"
                
                elif did in [0x2000, 0x2001, 0x2002]: # 초음파 센서 (2바이트 정수)
                    numeric_value = int.from_bytes(data_bytes, 'big', signed=False)
                    distance_cm = numeric_value / 10.0
                    display_value = f"{distance_cm:.1f} cm"
                
                elif did == 0xF1A0: # 지원 DID 목록
                     display_value = f"\n[파싱된 목록]\n{parse_supported_dids(data_bytes)}"
                
                else: # 부품 번호, 시리얼 등 (문자열)
                    try:
                        display_value = data_bytes.decode('utf-8')
                    except UnicodeDecodeError:
                        display_value = f"Hex: {data_bytes.hex()}"
            else:
                display_value = "수신된 데이터가 너무 짧습니다."
            # ▲▲▲▲▲▲▲▲▲▲▲▲▲▲ 수동 파싱 로직 끝 ▲▲▲▲▲▲▲▲▲▲▲▲▲▲

            update_result_text(f"[+] {description} (0x{did:04X}):\n  - {display_value}")

        else:
            update_result_text(f"[-] DID 0x{did:04X} 읽기 실패: {response.code_name}")
            
    except Exception as e:
        update_result_text(f"[!] 에러: {e}")


def _routine_start_handler(client, rid):
    """
    ECU의 특정 루틴(RID)을 시작하도록 요청하는 핸들러 함수.
    UDS Service 0x31 (RoutineControl)의 subfunction 0x01 (startRoutine)을 사용합니다.
    """
    # 1. 전달받은 루틴 ID(rid)에 따라 GUI에 표시할 테스트 이름을 결정합니다.
    if rid == 0x0001:
        routine_name = "모터 정회전"
    elif rid == 0x0002:
        routine_name = "모터 역회전"
    else:
        routine_name = f"알 수 없는 루틴 (ID: 0x{rid:04X})"

    update_result_text(f"[*] '{routine_name}' 테스트 루틴 시작 요청 중...")

    try:
        # 2. udsoncan 클라이언트의 routine_control 함수를 호출하여 실제 요청을 보냅니다.
        #    - subfunction=1은 'startRoutine'을 의미합니다.
        response = client.routine_control(rid, 1)
        print_hex("ECU -> PC (Raw Response)", response.original_payload)

        # 3. ECU의 응답을 확인하고 결과를 GUI에 표시합니다.
        if response.positive:
            update_result_text(f"[+] '{routine_name}' 테스트가 성공적으로 시작되었습니다.")
        else:
            # ECU가 요청을 거부한 경우 (예: 조건 불충분, 보안 액세스 필요 등)
            update_result_text(f"[-] 루틴 시작 실패: {response.code_name}")

    except Exception as e:
        # 통신 오류 등 예외가 발생한 경우
        update_result_text(f"[!] 에러 발생: {e}")
        
def parse_supported_dids(did_bytes):
    """지원 DID 목록 바이너리를 파싱하여 문자열로 반환"""
    did_list = []
    for i in range(0, len(did_bytes), 2):
        did_chunk = did_bytes[i:i+2]
        if len(did_chunk) == 2:
            did_value = int.from_bytes(did_chunk, 'big')
            description, _ = DID_DESCRIPTIONS.get(did_value, ("알 수 없는 DID", None))
            did_list.append(f"  - 0x{did_value:04X}: {description}")
    return "\n".join(did_list)

# --- GUI 생성 (기존 코드와 거의 동일) ---
window = tk.Tk()
window.title("ECU 진단 툴 (udsoncan 기반)")
window.geometry("600x800")
window.configure(bg="#f0f0f0")

default_font = tkFont.nametofont("TkDefaultFont")
default_font.configure(family="맑은 고딕", size=9)

session_status_label = tk.Label(window, text="현재 세션: 확인 중...", font=("맑은 고딕", 9, "italic"), bg="#f0f0f0", fg="blue")
session_status_label.pack(pady=(5, 0))

# --- 1. 센서 데이터 그룹 ---
sensor_frame = tk.LabelFrame(window, text=" 센서 데이터 ", padx=10, pady=5, bg="#f0f0f0")
sensor_frame.pack(pady=5, padx=10, fill="x")
laser_button = tk.Button(sensor_frame, text="레이저", command=lambda: request_did_read(0x1000))
laser_button.grid(row=0, column=0, padx=5, pady=5, sticky="ew")
left_us_button = tk.Button(sensor_frame, text="초음파(좌)", command=lambda: request_did_read(0x2000))
left_us_button.grid(row=0, column=1, padx=5, pady=5, sticky="ew")
right_us_button = tk.Button(sensor_frame, text="초음파(우)", command=lambda: request_did_read(0x2001))
right_us_button.grid(row=0, column=2, padx=5, pady=5, sticky="ew")
rear_us_button = tk.Button(sensor_frame, text="초음파(후방)", command=lambda: request_did_read(0x2002))
rear_us_button.grid(row=0, column=3, padx=5, pady=5, sticky="ew")
all_us_button = tk.Button(sensor_frame, text="초음파(모두)", command=request_all_ultrasonic_data, font=("맑은 고딕", 9, "bold"))
all_us_button.grid(row=0, column=4, padx=5, pady=5, sticky="ew")
for i in range(5): sensor_frame.grid_columnconfigure(i, weight=1)

# --- 2. ECU 정보 그룹 ---
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

# --- 3. 진단 및 제어 그룹 ---
control_frame = tk.LabelFrame(window, text=" 진단 및 제어 ", padx=10, pady=5, bg="#f0f0f0")
control_frame.pack(pady=5, padx=10, fill="x")

dtc_button = tk.Button(control_frame, text="DTC 정보 읽기", command=request_dtc_data)
dtc_button.grid(row=0, column=0, columnspan=2, padx=5, pady=5, sticky="ew")
# AEB DID를 0xAEB0로 가정
aeb_on_button = tk.Button(control_frame, text="AEB 기능 ON", command=lambda: request_aeb_write(True))
aeb_on_button.grid(row=0, column=2, padx=5, pady=5, sticky="ew")
aeb_off_button = tk.Button(control_frame, text="AEB 기능 OFF", command=lambda: request_aeb_write(False))
aeb_off_button.grid(row=0, column=3, padx=5, pady=5, sticky="ew")
# 모터 루틴 RID를 0x0001, 0x0002로 가정
motor_forward_button = tk.Button(control_frame, text="모터 정회전 테스트", command=lambda: request_routine_start(0x0001))
motor_forward_button.grid(row=2, column=0, columnspan=2, padx=5, pady=5, sticky="ew")
motor_reverse_button = tk.Button(control_frame, text="모터 역회전 테스트", command=lambda: request_routine_start(0x0002))
motor_reverse_button.grid(row=2, column=2, columnspan=2, padx=5, pady=5, sticky="ew")

session_extended_button = tk.Button(control_frame, text="세션 시작 (Ext)", command=lambda: request_session_change(0x03))
session_extended_button.grid(row=1, column=0, columnspan=2, padx=5, pady=5, sticky="ew")
session_default_button = tk.Button(control_frame, text="세션 종료 (Def)", command=lambda: request_session_change(0x01))
session_default_button.grid(row=1, column=2, columnspan=2, padx=5, pady=5, sticky="ew")




# --- ✨ 4. OTA 그룹 추가 ---
ota_frame = tk.LabelFrame(window, text=" OTA 업데이트 ", padx=10, pady=5, bg="#f0f0f0")
ota_frame.pack(pady=5, padx=10, fill="x")

ota_button = tk.Button(ota_frame, text="펌웨어 파일 선택 (OTA)", command=select_ota_file)
ota_button.pack(fill="x", padx=5, pady=5)



for i in range(4): control_frame.grid_columnconfigure(i, weight=1)

# --- 결과 텍스트 창 ---
result_text = scrolledtext.ScrolledText(window, height=8, font=("Consolas", 14))
result_text.pack(pady=10, padx=10, fill="both", expand=True)
result_text.insert(tk.END, "ECU에 연결 중...")
result_text.config(state=tk.DISABLED)

# --- 메인 실행 로직 ---
if __name__ == "__main__":
    doip_client = None
    comm_thread = None
    try:
        # 1. DoIP 클라이언트 생성 및 연결
        update_result_text(f"[*] DoIP 서버 [{RPI_HOST}]에 연결 시도 중...")
        doip_client = DoIPClient(RPI_HOST, ECU_LOGICAL_ADDRESS, auto_reconnect_tcp=True)
        conn = DoIPClientUDSConnector(doip_client)
        update_result_text("🤝 DoIP 소켓 연결 성공!")

        # 2. 통신 스레드 시작
        comm_thread = threading.Thread(target=communication_thread, args=(doip_client, conn, window), daemon=True)
        comm_thread.start()

        # 3. GUI 메인 루프 시작
        window.mainloop()

    except Exception as e:
        update_result_text(f"\n❌ 클라이언트 실행 중 심각한 오류 발생: {e}")
        time.sleep(5)
    finally:
        # 4. 프로그램 종료 시 자원 정리
        print("👋 프로그램을 종료합니다.")
        stop_event.set() # 스레드에 종료 신호 전송
        if comm_thread:
            comm_thread.join(timeout=1) # 스레드가 끝날 때까지 대기
        if doip_client:
            doip_client.close()
            print("🔌 DoIP 연결이 종료되었습니다.")