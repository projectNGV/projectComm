#include "fsm.h"

/*********************************************************************************************************************/
/*-------------------------------------------------- Global Variables ------------------------------------------------*/
/*********************************************************************************************************************/

extern volatile bool aebEnableFlag; // AEB(긴급 제동) 상태 여부 — 센서(AEB_DISTANCE_MM 이하)에서 위험을 감지하면 TRUE
volatile bool buzzerFlag = TRUE; // 긴급 정지 시 부저를 1회만 울리기 위한 내부 제어 플래그

// 현재 차량 FSM 상태 (초기값: 대기 상태)
VehicleState currentState = STATE_IDLE;

/*********************************************************************************************************************/
/*-------------------------------------------- Finite State Machine (FSM) --------------------------------------------*/
/*********************************************************************************************************************/
/**
 * @brief  차량의 주행 상태를 관리하는 메인 FSM
 *
 * @param  motorState : 현재 모터 상태 구조체 포인터
 *
 * 상태 전이 개요
 * ────────────────────────────────────────────────
 * STATE_IDLE          : 대기 상태 (입력 또는 명령 대기)
 * STATE_MANUAL_DRIVE  : SOA 명령 기반 수동 주행
 * STATE_EMERGENCY_STOP: AEB 감지 시 긴급 정지
 * STATE_AUTO_PARK     : 자동 주차 기능 수행
 */
void handleStateMachine(MotorState *motorState)
{
    /***********************************************
     * 암호 기반 시동
     ***********************************************/
    if (!g_isLogin) {
        currentState = STATE_IDLE;   // 로그인 전에는 항상 대기 상태
        motorStop();                 // PWM, 방향 모두 정지
        ledStartBlinking(LED_BOTH);  // 로그인 요청 시 시각적 경고 (비밀번호 입력 요구)
        return;                      // FSM 로직 중단
    }
    ledStopAll();

    /***********************************************
     * TOF 거리 기반 AEB 상태 갱신
     ***********************************************/
    unsigned int distance = tofGetValue();   // 전방 거리(mm) 측정

    // AEB 기능이 켜져 있을 때만 거리 기반으로 작동 상태 업데이트
    if (aebEnableFlag)
        updateAebFlagByTof(distance);
    else
        motorState->aebActiveFlag = FALSE;   // 기능이 꺼져 있으면 작동 안 함

    /***********************************************
     * FSM 상태에 따른 분기 처리
     ***********************************************/
    switch (currentState)
    {
        /* ───────────────────────────────
         * [STATE_IDLE]
         * - 차량 정지 상태
         * - 외부 명령(SOA) 대기
         * - Drive 또는 AutoPark 명령에 의해 전이됨
         * ─────────────────────────────── */
        case STATE_IDLE:
            // 1~9 방향 명령이 들어오면 수동 주행 상태로 전환
            if (motorState->currentDir >= 0x01 && motorState->currentDir <= 0x09)
            {
                currentState = STATE_MANUAL_DRIVE;
            }
            // AutoPark 명령이 활성화되면 자동 주차 상태로 전환
            else if (motorState->autoParkFlag == TRUE)
            {
                currentState = STATE_AUTO_PARK;
            }
            break;

        /* ───────────────────────────────
         * [STATE_MANUAL_DRIVE]
         * - SOA 명령 기반의 수동 주행 상태
         * - AEB 조건 발생 시 긴급정지로 전환
         * ─────────────────────────────── */
        case STATE_MANUAL_DRIVE:
            // 전방 장애물 감지 (AEB 발동) && 후진 명령이 아닐 경우 → 긴급 정지 상태로 전환
            if (motorState->aebActiveFlag &&
                !(motorState->currentDir == 0x01 || motorState->currentDir == 0x02 || motorState->currentDir == 0x03))
            {
                currentState = STATE_EMERGENCY_STOP;
            }
            else
            {
                // AEB 비활성화 or 후진 명령 → 정상 주행
                motorRunCommand(motorState);
            }
            break;

        /* ───────────────────────────────
         * [STATE_EMERGENCY_STOP]
         * - AEB가 작동하여 차량이 강제 정지된 상태
         * - 부저 및 LED 경고 출력
         * - 장애물 해제 또는 후진 명령 시 복귀
         * ─────────────────────────────── */
        case STATE_EMERGENCY_STOP:
            performEmergencyStop();  // 모터 정지 및 역방향 감속

            // 1회성 경고음 및 점멸 표시
            if (buzzerFlag)
            {
                emergencyBuzzer();     // 경고음 출력
                ledStartBlinking(LED_BOTH); // 양쪽 방향등 점멸
                buzzerFlag = FALSE;    // 중복 방지
            }

            // 후진 명령(1,2,3) 또는 AEB 해제 시 복귀
            if ((motorState->currentDir == 0x01 || motorState->currentDir == 0x02 || motorState->currentDir == 0x03) ||
                !motorState->aebActiveFlag)
            {
                buzzerFlag = TRUE;     // 다음 긴급정지 시 부저 재활성화
                ledStopAll();
                currentState = STATE_MANUAL_DRIVE;
            }
            break;

        /* ───────────────────────────────
         * [STATE_AUTO_PARK]
         * - 자동 주차 알고리즘 실행
         * - 완료 후 IDLE로 복귀
         * ─────────────────────────────── */
        case STATE_AUTO_PARK:
            autoPark();                  // 자동 주차 수행 (장애물 회피, 후진 포함)
            motorState->autoParkFlag = FALSE; // 명령 완료 후 플래그 해제
            currentState = STATE_IDLE;   // 대기 상태로 복귀
            break;
    }
}
