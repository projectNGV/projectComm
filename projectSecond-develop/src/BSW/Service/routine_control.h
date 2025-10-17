#ifndef ROUTINE_CONTROL_H_
#define ROUTINE_CONTROL_H_

#include "Ifx_Types.h"

// 1. RID (Routine Identifier) 정의
#define RID_MOTOR_FORWARD_TEST 0x0001
#define RID_MOTOR_REVERSE_TEST 0x0002

// 2. 외부에서 호출할 함수 선언
// 이 함수는 udsHandler가 호출하며, RID에 맞는 루틴을 실행합니다.
void startRoutine(uint16 rid);

#endif /* ROUTINE_CONTROL_H_ */
