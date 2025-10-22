#include "tof.h"
#include "stm.h" //resetTofTimeoutTimer()
#include "dtc.h"


unsigned int g_TofValue;
volatile bool tofFlag = false;
static uint64 g_lastTofMessageTime = 0; // 마지막 CAN 메시지 수신 시간을 기록할 변수


void tofInit (void)
{
    canInit(BD_500K, CAN_NODE0);
    canRegisterTofCallback(tofUpdateFromCAN);
    g_TofValue = TOF_DEFAULT_VALUE_MM;
    tofFlag = false;
    g_lastTofMessageTime = getTime10Ns();
}

void tofOnOff(void)
{
    if (tofFlag)
    {
        tofFlag = false;
    }
    else
    {
        tofFlag = true;
    }
}

void tofUpdateFromCAN (unsigned char *rxData)
{
    if (rxData == NULL) return;

    unsigned short signal_strength = rxData[5] << 8 | rxData[4];

    if (signal_strength != 0)
    {
        g_TofValue = rxData[2] << 16 | rxData[1] << 8 | rxData[0];
        updateAebFlagByTof(g_TofValue);

        resetTofTimeoutTimer();
    }
}

unsigned int tofGetValue (void)
{
    return g_TofValue;
}


void diagnoseTofSensor (void)
{
    // --- 진단 항목 1: 통신 타임아웃 (연결 해제) 검사 ---
    // 1초(100,000,000 * 10ns) 이상 메시지가 없으면 고장으로 판단합니다.
    bool isTimeout = (getTime10Ns() - g_lastTofMessageTime > 100000000);

    // 진단 결과를 '의무기록사(dtc.c)'에게 전달하여 기록을 요청합니다.
    dtc_updateStatus(UDS_DTC_TOF_TIMEOUT, isTimeout);

    // --- 진단 항목 2: 측정값 범위 초과 검사 ---
    // 값이 2000mm를 초과하면 고장으로 판단합니다.
    bool isOutOfRange = (g_TofValue > 2000);

    // 진단 결과를 '의무기록사(dtc.c)'에게 전달하여 기록을 요청합니다.
    dtc_updateStatus(UDS_DTC_TOF_OUTOFRANGE, isOutOfRange);
}
