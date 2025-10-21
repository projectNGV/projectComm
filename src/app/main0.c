#include "main0.h"

// 모터 상태를 저장하는 구조체 변수 선언 및 초기화
MotorState motorState = {
    .currentDuty = 0,       // 현재 모터 Duty (정지 상태로 시작)
    .currentDir = 0x05,      // 초기 방향 (정지 상태)
    .aebActiveFlag = false, // AEB 비활성화 상태로 시작
    .autoParkFlag = false   // AutoPark 비활성화 상태로 시작
};

void main0 (void)
{
    systemInit();

    delayMs(1000);
    while (1)
    {
        // 현재 상태에 따라 차량의 동작을 제어하는 상태 머신 처리
        // 사용자의 키 입력, 센서 값 등에 따라 상태를 변경하고 그에 맞는 행동 수행
        handleStateMachine(&motorState);

        CANTP_MainFunction();
//        diagnoseUltrasonicSensor();
        UDS_HandlePeriodicTransmission(); // 주기적 전송 처리 함수 호출
    }

}
