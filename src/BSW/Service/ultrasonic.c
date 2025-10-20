#include "ultrasonic.h"
#include "stdbool.h"
// 최근 5개 데이터 평균
#define FILTER_BUFFER_SIZE 5

// --- 수정 1: 배열 전체를 0으로 올바르게 초기화 ---
static float distance_buffers[ULT_SENSORS_NUM][FILTER_BUFFER_SIZE] = {{0}};
static int buffer_indexes[ULT_SENSORS_NUM] = {0};

// --- 추가: 필터 안정화를 위한 플래그 ---
// 필터가 처음 유효한 값으로 채워졌는지 여부를 확인

static bool is_filter_primed[ULT_SENSORS_NUM] = {false};


float ultrasonic_getDistanceFiltered(UltraDir dir)
{
    // 1. 새로운 거리 값을 측정 (범위 제한이 포함된 함수 호출)
    float new_distance = ultrasonic_getDistanceCm(dir);

    // 2. 유효한 값(0 이상)일 경우에만 필터 로직 수행
    if (new_distance >= 0)
    {
        // --- 수정 2: 필터 안정화 로직 ---
        // 만약 처음으로 유효한 값이 들어왔다면,
        // 버퍼 전체를 이 첫 값으로 채워서 평균이 급격하게 변하는 것을 방지합니다.
        if (is_filter_primed[dir] == false)
        {
            for (int i = 0; i < FILTER_BUFFER_SIZE; i++) {
                distance_buffers[dir][i] = new_distance;
            }
            is_filter_primed[dir] = true;
        }

        // 버퍼의 다음 위치에 새로운 값을 덮어씁니다 (순환 방식).
        distance_buffers[dir][buffer_indexes[dir]] = new_distance;
        buffer_indexes[dir] = (buffer_indexes[dir] + 1) % FILTER_BUFFER_SIZE;
    }

    // 3. 버퍼에 있는 모든 값의 합계를 구합니다.
    float sum = 0;
    for (int i = 0; i < FILTER_BUFFER_SIZE; i++)
    {
        sum += distance_buffers[dir][i];
    }

    // 4. 합계를 버퍼 크기로 나누어 최종 평균값을 반환합니다.
    return sum / FILTER_BUFFER_SIZE;
}

const UltPin ULT_PINS[ULT_SENSORS_NUM] = {
        // shiled 씌우고 테스트하느라 잠시 ULT_LEFT의 핀을 trig = 13.2, echo = 13.1로 바꾸겠다
        //[ULT_LEFT] = {.trigger = {&MODULE_P13, 2}, .echo = {&MODULE_P13, 1}},

        [ULT_LEFT] = {.trigger = {&MODULE_P15, 2}, .echo = {&MODULE_P15, 3}},
        [ULT_RIGHT] = {.trigger = {&MODULE_P10, 5}, .echo = {&MODULE_P02, 4}},
        [ULT_REAR] = {.trigger = {&MODULE_P02, 5}, .echo = {&MODULE_P02, 3}}
};

void ultrasonicInit(void)
{
    for (int i = 0; i < ULT_SENSORS_NUM; i++)
    {
        IfxPort_setPinMode(ULT_PINS[i].trigger.port, ULT_PINS[i].trigger.pinIndex, IfxPort_Mode_outputPushPullGeneral);
        IfxPort_setPinMode(ULT_PINS[i].echo.port, ULT_PINS[i].echo.pinIndex, IfxPort_Mode_inputPullUp);
    }
}



static void sendTrigger(UltraDir dir)
{
    IfxPort_setPinState(ULT_PINS[dir].trigger.port, ULT_PINS[dir].trigger.pinIndex, IfxPort_State_high);
    delayUs(10);
    IfxPort_setPinState(ULT_PINS[dir].trigger.port, ULT_PINS[dir].trigger.pinIndex, IfxPort_State_low);

}


// --- ✨ 새로 추가된 함수 ---
// getDistanceByUltra를 호출하여 받은 raw time 값을 cm 단위로 변환하여 반환
float ultrasonic_getDistanceCm(UltraDir dir)
{
    // 1. 기존 함수를 호출하여 펄스 시간(10ns 단위)을 가져옵니다.
    int duration_10ns = getDistanceByUltra(dir);

    // 2. 만약 측정에 실패했다면(-1), 에러를 그대로 반환합니다.
    if (duration_10ns < 0)
    {
        return -1.0f;
    }

    // 3. 10ns 단위의 시간을 us 단위로 변환합니다.
    float duration_us = (float)duration_10ns / 100.0f;

    // 4. 거리 계산 공식을 적용합니다.
    float distance_cm = (duration_us * 0.0343f) / 2.0f;

    // 이상치 처리 로직 추가
    // 센서의 유효 측정 범위(예 : 2cm ~ 400cm) 벗어나면 에러 값 반환
    if (distance_cm < 2.0f || distance_cm > 400.0f) {
        return -2.0f; // -1.0f는 타임아웃, -2.0f는 범위 초과 에러로 구분
    }

    // 5. 최종 거리(cm) 값을 반환합니다.
    return distance_cm;
}


int getDistanceByUltra(UltraDir dir)
{
    uint64 start, timeOut;
    sendTrigger(dir);

    timeOut = getTime10Ns();
    timeOut += 10000000; // 100ms

    while(!IfxPort_getPinState(ULT_PINS[dir].echo.port, ULT_PINS[dir].echo.pinIndex)){
        if(getTime10Ns() > timeOut) return -1;
    }
    start = getTime10Ns();
    timeOut = start + 10000000;

    while(IfxPort_getPinState(ULT_PINS[dir].echo.port, ULT_PINS[dir].echo.pinIndex)){
        if(getTime10Ns() > timeOut) return -1;
    }

    int res = (int) (getTime10Ns() - start);
    if(res < 0) return -1;
    return res;
}

void diagnoseUltrasonicSensor(void) {
    // 좌측 센서 진단
    // cm 단위 거리 값 가져옴 (-1 : 신호 없음, -2 : 범위 초과)
    float distance_left = ultrasonic_getDistanceCm(ULT_LEFT);
    float distance_right = ultrasonic_getDistanceCm(ULT_RIGHT);
    float distance_rear = ultrasonic_getDistanceCm(ULT_REAR);


    // 1. 신호/회로 고장 진단 (연결 해제)
    // ultrasonic_getDistanceCm이 -1.0f 반환했는지 확인
    bool isSignalFault_left = (distance_left == -1.0f);
    dtc_updateStatus(UDS_DTC_ULT_LEFT_TIMEOUT, isSignalFault_left);

    bool isSignalFault_right = (distance_right == -1.0f);
    dtc_updateStatus(UDS_DTC_ULT_RIGHT_TIMEOUT, isSignalFault_right);

    bool isSignalFault_rear = (distance_rear == -1.0f);
    dtc_updateStatus(UDS_DTC_ULT_REAR_TIMEOUT, isSignalFault_rear);


    // 2. 측정값 범위 고장 진단
    // ultrasonic_getDistanceCm이 -2.0f 반환했는지 확인
    bool isRangeFault_left = (distance_left == -2.0f);
    dtc_updateStatus(UDS_DTC_ULT_LEFT_OUTOFRANGE, isRangeFault_left);

    bool isRangeFault_right = (distance_right == -2.0f);
    dtc_updateStatus(UDS_DTC_ULT_RIGHT_OUTOFRANGE, isRangeFault_right);

    bool isRangeFault_rear = (distance_rear == -2.0f);
    dtc_updateStatus(UDS_DTC_ULT_REAR_OUTOFRANGE, isRangeFault_rear);
}
