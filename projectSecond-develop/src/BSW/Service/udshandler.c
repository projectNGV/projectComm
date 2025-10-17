#include "udshandler.h"

/* ==========================================================================
 * UDS/ISO-TP 프로토콜 전문 처리 함수
 * ========================================================================== */

void udsHandler (unsigned char *rxData, int rxLen)
{
    // 이 함수는 rxID가 0x7E0인 메시지만을 처리합니다.
    uint8 pci = rxData[0];
    uint8 type = pci & 0xF0;

    switch (type)
    {
        case 0x00 :
        { // SF (Single Frame)
            uint8 sfLen = pci & 0x0F;
            if (sfLen == 0 || sfLen > 7)
                break;

            uint8 SID = rxData[1];

            // SID 0x10 (Diagnostic Session Control) 처리 로직 ---
            if (SID == SESSION_CONTROL && sfLen >= 2)
            {
                uint8 sub_function = rxData[2];
                // 지원하는 세션(Default, Extended)인지 확인합니다.
                if (sub_function == SESSION_DEFAULT || sub_function == SESSION_EXTENDED)
                {
                    // 세션 관리 모듈을 통해 현재 세션을 변경합니다.
                    session_setCurrent((DiagnosticSession) sub_function);

                    // "세션 변경 성공" 긍정 응답을 전송합니다.
                    uint8 payload[] = {0x50, sub_function}; // 0x50 = 0x10 + 0x40
                    isotp_send_response(0x7E8, payload, sizeof(payload));
                }
                else
                {
                    // 지원하지 않는 세션에 대한 부정 응답
                    uint8 nr_payload[] = {0x7F, 0x10, 0x12}; // NRC 0x12 = subFunctionNotSupported
                    isotp_send_response(0x7E8, nr_payload, sizeof(nr_payload));
                }
            }

            else if (SID == TESTER_PRESENT)
            {
                uint8 sub_function = rxData[2];
                if (sub_function == 0x00)
                { // sub-function 0x00(응답 필요)
                    uint8 payload[] = {0x7E, 0x00};
                    isotp_send_response(0x7E8, payload, sizeof(payload));
                }
                else if (sub_function == 0x80)
                { // sub-function 0x80 (응답 불필요)
                  // 아무 응답도 안 보냄
                }
            }

            else if (SID == READ_DATA_BY_IDENTIFIER && sfLen >= 3)
            { // 03 22 10 00  =>
                uint16 DID = ((uint16) rxData[2] << 8) | rxData[3];

                switch (DID)
                {
                    case DID_LASER_SENSOR_DISTANCE :
                    { // 레이저 센서 거리 요청
                      // 1. tofGetValue()가 mm 단위를 반환한다고 약속합니다.
                        uint16 distance_mm = (uint16) tofGetValue();

                        // 2. 이 mm 값을 UDS 페이로드에 담아 전송합니다.
                        uint8 payload[5] = {0x62, 0x10, 0x00,           // UDS 긍정 응답 헤더
                                (uint8) (distance_mm >> 8),    // 거리 값(mm) 상위 바이트
                                (uint8) (distance_mm & 0xFF)   // 거리 값(mm) 하위 바이트
                                };
                        isotp_send_response(0x7E8, payload, sizeof(payload));
                        break;
                    }
                    case DID_VEHICLE_MANUFACTURER_ECU_PART_NUMBER :
                    { // ECU 하드웨어 부품 번호 요청
                        uint8 payload[50]; // 문자열을 담을 충분한 공간
                        uint16 plen = 0;

                        // UDS 긍정 응답 헤더
                        payload[plen++] = 0x62;
                        payload[plen++] = 0xF1;
                        payload[plen++] = 0x87;

                        // 실제 데이터 (문자열) 복사
                        memcpy(&payload[plen], g_config.partNumber, strlen(g_config.partNumber));
                        plen += strlen(g_config.partNumber);

                        // isotp_send_response가 알아서 다중 프레임(FF/CF)으로 보낼 것임
                        isotp_send_response(0x7E8, payload, plen);
                        break;
                    }

                    case DID_ECU_SERIAL_NUMBER :
                    { // ECU 고유 시리얼 번호 요청
                        uint8 payload[50];
                        uint16 plen = 0;

                        payload[plen++] = 0x62;
                        payload[plen++] = 0xF1;
                        payload[plen++] = 0x8C;

                        memcpy(&payload[plen], g_config.serialNumber, strlen(g_config.serialNumber));
                        plen += strlen(g_config.serialNumber);

                        isotp_send_response(0x7E8, payload, plen);
                        break;
                    }

                    case DID_VEHICLE_IDENTIFICATION_NUMBER :
                    { // 차대번호(VIN) 요청
                        uint8 payload[50];
                        uint16 plen = 0;
                        payload[plen++] = 0x62;
                        payload[plen++] = 0xF1;
                        payload[plen++] = 0x90;

                        memcpy(&payload[plen], g_config.vin, strlen(g_config.vin));
                        plen += strlen(g_config.vin);
                        isotp_send_response(0x7E8, payload, plen);
                        break;
                    }

                    case DID_ECU_SUPPLIER_INFORMATION :
                    { // ECU 공급업체 정보 요청
                        uint8 payload[50];
                        uint16 plen = 0;
                        payload[plen++] = 0x62;
                        payload[plen++] = 0xF1;
                        payload[plen++] = 0x93;
                        memcpy(&payload[plen], g_config.supplier, strlen(g_config.supplier));
                        plen += strlen(g_config.supplier);
                        isotp_send_response(0x7E8, payload, plen);
                        break;
                    }

                    case DID_ECU_MANUFACTURING_DATE :
                    { // ECU 제조 날짜 요청
                        uint8 payload[50];
                        uint16 plen = 0;
                        payload[plen++] = 0x62;
                        payload[plen++] = 0xF1;
                        payload[plen++] = 0x92;

                        memcpy(&payload[plen], g_config.manufacturingDate, strlen(g_config.manufacturingDate));
                        plen += strlen(g_config.manufacturingDate);
                        isotp_send_response(0x7E8, payload, plen);
                        break;
                    }

                    case DID_SUPPORTED_DIDS_LIST :
                    { // 지원 DID 목록 요청
                        uint8 payload[50];
                        uint16 plen = 0;

                        // UDS 긍정 응답 헤더
                        payload[plen++] = 0x62;
                        payload[plen++] = 0xF1;
                        payload[plen++] = 0xA0;

                        // config.c에 정의된 지원 DID 목록 배열 순회하며
                        // 각 DID 2바이트씩 페이로드에 추가
                        for (int i = 0; i < NUM_SUPPORTED_DIDS; i++)
                        {
                            payload[plen++] = (uint8) (SUPPORTED_DIDS[i] >> 8); // DID 상위 바이트
                            payload[plen++] = (uint8) (SUPPORTED_DIDS[i] & 0xFF); // DID 하위 바이트
                        }

                        // 최종 페이로드 전송
                        isotp_send_response(0x7E8, payload, plen);
                        break;

                    }

                    case DID_ULTRASONIC_LEFT_DISTANCE :
                    { // 초음파 (좌) 센서 거리 요청
                      // 1. 좌측 초음파 센서의 거리를 cm 단위로 반환하는 함수 호출한다
                        float distance_cm = ultrasonic_getDistanceCm(ULT_LEFT);

                        // 2. 만약 센서 측정에 실패했다면, 부정 응답(NRC)를 보낸다.
                        if (distance_cm < 0)
                        {
                            uint8 nr_payload[3] = {0x7F, 0x22, 0x31}; // 0x31는 requestOutOfRange
                            isotp_send_response(0x7E8, nr_payload, sizeof(nr_payload));
                            break;
                        }

                        // 3. float 값을 정수로 변환(소수점 첫째 자리까지 표현하기 위해 10 곱함)
                        // ex) 15.7cm => 157
                        uint16 scaled_distance = (uint16) (distance_cm * 10.0f);

                        // 4. 변환된 2바이트 정수 값을 페이로드에 담아 전송
                        uint8 payload[5] = {0x62, 0x20, 0x00, (uint8) (scaled_distance >> 8), (uint8) (scaled_distance
                                & 0xFF)};

                        isotp_send_response(0x7E8, payload, sizeof(payload));

                        break;
                    }
                    case DID_ULTRASONIC_RIGHT_DISTANCE :
                    { // 초음파 (우) 센서 거리 요청
                      // 1. 우측 초음파 센서의 거리를 cm 단위로 반환하는 함수 호출한다
                        float distance_cm = ultrasonic_getDistanceCm(ULT_RIGHT);

                        // 2. 만약 센서 측정에 실패했다면, 부정 응답(NRC)를 보낸다.
                        if (distance_cm < 0)
                        {
                            uint8 nr_payload[3] = {0x7F, 0x22, 0x31}; // 0x31는 requestOutOfRange
                            isotp_send_response(0x7E8, nr_payload, sizeof(nr_payload));
                            break;
                        }

                        // 3. float 값을 정수로 변환(소수점 첫째 자리까지 표현하기 위해 10 곱함)
                        // ex) 15.7cm => 157
                        uint16 scaled_distance = (uint16) (distance_cm * 10.0f);

                        // 4. 변환된 2바이트 정수 값을 페이로드에 담아 전송
                        uint8 payload[5] = {0x62, 0x20, 0x01, (uint8) (scaled_distance >> 8), (uint8) (scaled_distance
                                & 0xFF)};

                        isotp_send_response(0x7E8, payload, sizeof(payload));

                        break;
                    }
                    case DID_ULTRASONIC_REAR_DISTANCE :
                    { // 초음파 (후방) 센서 거리 요청
                      // 1. 후방 초음파 센서의 거리를 cm 단위로 반환하는 함수 호출한다
                        float distance_cm = ultrasonic_getDistanceCm(ULT_REAR);

                        // 2. 만약 센서 측정에 실패했다면, 부정 응답(NRC)를 보낸다.
                        if (distance_cm < 0)
                        {
                            uint8 nr_payload[3] = {0x7F, 0x22, 0x31}; // 0x31는 requestOutOfRange
                            isotp_send_response(0x7E8, nr_payload, sizeof(nr_payload));
                            break;
                        }

                        // 3. float 값을 정수로 변환(소수점 첫째 자리까지 표현하기 위해 10 곱함)
                        // ex) 15.7cm => 157
                        uint16 scaled_distance = (uint16) (distance_cm * 10.0f);

                        // 4. 변환된 2바이트 정수 값을 페이로드에 담아 전송
                        uint8 payload[5] = {0x62, 0x20, 0x02, (uint8) (scaled_distance >> 8), (uint8) (scaled_distance
                                & 0xFF)};

                        isotp_send_response(0x7E8, payload, sizeof(payload));

                        break;
                    }

                    case DID_ACTIVE_DIAGNOSTIC_SESSION :
                    { // 현재 진단 세션 상태 요청
                        uint8 current_session = (uint8) session_getCurrent();

                        // UDS 응답: 헤더(3) + 데이터(1) = 4바이트
                        uint8 payload[4] = {0x62, 0xF1, 0x86, // 긍정 응답 헤더
                                current_session   // 현재 세션 값
                                };
                        isotp_send_response(0x7E8, payload, sizeof(payload));
                        break;
                    }

                    default :
                    { // 메뉴판에 없는 DID
                        uint8 nr_payload[3] = {0x7F, 0x22, 0x31};
                        isotp_send_response(0x7E8, nr_payload, sizeof(nr_payload));
                        break;
                    }

                }
            }

            else if (SID == READ_DTC_INFORMATION && sfLen >= 2)
            {
                uint8 sub_function = rxData[2];
                if (sub_function == 0x02)
                {
                    uint8 payload[50];
                    uint16 plen = 0;
                    payload[plen++] = 0x59;
                    payload[plen++] = rxData[2];
                    payload[plen++] = rxData[3];

                    for (int i = 0; i < MAX_DTCS; i++)
                    {
                        if (g_dtcStorage[i].status & DTC_STATUS_TEST_FAILED)
                        {
                            payload[plen++] = (uint8) (g_dtcStorage[i].dtc_code >> 16);
                            payload[plen++] = (uint8) (g_dtcStorage[i].dtc_code >> 8);
                            payload[plen++] = (uint8) (g_dtcStorage[i].dtc_code & 0xFF);
                            payload[plen++] = g_dtcStorage[i].status;
                        }
                    }
                    isotp_send_response(0x7E8, payload, plen);
                }
            }

            // SID 0x2E (Write Data By Identifier) 수정 ---
            else if (SID == WRITE_DATA_BY_IDENTIFIER && sfLen >= 4)
            {
                // 1. 세션 확인: 이 기능은 Extended Session에서만 허용됩니다.
                if (session_getCurrent() != SESSION_EXTENDED)
                {
                    // 현재 세션이 Extended가 아니면 부정 응답을 보내고 종료합니다.
                    uint8 nr_payload[] = {0x7F, 0x2E, 0x7E}; // NRC 0x7E = subFunctionNotSupportedInActiveSession
                    isotp_send_response(0x7E8, nr_payload, sizeof(nr_payload));
                    break; // 여기서 처리를 중단합니다.
                }

                // 2. (세션 확인 통과 시) 기존 쓰기 로직 실행
                uint16 DID = ((uint16) rxData[2] << 8) | rxData[3];
                if (DID == 0x2000)
                {
                    uint8 new_status = rxData[4];
                    g_config.isAebEnabled = (new_status == 0x01);
                    // ... (myPrintf 등 디버깅 코드)
                    uint8 positive_response[] = {0x6E, 0x20, 0x00};
                    isotp_send_response(0x7E8, positive_response, sizeof(positive_response));
                }
                else
                {
                    uint8 nr_payload[3] = {0x7F, 0x2E, 0x31};
                    isotp_send_response(0x7E8, nr_payload, sizeof(nr_payload));
                }
            }
            // SID 0x31 (Routine Control)
            else if (SID == ROUTINE_CONTROL)
            {
                uint8 sub_function = rxData[2];
                // 2바이트 RID를 파싱합니다 (rxData[3]과 rxData[4] 사용).
                uint16 rid = ((uint16) rxData[3] << 8) | rxData[4];

                // 1. "루틴 시작" 요청(sub-function 0x01)이 맞는지 확인합니다.
                if (sub_function == 0x01) // 0x01 = startRoutine
                {
                    // 2. 안전을 위해, 현재 세션이 Extended Session인지 확인합니다.
                    if (session_getCurrent() != SESSION_EXTENDED)
                    {
                        // 아니라면 "조건이 맞지 않다"는 부정 응답을 보냅니다.
                        uint8 nr_payload[] = {0x7F, 0x31, 0x22}; // NRC 0x22 = conditionsNotCorrect
                        isotp_send_response(0x7E8, nr_payload, sizeof(nr_payload));
                        break; // 여기서 처리 중단
                    }

                    // 3. 지원하는 RID인지 확인합니다.
                    if (rid == RID_MOTOR_FORWARD_TEST || rid == RID_MOTOR_REVERSE_TEST)
                    {
                        // 4. "요청을 접수했다"는 긍정 응답을 먼저 보냅니다.
                        //    (실제 루틴 실행은 시간이 걸릴 수 있으므로)
                        uint8 pos_payload[] = {0x71, 0x01, rxData[3], rxData[4]}; // 0x71 = 0x31 + 0x40
                        isotp_send_response(0x7E8, pos_payload, sizeof(pos_payload));

                        // 5. routine_control.c에 있는 실제 루틴 실행 함수를 호출합니다.
                        startRoutine(rid);
                    }
                    else
                    {
                        // 지원하지 않는 RID에 대한 부정 응답
                        uint8 nr_payload[] = {0x7F, 0x31, 0x31}; // NRC 0x31 = requestOutOfRange
                        isotp_send_response(0x7E8, nr_payload, sizeof(nr_payload));
                    }
                }
                else
                {
                    // startRoutine(0x01) 외 다른 Sub-function에 대한 부정 응답
                    uint8 nr_payload[] = {0x7F, 0x31, 0x12}; // NRC 0x12 = subFunctionNotSupported
                    isotp_send_response(0x7E8, nr_payload, sizeof(nr_payload));
                }
            }
            break;
        }

        case 0x10 :
        { // FF (First Frame) - 긴 요청 수신
          // (현재는 간단히 FC만 보내는 로직)
            uint8 fc[8] = {0x30, 0x00, 0x00};
            canSend8(0x7E8, fc); // FF에 대한 FC 응답
            break;
        }
        case 0x30 :
        { // FC (Flow Control) - ECU가 보낸 FF에 대한 테스터의 응답 수신
            uint8 fs = pci & 0x0F;
            if (g_isotp_tx.active && fs == 0x00)
            {
                g_isotp_tx.bs = rxData[1];
                g_isotp_tx.stmin = rxData[2];
                g_isotp_tx.waiting_fc = 0;
                isotp_send_next_block(0x7E8);
            }
            else
            {
                g_isotp_tx.active = 0;
            }
            break;
        }
            // (CF 수신 로직 등은 필요시 추가)
    }

    // FC를 받은 후 남은 CF가 있다면 모두 전송
    if (g_isotp_tx.active && !g_isotp_tx.waiting_fc)
    {
        isotp_send_next_block(0x7E8);
    }
}
