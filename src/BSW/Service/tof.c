#include "tof.h"
#include "stm.h" //resetTofTimeoutTimer()


unsigned int g_TofValue;             // 현재 측정된 거리(mm)
volatile bool aebEnableFlag = false;        // AEB 기능 ON/OFF 플래그

void tofInit(void)
{
    canInit(BD_500K, CAN_NODE0);            // CAN 초기화
    canRegisterTofCallback(tofUpdateFromCAN);
    g_TofValue = TOF_DEFAULT_VALUE_MM;      // 초기 거리값 설정
    aebEnableFlag = false;                  // 기본: AEB 기능 비활성화
}

/* CAN으로부터 거리 데이터 수신 시 호출됨 */
void tofUpdateFromCAN(unsigned char *rxData)
{
    if (rxData == NULL) return;

    unsigned short signal_strength = (rxData[5] << 8) | rxData[4];

    if (signal_strength != 0)
    {
        // 거리(mm) 계산
        g_TofValue = (rxData[2] << 16) | (rxData[1] << 8) | rxData[0];

        // AEB 상태 갱신 (거리 기반)
        updateAebFlagByTof(g_TofValue);

        resetTofTimeoutTimer();
    }
}

/* 현재 거리값(mm)을 반환 */
unsigned int tofGetValue(void)
{
    return g_TofValue;
}
