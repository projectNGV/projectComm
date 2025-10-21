#ifndef BSW_SERVICE_TOF_H_
#define BSW_SERVICE_TOF_H_

#include "can.h"
#include "motor.h"
#include "util.h"
#include "stdbool.h"
#include "IfxSrc_reg.h"
#include "aeb.h"

#define TOF_DEFAULT_VALUE_MM   5000  // 초기 거리값 (센서 미활성 시 기본값)

extern volatile bool aebEnableFlag;  // AEB 기능 활성화 여부
extern unsigned int g_TofValue;

void tofInit(void);
void tofUpdateFromCAN(unsigned char *rxData);
unsigned int tofGetValue(void);

#endif /* BSW_SERVICE_TOF_H_ */
