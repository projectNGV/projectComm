#include "uds.h"
#include "sidhandler.h"
#include "cantp.h"
#include "tof.h"
#include "ultrasonic.h"
#include "uart.h"
#include "stm.h"
#include "config.h"
#include "session.h"
#include <string.h>

/* ---- 0x10: Diagnostic Session Control Handler ---- */
void handleSID10 (const uint8_t *data, uint16_t length, const uint8_t sid)
{
    if (length != 2)
    {
        sendNegativeResponse(sid, UDS_NRC_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT);
        return;
    }

    uint8_t subFunction = data[1];
    if (subFunction == SESSION_DEFAULT || subFunction == SESSION_EXTENDED)
    {
        session_setCurrent((DiagnosticSession) subFunction);

        uint16_t p2_star_value = P2_STAR_SERVER_MAX_MS / 10;
        uint8_t payload[] = {UDS_POSITIVE_RESPONSE_SID(sid), subFunction, (uint8) (P2_SERVER_MAX_MS >> 8),
                (uint8) (P2_SERVER_MAX_MS & 0xFF), (uint8) (p2_star_value >> 8), (uint8) (p2_star_value & 0xFF)};
        CANTP_SendResponse(payload, sizeof(payload));
    }
//    else if(subFunction == SESSION_DEFAULT || subFunction == SESSION_PROGRAMMING)
//    {
//        // 재원이꺼
//    }
    else
    {
        sendNegativeResponse(sid, UDS_NRC_SUB_FUNCTION_NOT_SUPPORTED);
    }
}

/* ---- 0x22: Read Data By Identifier Handler ---- */
void handleSID22 (const uint8_t *data, uint16_t length, const uint8_t sid)
{
    if (length != 3)
    {
        sendNegativeResponse(sid, UDS_NRC_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT);
        return;
    }

    uint16_t did = (data[1] << 8) | data[2];
    uint8_t responseMsg[256]; // Sufficient buffer
    uint16_t responseLen = 0;

    responseMsg[responseLen++] = UDS_POSITIVE_RESPONSE_SID(sid);
    responseMsg[responseLen++] = (did >> 8) & 0xFF;
    responseMsg[responseLen++] = did & 0xFF;

    switch (did)
    {
        case UDS_DID_TOF_SENSOR : // 0x1000 from uds.h, same as DID_LASER_SENSOR_DISTANCE
        {
            uint32_t currentTofValue = tofGetValue();
            responseMsg[responseLen++] = (currentTofValue >> 24) & 0xFF; // 최상위 바이트
            responseMsg[responseLen++] = (currentTofValue >> 16) & 0xFF;
            responseMsg[responseLen++] = (currentTofValue >> 8) & 0xFF;
            responseMsg[responseLen++] = currentTofValue & 0xFF;

            CANTP_SendResponse(responseMsg, responseLen);
            break;
        }
        case DID_ULTRASONIC_LEFT_DISTANCE :
        {
            float distance_cm = ultrasonic_getDistanceCm(ULT_LEFT);
            if (distance_cm < 0)
            {
                sendNegativeResponse(sid, UDS_NRC_CONDITIONS_NOT_CORRECT);
                return;
            }
            uint16_t scaled_distance = (uint16_t) (distance_cm * 10.0f);
            responseMsg[responseLen++] = (scaled_distance >> 8) & 0xFF;
            responseMsg[responseLen++] = scaled_distance & 0xFF;
            CANTP_SendResponse(responseMsg, responseLen);
            break;
        }
            // ... (other DIDs like ultrasonic right/rear)

        case DID_VEHICLE_MANUFACTURER_ECU_PART_NUMBER :
        {
            // 1. config.c에 저장된 부품 번호 문자열의 길이를 가져옵니다.
            size_t len = sizeof(g_config.partNumber);
            // 2. 응답 메시지 버퍼에 부품 번호 문자열을 복사합니다.
            memcpy(&responseMsg[responseLen], g_config.partNumber, len);
            // 3. 복사한 길이만큼 전체 응답 길이를 늘려줍니다.
            responseLen += len;
            // 4. CAN-TP를 통해 최종 응답 메시지를 전송합니다.
            CANTP_SendResponse(responseMsg, responseLen);
            break;
        }

        case DID_ECU_SERIAL_NUMBER :
        {
            // 1. config.c에 저장된 시리얼 번호 문자열의 길이를 가져옵니다.
            size_t len = sizeof(g_config.serialNumber);
            // 2. 응답 메시지 버퍼에 부품 번호 문자열을 복사합니다.
            memcpy(&responseMsg[responseLen], g_config.serialNumber, len);
            // 3. 복사한 길이만큼 전체 응답 길이를 늘려줍니다.
            responseLen += len;
            // 4. CAN-TP를 통해 최종 응답 메시지를 전송합니다.
            CANTP_SendResponse(responseMsg, responseLen);
            break;
        }

        case DID_VEHICLE_IDENTIFICATION_NUMBER :
        {
            // 1. config.c에 저장된 VIN 문자열의 길이를 가져옵니다.
            size_t len = sizeof(g_config.vin);
            // 2. 응답 메시지 버퍼에 부품 번호 문자열을 복사합니다.
            memcpy(&responseMsg[responseLen], g_config.vin, len);
            // 3. 복사한 길이만큼 전체 응답 길이를 늘려줍니다.
            responseLen += len;
            // 4. CAN-TP를 통해 최종 응답 메시지를 전송합니다.
            CANTP_SendResponse(responseMsg, responseLen);
            break;
        }

        case DID_ECU_SUPPLIER_INFORMATION :
        {
            // 1. config.c에 저장된 공급업체 문자열의 길이를 가져옵니다.
            size_t len = sizeof(g_config.supplier);
            // 2. 응답 메시지 버퍼에 부품 번호 문자열을 복사합니다.
            memcpy(&responseMsg[responseLen], g_config.supplier, len);
            // 3. 복사한 길이만큼 전체 응답 길이를 늘려줍니다.
            responseLen += len;
            // 4. CAN-TP를 통해 최종 응답 메시지를 전송합니다.
            CANTP_SendResponse(responseMsg, responseLen);
            break;
        }

        case DID_ECU_MANUFACTURING_DATE :
        {
            // 1. config.c에 저장된 공급업체 문자열의 길이를 가져옵니다.
            size_t len = sizeof(g_config.manufacturingDate);
            // 2. 응답 메시지 버퍼에 부품 번호 문자열을 복사합니다.
            memcpy(&responseMsg[responseLen], g_config.manufacturingDate, len);
            // 3. 복사한 길이만큼 전체 응답 길이를 늘려줍니다.
            responseLen += len;
            // 4. CAN-TP를 통해 최종 응답 메시지를 전송합니다.
            CANTP_SendResponse(responseMsg, responseLen);
            break;
        }

        case DID_SUPPORTED_DIDS_LIST :
        {
            // 1. config.c에 정의된 메뉴판(SUPPORTED_DIDS 배열)을 순회합니다.
            //    NUM_SUPPORTED_DIDS는 메뉴의 총 개수입니다.
            for (int i = 0; i < NUM_SUPPORTED_DIDS; i++)
            {
                // 2. 각 DID(uint16_t, 2바이트)를 상위/하위 바이트로 분리합니다.
                // 예: 0xF187 -> 상위 바이트 0xF1, 하위 바이트 0x87

                // 상위 바이트(High-byte)를 담습니다.
                responseMsg[responseLen++] = (uint8_t) (SUPPORTED_DIDS[i] >> 8);

                // 하위 바이트(Low-byte)를 담습니다.
                responseMsg[responseLen++] = (uint8_t) (SUPPORTED_DIDS[i] & 0xFF);
            }

            // 3. 모든 DID가 담긴 최종 응답 메시지를 전송합니다.
            CANTP_SendResponse(responseMsg, responseLen);
            break;
        }
            // ... (other string DIDs like serial number, VIN, etc.)

        case DID_ACTIVE_DIAGNOSTIC_SESSION :
        {
            responseMsg[responseLen++] = (uint8_t) session_getCurrent();
            CANTP_SendResponse(responseMsg, responseLen);
            break;
        }

        default :
            sendNegativeResponse(sid, UDS_NRC_REQUEST_OUT_OF_RANGE);
            break;
    }
}

/* ---- 0x2A: Read Data By Periodic Identifier Handler ---- */
void handleSID2A (const uint8_t *data, uint16_t length, const uint8_t sid)
{
    if (length < 2)
    {
        sendNegativeResponse(sid, UDS_NRC_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT);
        return;
    }

    uint8_t subFunction = data[1];
    // For simplicity, this handler assumes only one DID is sent at a time
    uint16_t requestedDid = (data[2] << 8) | data[3];

    int task_index = -1;
    for (int i = 0; i < MAX_PERIODIC_TASKS; i++)
    {
        if (g_periodicTasks[i].did == requestedDid)
        {
            task_index = i;
            break;
        }
    }

    if (task_index != -1)
    {
        uint8_t responseMsg[2] = {UDS_POSITIVE_RESPONSE_SID(sid), subFunction};
        switch (subFunction)
        {
            case 0x01 : // startSending
            case 0x02 : // slow
            case 0x03 : // medium
            case 0x04 : // fast
                g_periodicTasks[task_index].isActive = 1;
                g_periodicTasks[task_index].timer = g_periodicTasks[task_index].intervalMs; // Start immediately
                CANTP_SendResponse(responseMsg, sizeof(responseMsg));
                myPrintf("UDS: Start Periodic DID 0x%04X\n", requestedDid);
                // Activate STM1 if it was disabled
                if (MODULE_STM0.ICR.B.CMP1EN == 0)
                {
                    myPrintf("STM1 interrupt was disabled. Reactivating...\n");
                    MODULE_STM0.CMP[1].U = (uint32_t) (MODULE_STM0.TIM0.U + PERIODIC_TICKS);
                    MODULE_STM0.ISCR.B.CMP1IRR = 1U;
                    MODULE_STM0.ICR.B.CMP1EN = 1U;
                }
                break;
            case 0x00 : // stopSending
                g_periodicTasks[task_index].isActive = 0;
                CANTP_SendResponse(responseMsg, sizeof(responseMsg));
                myPrintf("UDS: Stop Periodic DID 0x%04X\n", requestedDid);
                break;
            default :
                sendNegativeResponse(sid, UDS_NRC_SUB_FUNCTION_NOT_SUPPORTED);
                break;
        }
    }
    else
    {
        sendNegativeResponse(sid, UDS_NRC_REQUEST_OUT_OF_RANGE);
    }
}

/* ---- 0x3E: Tester Present Handler ---- */
void handleSID3E (const uint8_t *data, uint16_t length, const uint8_t sid)
{
    // Implementation from original udshandler.c can be moved here
    uint8_t sub_function = data[1];
    if (sub_function == 0x00)
    { // response required
        uint8_t payload[] = {0x7E, 0x00};
        CANTP_SendResponse(payload, sizeof(payload));
    }
    // No response for sub_function 0x80
}

/* ---- Other SID Handlers (0x19, 0x2E, 0x31) ---- */
// Implementations for these can be moved from udshandler.c to here,
// changing isotp_send_response to CANTP_SendResponse.
void handleSID19 (const uint8_t *data, uint16_t length, const uint8_t sid)
{
}
void handleSID2E (const uint8_t *data, uint16_t length, const uint8_t sid)
{
}
void handleSID31 (const uint8_t *data, uint16_t length, const uint8_t sid)
{
}
