#include "main0.h"

// 모터 상태를 저장하는 구조체 변수 선언 및 초기화
MotorState motorState = {
    .currentDuty = 0,       // 현재 모터 Duty (정지 상태로 시작)
    .currentDir = '5',      // 초기 방향 (정지 상태)
    .aebActiveFlag = false, // AEB 비활성화 상태로 시작
    .autoParkFlag = false   // AutoPark 비활성화 상태로 시작
};

void main0 (void)
{
    systemInit();

    unsigned char tx[2] = { 0x01, 0x01 }; // 예: STATUS=AEB_STATE(0x01), ON(0x01)
    canSendMsg(0x310, tx, 2);

    while (1)
    {
//        myPrintf("TOF : %d mm\n", tofGetValue());
        // 현재 상태에 따라 차량의 동작을 제어하는 상태 머신 처리
        // 사용자의 키 입력, 센서 값 등에 따라 상태를 변경하고 그에 맞는 행동 수행
        handleStateMachine(&motorState);
    }

}
