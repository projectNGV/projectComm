import os
import time
import struct
import threading
from doipclient import DoIPClient
from doipclient.connectors import DoIPClientUDSConnector
from udsoncan.client import Client
from udsoncan import Request, services, DidCodec

# --- 설정값 ---
ECU_IP = '192.168.137.10'
ECU_LOGICAL_ADDRESS = 0x1000
TOF_SENSOR_DID = 0x1000
TOF_INVALID_VALUE = 0xFFFFFF # ECU의 에러 값과 동일하게 정의
MAX_BLOCK_SIZE = 4093 # UDS 헤더 크기: 2 바이트 (SID + Block Sequence) 제외

# UDS 설정
uds_config = {
    'data_identifiers': {
        TOF_SENSOR_DID: DidCodec('3s')  # '3s'는 3바이트 바이트 시퀀스를 의미
    },
    # P2 Timeout: 서버(ECU)의 초기 응답을 기다리는 시간 (단위: 초)
    'p2_timeout': 10,
    # P2* Timeout: 서버가 "처리 중(Response Pending)" 응답을 보낸 후, 다음 응답을 기다리는 시간
    'p2_star_timeout': 5
}

# 스레드 제어를 위한 전역 이벤트
stop_listening_event = threading.Event()
stop_tester_present_event = threading.Event() # TesterPresent 전송 제어용

def display_menu():
    print("\n========= [UDS 진단기 (최종 수정)] =========")
    print("1. ToF 센서 값 요청")
    print("2. ToF 센서 값 주기적 읽기 시작/중지")
    print("3. [테스트] 파일 데이터 전송 (Transfer Data)")
    print("4. [테스트] ECU 전송 상태 초기화 (Transfer Exit)")
    print("5. [테스트] DTC 정보 요청 (가변 길이 응답)")
    print("6. TesterPresent 주기적 전송 시작/중지")
    print("7. 종료")
    print("==============================================")
    return input("메뉴를 선택하세요: ")

def listen_for_periodic_data(conn):
    """주기적으로 들어오는 데이터를 수신하고 출력하는 함수 (스레드에서 실행됨)"""
    print("\n👂 ToF 센서 데이터 수신 시작... (메뉴와 함께 표시됩니다)")
    # stop_listening_event가 설정되지 않은 동안에만 루프 실행
    while not stop_listening_event.is_set():
        try:
            # wait_frame은 타임아웃 시 None을 반환하므로 예외 처리가 필요 없음
            payload = conn.wait_frame(timeout=0.5)
            if payload:
                # 1. 첫 바이트가 게이트웨이가 붙여준 응답 SID(0x62)인지 먼저 확인
                if payload[0] == 0x62:
                    # 2. DID를 두 번째 바이트부터 파싱 (payload[1:3])
                    received_did = int.from_bytes(payload[1:3], 'big')
                    if received_did == TOF_SENSOR_DID:
                        tof_distance = int.from_bytes(payload[3:], 'big')
                        
                        # 4. 수신된 값이 에러 값인지 확인하여 다른 메시지 출력
                        if tof_distance == TOF_INVALID_VALUE:
                            display_text = "센서 값 유효하지 않음 (Timeout)"
                        else:
                            display_text = f"{tof_distance} mm"
                        
                        # 한 줄에 계속 덮어쓰면서 출력
                        print(f"\r  > 수신된 ToF 값: {display_text}                   ", end='', flush=True)

        except Exception as e:
            if not stop_listening_event.is_set():
                # 스레드 종료 시 예상되는 오류는 출력하지 않음
                pass
            break
    # 수신이 중지되면, 덮어쓰던 줄을 지우고 다음 줄로 넘어감
    print("\n\n⏹️ 데이터 수신이 중지되었습니다. 메뉴를 다시 선택하세요.")

def send_tester_present_periodically(client):
    """2초마다 TesterPresent 요청을 보내는 함수 (스레드에서 실행됨)"""
    print("\n[세션유지] ✅ TesterPresent 주기적 전송을 시작합니다. (2초 간격)")
    while not stop_tester_present_event.is_set():
        try:
            req = Request(service=services.TesterPresent, subfunction=0x00)
            response = client.send_request(req)

            if response.positive:
                # \r과 end=''를 이용해 한 줄에 계속 덮어쓰며 상태 표시
                print(f"\r[세션유지] ┕ ECU 응답 OK ({time.strftime('%H:%M:%S')})", end='', flush=True)
            else:
                print(f"\r[세션유지] ┕ ECU 부정 응답: {response.code_name} ({time.strftime('%H:%M:%S')})", end='', flush=True)
        except Exception as e:
            print(f"\n[세션유지] ❌ TesterPresent 전송 중 오류: {e}")
            break # 오류 발생 시 스레드 종료
        
        # 2초 동안 대기. stop_tester_present_event가 설정되면 즉시 중지.
        stop_tester_present_event.wait(2) 

    print("\n[세션유지] ⏹️ TesterPresent 주기적 전송을 중지합니다.")

def main():
    """메인 실행 함수"""
    doip_client = None
    listener_thread = None
    tester_present_thread = None

    try:
        print(f"✅ DoIP 서버 [{ECU_IP}]에 연결 시도 중...")
        doip_client = DoIPClient(ECU_IP, ECU_LOGICAL_ADDRESS, auto_reconnect_tcp=True)
        conn = DoIPClientUDSConnector(doip_client)
        print("🤝 DoIP 소켓 연결 성공!")

        with Client(conn, config=uds_config, request_timeout=30) as client:
            print("✅ UDS 클라이언트 생성 완료.")
            while True:
                choice = display_menu()
                
                if choice == '1':
                    print("\n▶️ [UDS 0x22] ToF 센서 값 요청 중...")
                    try:
                        response = client.read_data_by_identifier(TOF_SENSOR_DID)
                        if response.positive:
                            raw_bytes = response.data[2:]
                            tof_distance = int.from_bytes(raw_bytes, 'big')

                            if tof_distance == TOF_INVALID_VALUE:
                                print("✅ 수신 성공! ToF 센서 값이 유효하지 않습니다 (Timeout).")
                            else:
                                print(f"✅ 수신 성공! ToF 센서 값: {tof_distance} mm")
                        else:
                            print(f"⚠️ ECU가 부정 응답을 보냈습니다: {response.code_name}")
                    except Exception as e:
                        print(f"❌ ToF 센서 값 요청 중 오류 발생: {e}")

                elif choice == '2': # 주기적 읽기 시작
                    if listener_thread and listener_thread.is_alive():
                        # --- 중지 로직 ---
                        print("\n⏹️ ToF 주기적 읽기를 중지합니다...")
                    
                        try:
                            # 1. 수신 스레드에 먼저 중지 신호를 보냄
                            stop_listening_event.set()
                            listener_thread.join(timeout=1) # 스레드가 끝날 때까지 최대 1초 대기
                            
                            # 2. 통신 채널에 남아있을지 모를 찌꺼기 데이터 정리
                            print("...통신 채널을 정리하는 중...")
                            try:
                                while conn.wait_frame(timeout=0.05) is not None: pass
                            except Exception: pass
                            print("...채널 정리 완료.")

                            # 3. ECU에 공식적으로 중지 요청 전송
                            print("▶️ [UDS 0x2A] ToF 센서 값 주기적 읽기 중지 요청...")
                            did_as_bytes_stop = struct.pack('>H', TOF_SENSOR_DID)
                            req_stop = Request(service=services.ReadDataByPeriodicIdentifier, subfunction=0x02, data=did_as_bytes_stop)
                            response_stop = client.send_request(req_stop)
                            if response_stop.positive:
                                print("✅ ECU가 주기적 전송을 성공적으로 중지했습니다.")
                            else:
                                print(f"⚠️ ECU가 중지 요청에 부정 응답을 보냈습니다: {response_stop.code_name}")
                        except Exception as e:
                            print(f"❌ 중지 처리 중 오류 발생: {e}")
                    else:
                        # --- 시작 로직 ---
                        print("\n▶️ [UDS 0x2A] ToF 센서 값 주기적 읽기 시작 요청...")
                        try:
                            did_as_bytes = struct.pack('>H', TOF_SENSOR_DID)
                            req = Request(service=services.ReadDataByPeriodicIdentifier, subfunction=0x01, data=did_as_bytes)
                            response = client.send_request(req)

                            if response.positive:
                                print("✅ ECU가 주기적 전송을 시작했습니다. 데이터가 곧 표시됩니다.")
                                stop_listening_event.clear()
                                listener_thread = threading.Thread(target=listen_for_periodic_data, args=(conn,))
                                listener_thread.start()
                            else:
                                print(f"⚠️ ECU가 시작 요청에 부정 응답을 보냈습니다: {response.code_name}")
                        except Exception as e:
                            print(f"❌ 시작 요청 중 오류 발생: {e}")

                elif choice == '3':
                    filepath = input("전송할 파일의 경로를 입력하세요: ")
                    if not os.path.exists(filepath):
                        print(f"❌ 파일을 찾을 수 없습니다: {filepath}")
                        continue
                    
                    with open(filepath, 'rb') as f:
                        file_data = f.read()
                    
                    total_size = len(file_data)
                    print(f"\n▶️ [UDS 0x36] 파일 전송 시작 (전체 크기: {total_size} 바이트)")
                    
                    bytes_sent = 0
                    block_sequence = 1
                    success = True

                    print(f"DEBUG >>> 데이터 분할 전송 루프 시작 (bytes_sent={bytes_sent}, total_size={total_size})")

                    while bytes_sent < total_size:
                        chunk = file_data[bytes_sent : bytes_sent + MAX_BLOCK_SIZE]
                        
                        # --- 디버깅용 print 문 ---
                        print(f"DEBUG >>> 루프 #{block_sequence}: {len(chunk)} 바이트 크기의 청크 생성")
                        
                        try:
                            req = Request(
                                service=services.TransferData,
                                subfunction=block_sequence,
                                data=chunk
                            )
                            response = client.send_request(req)
                            if not response.positive:
                                print(f"❌ ECU가 청크 #{block_sequence}에 대해 부정 응답: {response.code_name}")
                                success = False
                                break
                            
                            bytes_sent += len(chunk)
                            block_sequence = (block_sequence + 1) % 256
                            if block_sequence == 0: block_sequence = 1
                            print(f"     (진행률: {bytes_sent / total_size:.1%})")
                            
                            time.sleep(0.05) # 50ms 정도의 지연

                        except Exception as e:
                            print(f"❌ 청크 #{block_sequence} 전송 중 오류 발생: {e}")
                            # print("     ⚠️ 타임아웃 발생. ECU 상태 리셋 및 통신 채널 정리를 시도합니다...")
                            # try:
                            #     # 이 요청은 0x77 응답을 기대하지만, 채널에 남아있는
                            #     # 이전의 0x76 응답(유령 응답)을 대신 받게 될 수 있습니다.
                            #     client.request_transfer_exit()
                            #     print("     ✅ ECU 상태가 성공적으로 초기화되었습니다.")
                            # except Exception as e_exit:
                            #     # 유령 응답을 성공적으로 "읽고 버린" 경우, UnexpectedResponseException이 발생합니다.
                            #     # 이는 의도된 동작이므로, 채널이 정리되었다고 간주할 수 있습니다.
                            #     print(f"     ℹ️ 채널에 남아있던 이전 응답을 성공적으로 정리했습니다. ({e_exit})")
                            
                            success = False
                            break
                    
                    print(f"DEBUG >>> 데이터 분할 전송 루프 종료")
                    
                    if success:
                        print("✅ 파일 전송이 성공적으로 완료되었습니다!")
                
                elif choice == '4':
                    try:
                        print("\n▶️ [UDS 0x37] ECU 전송 상태 초기화 요청")
                        client.request_transfer_exit()
                        print("✅ [UDS 0x37] ECU 상태가 성공적으로 초기화되었습니다.")
                    except Exception as e:
                        print(f"❌ [UDS 0x37] 요청 중 오류 발생: {e}")

                elif choice == '5':
                    print("\n▶️ [UDS 0x19] DTC 정보 요청 중...")
                    try:
                        # reportNumberOfDTCByStatusMask (0x01) 요청
                        # StatusMask 0xFF는 모든 상태의 DTC를 의미합니다.
                        req = Request(
                            service=services.ReadDTCInformation,
                            subfunction=0x01,
                            data=b'\xFF'
                        )
                        response = client.send_request(req)

                        if response.positive:
                            print(f"✅ DTC 정보 수신 성공! (총 응답 길이: {len(response.data)} 바이트)")

                            # 1. 응답 데이터 길이 유효성 검사 (최소 3바이트 필요)
                            if len(response.data) < 3:
                                print("   ⚠️ 응답 데이터가 너무 짧습니다.")
                                continue

                            # 2. 응답 데이터 파싱
                            sub_function_echo = response.data[0]
                            availability_mask = response.data[1] # 보통 0xFF
                            dtc_count = response.data[2]

                            print(f"  - Sub-function 확인: {sub_function_echo:#04x}")
                            print(f"  - DTC 상태 마스크: {availability_mask:#04x}")
                            print(f"  - 발견된 DTC 개수: {dtc_count}")
                            
                            # 3. 실제 DTC 데이터 파싱 (존재하는 경우에만)
                            dtc_records_data = response.data[3:]
                            
                            # 4. 수신된 DTC 개수와 실제 데이터 길이가 일치하는지 확인
                            if len(dtc_records_data) == dtc_count * 4:
                                for i in range(dtc_count):
                                    start_index = i * 4
                                    # 각 레코드는 4바이트: DTC(3) + Status(1)
                                    record = dtc_records_data[start_index : start_index + 4]
                                    
                                    dtc_id = int.from_bytes(record[0:3], 'big')
                                    dtc_status = record[3]

                                    print(f"    > DTC #{i+1}: {dtc_id:#08x}, Status: {dtc_status:#04x}")
                            
                    except Exception as e:
                        print(f"❌ DTC 정보 요청 중 오류 발생: {e}")
                
                elif choice == '6':
                    # 스레드가 실행 중인지 확인하여 시작/중지 토글
                    if tester_present_thread and tester_present_thread.is_alive():
                        # 실행 중이면 -> 중지
                        stop_tester_present_event.set()
                        tester_present_thread.join() # 스레드가 완전히 끝날 때까지 대기
                    else:
                        # 중지 상태이면 -> 시작
                        stop_tester_present_event.clear()
                        tester_present_thread = threading.Thread(target=send_tester_present_periodically, args=(client,))
                        tester_present_thread.start()

                elif choice == '7':
                    print("👋 프로그램을 종료합니다.")
                    break
                
                else:
                    print("⚠️ 잘못된 입력입니다.")

    except Exception as e:
        print(f"\n❌ 클라이언트 실행 중 심각한 오류 발생: {e}")
    finally:
        stop_listening_event.set()
        stop_tester_present_event.set()
        
        if listener_thread and listener_thread.is_alive():
            listener_thread.join(timeout=1)
        if tester_present_thread and tester_present_thread.is_alive():
            tester_present_thread.join(timeout=1)
        if doip_client:
            doip_client.close()
            print("🔌 DoIP 연결이 종료되었습니다.")

if __name__ == '__main__':
    main()