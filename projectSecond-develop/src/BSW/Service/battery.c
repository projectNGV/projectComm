#include "battery.h"
#include "IfxEvadc_Adc.h" // EVADC 관련 헤더
#include "IfxPort.h"      // 포트 관련 헤더

// --- ADC 관련 상수 정의 ---
#define ADC_GROUP IfxEvadc_GroupId_3 // 그룹 3 사용
#define ADC_CHANNEL 1              // 채널 1 사용 (P40.1)
#define ADC_MAX_VALUE 4095.0f
#define ADC_REFERENCE_VOLTAGE 3.3f

// --- 배터리 특성 상수 (사용하는 배터리에 맞게 수정 필요) ---
#define V_BATT_MAX 3.0f // 100%일 때 ADC 핀에 측정되는 전압
#define V_BATT_MIN 2.0f // 0%일 때 ADC 핀에 측정되는 전압

// --- 전역 변수 ---
static uint8 g_batterySoC = 0; // 계산된 최종 배터리 잔량(%)을 저장

// --- Private 함수 (내부용) ---
static unsigned int readAdcValue(void) {
    // (문서의 Evadc_readBattery 로직을 여기에 구현)
    // ...
    return 0; // 0 ~ 4095 사이의 값
}

static float convertAdcToVoltage(unsigned int adcValue) {
    // (문서의 convertAdcToVoltage 로직을 여기에 구현)
    return ((float)adcValue * ADC_REFERENCE_VOLTAGE) / ADC_MAX_VALUE;
}

static int calculateSoC(float measuredVoltage) {
    // (문서의 calculateBatteryPercentage 로직을 여기에 구현)
    if (measuredVoltage >= V_BATT_MAX) return 100;
    if (measuredVoltage <= V_BATT_MIN) return 0;
    return (int)(((measuredVoltage - V_BATT_MIN) / (V_BATT_MAX - V_BATT_MIN)) * 100.0f);
}

// --- Public 함수 (외부 공개용) ---
void battery_init(void) {
    // (문서의 Evadc_Init 로직을 여기에 구현)
    // P40.1 핀을 아날로그 입력으로 설정하고, EVADC 그룹 및 채널을 초기화합니다.
}

// 이 함수는 main 루프에서 주기적으로 호출되어야 합니다.
void battery_mainFunction(void) {
    unsigned int adcResult = readAdcValue();
    float batteryVoltage = convertAdcToVoltage(adcResult);
    g_batterySoC = calculateSoC(batteryVoltage);

    // (나중에 여기에 과전압/저전압 DTC 진단 로직을 추가할 수 있습니다)
}

uint8 battery_getSoC(void) {
    return g_batterySoC;
}
