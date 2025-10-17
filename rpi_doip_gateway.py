import socket
import can
import time

def main():
    # --- 1. CAN 버스 설정 ---
    # ECU로부터 오는 응답 ID(0x7E8)만 듣도록 필터를 명시적으로 설정합니다.
    can_filters = [
        {"can_id": 0x7E8, "can_mask": 0x7FF, "extended": False}
    ]
    # can.interface.Bus를 호출할 때 can_filters 인자를 추가합니다.
    can_bus = can.interface.Bus(channel='can0', interface='socketcan', can_filters=can_filters)
    print(">>> CAN 버스 초기화 성공 (수신 필터: ID 0x7E8)")

    # --- 2. 이더넷 TCP 서버 설정 ---
    HOST = '0.0.0.0'
    PORT = 13400

    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        s.bind((HOST, PORT))
        s.listen()
        print(f"[*] DoIP 게이트웨이가 {PORT} 포트에서 PC의 연결을 기다립니다...")
        
        while True:
            conn, addr = s.accept()
            with conn:
                print(f"\n[*] PC {addr} 에서 연결됨")
                
                # (PC -> CAN 방향 코드는 변경 없음)
                doip_packet_from_pc = conn.recv(1024)
                if not doip_packet_from_pc:
                    continue
                print(f"[PC -> RPi] 받은 DoIP 패킷: {doip_packet_from_pc.hex().upper()}")
                uds_request_payload = doip_packet_from_pc[8:]
                pci = len(uds_request_payload)
                can_data = [pci] + list(uds_request_payload)
                request_msg = can.Message(arbitration_id=0x7E0, data=can_data, is_extended_id=False)
                can_bus.send(request_msg)
                print(f"[RPi -> ECU] CAN으로 UDS 요청 전송: {request_msg.data.hex().upper()}")
                
                # (CAN -> PC 방향 코드도 변경 없음)
                print("[*] ECU 응답 수신 대기...")
                uds_response_payload = None
                first_msg = can_bus.recv(timeout=2)
                
                if first_msg: # 필터 덕분에 first_msg.arbitration_id == 0x7E8 임이 보장됨
                    pci_type = first_msg.data[0] & 0xF0
                    if pci_type == 0x00:
                        sf_len = first_msg.data[0] & 0x0F
                        uds_response_payload = first_msg.data[1:1+sf_len]
                        print(f"[ECU -> RPi] CAN으로 단일 프레임(SF) 응답 수신: {uds_response_payload.hex().upper()}")
                    elif pci_type == 0x10:
                        # (다중 프레임 처리 로직... 현재는 생략)
                        print(f"[ECU -> RPi] CAN으로 최초 프레임(FF) 수신... (처리 로직 필요)")
                        # 여기에 FF/FC/CF 처리 로직 추가 필요

                if uds_response_payload:
                    print(f"[*] 최종 조립된 UDS 응답: {uds_response_payload.hex().upper()}")
                    protocol_version = 0x02
                    payload_type = 0x8001
                    payload_length = len(uds_response_payload)
                    doip_header = bytearray([protocol_version, protocol_version ^ 0xFF])
                    doip_header.extend(payload_type.to_bytes(2, 'big'))
                    doip_header.extend(payload_length.to_bytes(4, 'big'))
                    doip_packet_to_pc = bytes(doip_header) + uds_response_payload
                    conn.sendall(doip_packet_to_pc)
                    print(f"[RPi -> PC] DoIP로 최종 응답 전송 완료")
                else:
                    print("[-] ECU로부터 응답 수신 실패 (타임아웃)")

if __name__ == "__main__":
    main()