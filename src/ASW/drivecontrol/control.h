
#ifndef ASW_CONTROL_H_
#define ASW_CONTROL_H_

#include "motor.h"
#include "led.h"
#include "stdbool.h"

#define Forward 1
#define Backward 0

typedef struct {
    int currentDuty;            // 현재 PWM Duty
    int currentDir;            // 현재 주행 방향 ('8': 전진, '2': 후진 등)
    bool aebActiveFlag;         // AEB 기능 활성화 여부 (TRUE: AEB 작동 중)
    bool autoParkFlag;          // AutoPark 기능 요청 여부
} MotorState;

// 다른 모듈에서 접근 가능하도록 extern 선언
extern MotorState motorState;

void moveForward(int duty);
void moveBackward(int duty);
void turnLeftInPlace(int duty);
void turnRightInPlace(int duty);
void moveForwardLeft(int duty);
void moveForwardRight(int duty);
void moveBackwardkLeft(int duty);
void moveBackwardRight(int duty);

void motorRunCommand (MotorState* state);

#endif /* ASW_CONTROL_H_ */
