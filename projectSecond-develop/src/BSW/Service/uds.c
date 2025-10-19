#include "uds.h"
#include "cantp.h"
#include "sidhandler.h"
#include "tof.h"
#include "ultrasonic.h"
#include <string.h>

/*********************************************************************************************************************/
/*-------------------------------------------------Global variables--------------------------------------------------*/
/*********************************************************************************************************************/
PeriodicTransmission g_periodicTasks[MAX_PERIODIC_TASKS] = {
    {.did = UDS_DID_TOF_SENSOR, .intervalMs = 200, .isActive = 0, .timer = 0},
    {.did = UDS_DID_LEFT_ULTRASONIC_SENSOR, .intervalMs = 500, .isActive = 0, .timer = 0},
    // Initialize other slots to 0
    {.did = 0, .intervalMs = 0, .isActive = 0, .timer = 0},
    {.did = 0, .intervalMs = 0, .isActive = 0, .timer = 0},
    {.did = 0, .intervalMs = 0, .isActive = 0, .timer = 0}
};

/*********************************************************************************************************************/
/*---------------------------------------------Function Implementations----------------------------------------------*/
/*********************************************************************************************************************/

void UDS_HandleMessage(const uint8_t *data, uint16_t length)
{
    if (length < 1) return;

    uint8_t sid = data[0];
    switch (sid)
    {
        case UDS_SERVICE_DIAGNOSTIC_SESSION_CONTROL: // 0x10
            handleSID10(data, length, sid);
            break;
        case UDS_SERVICE_READ_DATA_BY_IDENTIFIER: // 0x22
            handleSID22(data, length, sid);
            break;
        case UDS_SERVICE_READ_DATA_BY_PERIODIC_IDENTIFIER: // 0x2A
            handleSID2A(data, length, sid);
            break;
        case UDS_SERVICE_WRITE_DATA_BY_IDENTIFIER: // 0x2E
            handleSID2E(data, length, sid);
            break;
        case UDS_SERVICE_ROUTINE_CONTROL: // 0x31
            handleSID31(data, length, sid);
            break;
        case UDS_SERVICE_TESTER_PRESENT: // 0x3E
            handleSID3E(data, length, sid);
            break;
        case UDS_SERVICE_READ_DTC_INFORMATION: // 0x19
            handleSID19(data, length, sid);
            break;
        default:
            sendNegativeResponse(sid, UDS_NRC_SERVICE_NOT_SUPPORTED);
            break;
    }
}

void sendNegativeResponse(uint8_t sid, uint8_t nrc)
{
    uint8_t responseNRC[3];
    responseNRC[0] = 0x7F; // Negative Response SID
    responseNRC[1] = sid;  // Original SID
    responseNRC[2] = nrc;  // Negative Response Code
    CANTP_SendResponse(responseNRC, 3);
}


void UDS_HandlePeriodicTransmission(void)
{
    for (int i = 0; i < MAX_PERIODIC_TASKS; i++)
    {
        if (g_periodicTasks[i].isActive && g_periodicTasks[i].timer == 0)
        {
            switch (g_periodicTasks[i].did)
            {
                case UDS_DID_TOF_SENSOR:
                {
                    // ✨ FIX: SID(0x6A) + DID(2) + Data(3) = 6바이트
                    uint8_t payload[6];
                    payload[0] = UDS_POSITIVE_RESPONSE_SID(UDS_SERVICE_READ_DATA_BY_PERIODIC_IDENTIFIER); // 0x6A
                    payload[1] = (g_periodicTasks[i].did >> 8) & 0xFF;
                    payload[2] = g_periodicTasks[i].did & 0xFF;

                    uint32_t tofVal = tofGetValue();
                    payload[3] = (tofVal >> 16) & 0xFF;
                    payload[4] = (tofVal >> 8) & 0xFF;
                    payload[5] = tofVal & 0xFF;
                    CANTP_SendResponse(payload, sizeof(payload));
                    break;
                }
                case UDS_DID_LEFT_ULTRASONIC_SENSOR:
                {
                    // ✨ FIX: SID(0x6A) + DID(2) + Data(2) = 5바이트
                    uint8_t payload[5];
                    payload[0] = UDS_POSITIVE_RESPONSE_SID(UDS_SERVICE_READ_DATA_BY_PERIODIC_IDENTIFIER); // 0x6A
                    payload[1] = (g_periodicTasks[i].did >> 8) & 0xFF;
                    payload[2] = g_periodicTasks[i].did & 0xFF;

                    float distance_cm = ultrasonic_getDistanceCm(ULT_LEFT);
                    if (distance_cm < 0)
                    {
                        payload[3] = 0xFF;
                        payload[4] = 0xFF;
                    }
                    else
                    {
                        uint16_t scaled_distance = (uint16_t)(distance_cm * 10.0f);
                        payload[3] = (uint8_t)(scaled_distance >> 8);
                        payload[4] = (uint8_t)(scaled_distance & 0xFF);
                    }
                    CANTP_SendResponse(payload, sizeof(payload));
                    break;
                }
            }
            // 다음 전송을 위해 타이머 재설정
            g_periodicTasks[i].timer = g_periodicTasks[i].intervalMs;
        }
    }
}
