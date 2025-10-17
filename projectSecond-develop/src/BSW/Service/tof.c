#include "tof.h"
#include "dtc.h" // DTC 모듈의 함수(dtc_updateStatus)를 사용하기 위해 헤더를 포함합니다.
#include "Ifx_Types.h" // bool, uint64 등 표준 타입 사용
#include "config.h"
/* --- 전역 변수 --- */
static unsigned int g_TofValue;
volatile bool tofFlag = false;
static uint64 g_lastTofMessageTime = 0; // 마지막 CAN 메시지 수신 시간을 기록할 변수

/* --- 함수 구현 --- */

void tofInit (void)
{
    canInit(BD_500K, CAN_NODE0);
    canRegisterTofCallback(tofUpdateFromCAN);
    g_TofValue = TOF_DEFAULT_VALUE_MM;
    tofFlag = false;
    g_lastTofMessageTime = getTime10Ns(); // 부팅 시 현재 시간으로 초기화
}

void tofOnOff (void)
{
    // --- ✨ 새로 추가된 부분: 마스터 스위치 확인 ---
    // OTA로 AEB 기능 자체가 비활성화되었다면,
    // 사용자가 't'키를 눌러도 센서를 켤 수 없도록 막습니다.
    if (g_config.isAebEnabled == false)
    {
        tofFlag = false; // 사용자 스위치도 강제로 OFF 상태로 유지
        return;          // 함수 즉시 종료
    }

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
    // 1. CAN 메시지를 수신할 때마다 현재 시간을 기록합니다 (타임아웃 감지용).
    g_lastTofMessageTime = getTime10Ns();

    if (rxData == NULL)
        return;

    unsigned short signal_strength = rxData[5] << 8 | rxData[4];

    if (signal_strength != 0)
    {
        g_TofValue = rxData[2] << 16 | rxData[1] << 8 | rxData[0];
        updateAebFlagByTof(g_TofValue);
    }
}

unsigned int tofGetValue (void)
{
    return g_TofValue;
}

/*
 ==========================================================================
 고장 진단 함수 (이 파일의 핵심적인 추가 기능)
 ==========================================================================
 */
// 이 함수는 main.c의 while(1) 루프 등에서 주기적으로 호출되어야 합니다.
void diagnoseTofSensor (void)
{
    // --- 진단 항목 1: 통신 타임아웃 (연결 해제) 검사 ---
    // 1초(100,000,000 * 10ns) 이상 메시지가 없으면 고장으로 판단합니다.
    bool isTimeout = (getTime10Ns() - g_lastTofMessageTime > 100000000);

    // 진단 결과를 '의무기록사(dtc.c)'에게 전달하여 기록을 요청합니다.
    dtc_updateStatus(DTC_TOF_TIMEOUT, isTimeout);

    // --- 진단 항목 2: 측정값 범위 초과 검사 ---
    // 값이 2000mm를 초과하면 고장으로 판단합니다.
    bool isOutOfRange = (g_TofValue > 2000);

    // 진단 결과를 '의무기록사(dtc.c)'에게 전달하여 기록을 요청합니다.
    dtc_updateStatus(DTC_TOF_OUTOFRANGE, isOutOfRange);
}
