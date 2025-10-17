#include "routine_control.h"
#include "motor.h" // motorMoveForward 등 함수 사용을 위해 포함
#include "util.h"  // delayMs 함수 사용을 위해 포함

// --- 내부 테스트 스크립트 함수들 (static) ---

// RID 0x0201에 해당하는 스크립트
static void runMotorForwardTest(void)
{
    // 30% 듀티(300)로 5초(5000ms)간 정회전
    motorMoveForward(300);
    delayMs(5000);
    motorStop();
}

// RID 0x0202에 해당하는 스크립트
static void runMotorReverseTest(void)
{
    // 30% 듀티(300)로 5초(5000ms)간 역회전
    motorMoveReverse(300);
    delayMs(5000);
    motorStop();
}

// --- 외부 공개 함수 ---
// udsHandler가 이 함수를 호출합니다.
void startRoutine(uint16 rid)
{
    // RID 메뉴판에 따라 적절한 테스트 스크립트를 실행합니다.
    switch (rid)
    {
        case RID_MOTOR_FORWARD_TEST:
            runMotorForwardTest();
            break;

        case RID_MOTOR_REVERSE_TEST:
            runMotorReverseTest();
            break;

        default:
            // 지원하지 않는 RID인 경우 아무것도 하지 않음
            break;
    }
}
