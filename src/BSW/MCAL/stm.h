#ifndef BSW_DRIVER_STM_H_
#define BSW_DRIVER_STM_H_

#include "IfxStm.h"
#include "soa_publisher.h"
#include "priority.h"

#define STM_TICK_500MS   100000000ULL   // 100 MHz 기준 500ms (0.5초)

void stm1IsrHandler(void);
void stmInterruptInit(void);


#endif /* BSW_DRIVER_STM_H_ */
