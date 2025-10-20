#include "control.h"

/*********************************************************************************************************************/
/*--------------------------------------------- 모터 제어 함수 정의 --------------------------------------------------*/
/*********************************************************************************************************************/

// 8: 전진 (양쪽 정방향)
void moveForward(int duty)
{
    motorMovChAPwm(duty, Forward);
    motorMovChBPwm(duty, Forward);
}

// 2: 후진 (양쪽 역방향)
void moveBackward(int duty)
{
    motorMovChAPwm(duty, Backward);
    motorMovChBPwm(duty, Backward);
}

// 4: 제자리 좌회전 (좌측 역방향, 우측 정방향)
void turnLeftInPlace(int duty)
{
    motorMovChAPwm(duty, Backward);
    motorMovChBPwm(duty, Forward);
}

// 6: 제자리 우회전 (좌측 정방향, 우측 역방향)
void turnRightInPlace(int duty)
{
    motorMovChAPwm(duty, Forward);
    motorMovChBPwm(duty, Backward);
}

// 7: 전진+좌회전
void moveForwardLeft(int duty)
{
    motorMovChBPwm(duty, Forward);
    motorMovChAPwm(duty / 3, Forward);
}

// 9: 전진+우회전
void moveForwardRight(int duty)
{
    motorMovChAPwm(duty, Forward);
    motorMovChBPwm(duty / 3, Forward);
}

// 1: 후진+좌회전
void moveBackwardLeft(int duty)
{
    motorMovChBPwm(duty, Backward);
    motorStopChA();
}

// 3: 후진+우회전
void moveBackwardRight(int duty)
{
    motorMovChAPwm(duty, Backward);
    motorStopChB();
}

/*********************************************************************************************************************/
/*----------------------------------------- 모터 실행 명령 (SOA 기반) ----------------------------------------------*/
/*********************************************************************************************************************/
/**
 * @brief 현재 motorState 상태에 따라 실제 모터 구동
 * @param state : 모터 상태 구조체 포인터
 *
 * currentDir : SOA 명령으로 설정된 주행 방향 (1~9)
 * currentDuty: SOA 명령으로 설정된 속도
 */
void motorRunCommand(MotorState *state)
{
    int dir = state->currentDir;
    int duty = state->currentDuty;

    // STOP 또는 duty=0이면 바로 정지
    if (dir == 0x05 || duty <= 0)
    {
        motorStop();
        ledStopAll();
        return;
    }

    switch (dir)
    {
        case 0x08:  // 전진
            ledStopAll();
            moveForward(duty);
            break;

        case 0x02:  // 후진
            ledStopAll();
            moveBackward(duty);
            break;

        case 0x04:  // 좌회전
            turnLeftInPlace(duty);
            ledSetRight(0);
            ledStartBlinking(LED_LEFT);
            break;

        case 0x06:  // 우회전
            turnRightInPlace(duty);
            ledSetLeft(0);
            ledStartBlinking(LED_RIGHT);
            break;

        case 0x07:  // 전진+좌회전
            moveForwardLeft(duty);
            ledSetRight(0);
            ledStartBlinking(LED_LEFT);
            break;

        case 0x09:  // 전진+우회전
            moveForwardRight(duty);
            ledSetLeft(0);
            ledStartBlinking(LED_RIGHT);
            break;

        case 0x01:  // 후진+좌회전
            ledStopAll();
            moveBackwardLeft(duty);
            break;

        case 0x03:  // 후진+우회전
            ledStopAll();
            moveBackwardRight(duty);
            break;


        default:
            motorStop();
            ledStopAll();
            break;
    }
}
