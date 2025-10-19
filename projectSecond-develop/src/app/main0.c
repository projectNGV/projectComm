#include "main0.h"

// 모터 상태를 저장하는 구조체 변수 선언 및 초기화
MotorState motorState = {.baseDuty = 50,      // 기본 주행 속도(Duty 비율), 사용자가 설정한 값 (0~100%)
        .currentDuty = 0,    // 현재 실제 적용 중인 Duty 값 (초기에는 정지)
        .currentDir = '5',   // 현재 주행 방향 (초기에는 정지)
        .prevDir = '5',      // 바로 직전 주행 방향 (초기에는 정지)
        .lastKeyInput = '5'  // 가장 마지막으로 입력된 키 (초기에는 정지)
        };

void main0 (void)
{
    systemInit();
    session_init();
    // 사용자 인증 절차 실행(암호 기반 시동)
    // authenticate();

    // main.c의 while(1) 루프
    while (1)
    {
        // ... 기존 코드
        handleStateMachine(&motorState);
        diagnoseTofSensor();
        diagnoseUltrasonicSensor();

        // ✨ 추가: 주기적 UDS 전송 핸들러 호출
        UDS_HandlePeriodicTransmission();

        IfxScuWdt_serviceCpuWatchdog(IfxScuWdt_getCpuWatchdogPassword());
    }
}


