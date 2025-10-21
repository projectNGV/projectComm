#include "soa_publisher.h"

static bool prevAebState;
static unsigned char prevAutoparkState;
static bool prevAuthState;
static unsigned int prevTofValue;   // mm 단위 (on-change 최소 감도 적용용)


void soaPublisherInit(void)
{
    prevAebState = false;
    prevAutoparkState = 0x00;
    prevAuthState = false;
    prevTofValue = 0;
}

/* ============================================================
 * @brief AEB_STATE 변경 시만 송신
 * ============================================================ */
void sendAebStateIfChanged(void)
{
    if (prevAebState != motorState.aebActiveFlag) {
        unsigned char txData[2] = {STATUS_AEB_STATE, motorState.aebActiveFlag ? 0x01 : 0x00};
        canSendMsg(CAN_SOA_STATUS_ID, txData, 2);
        prevAebState = motorState.aebActiveFlag;
    }
}

/* ============================================================
 * @brief AUTOPARK_STATE 변경 시만 송신
 * ============================================================ */
void sendAutoparkStateIfChanged(unsigned char state)
{
    if (prevAutoparkState != state) {
        unsigned char txData[2] = {STATUS_AUTOPARK_STATE, state};
        canSendMsg(CAN_SOA_STATUS_ID, txData, 2);
        prevAutoparkState = state;
    }
}

/* ============================================================
 * @brief AUTH_STATE 변경 시만 송신
 * ============================================================ */
void sendAuthStateIfChanged(bool newState)
{
    if (prevAuthState != newState)
    {
        unsigned char txData[2] = {STATUS_AUTH_STATE, newState ? 0x01 : 0x00};
        canSendMsg(CAN_SOA_STATUS_ID, txData, 2);
        prevAuthState = newState;
    }
}

/* ============================================================
 * @brief TOF_DISTANCE 주기 송신 (500ms마다 호출)
 * @note   단, 값이 일정 범위 이상 변하면 즉시 송신 (on-change 혼합)
 * ============================================================ */
void sendTofDistancePeriodic(void)
{
    unsigned int tof = tofGetValue(); // mm 단위
    unsigned int diff = (prevTofValue > tof) ? (prevTofValue - tof) : (tof - prevTofValue);

    // 너무 미세한 변화는 송신하지 않음 (5mm 이하 변화 무시)
    if (diff < 5)
        return;

    // big-endian 변환
    unsigned char txData[4] = {
        STATUS_TOF_DISTANCE,
        (unsigned char)((tof >> 16) & 0xFF),
        (unsigned char)((tof >> 8) & 0xFF),
        (unsigned char)(tof & 0xFF)
    };

    canSendMsg(CAN_SOA_STATUS_ID, txData, 4);
    prevTofValue = tof;

//    myPrintf("[PUB] TOF_DISTANCE = %u mm\n", tof);
}
