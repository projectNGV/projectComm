#ifndef BSW_SERVICE_BATTERY_H_
#define BSW_SERVICE_BATTERY_H_

#include "Ifx_Types.h"

// 1. 배터리 잔량을 조회하기 위한 DID 정의
#define DID_BATTERY_SOC 0x1010

// 2. 외부에서 사용할 함수 선언
void battery_init(void);               // EVADC 초기화
void battery_mainFunction(void);       // 주기적으로 배터리 상태를 읽고 업데이트하는 함수
uint8 battery_getSoC(void);            // 현재 배터리 잔량(%)을 반환하는 함수

#endif /* BSW_SERVICE_BATTERY_H_ */
