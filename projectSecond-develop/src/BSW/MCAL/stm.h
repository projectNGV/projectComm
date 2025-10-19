#ifndef BSW_DRIVER_STM_H_
#define BSW_DRIVER_STM_H_

#include "IfxStm.h"
#include "priority.h"

/*********************************************************************************************************************/
/*------------------------------------------------------Macros-------------------------------------------------------*/
/*********************************************************************************************************************/
#define TOF_INVALID_VALUE       0xFFFFFFFF  // ToF sensor error value
#define TOF_TIMEOUT_MS          150         // ToF message timeout (ms)

#define STM_CLOCK_HZ            100000000UL // Clock frequency 100MHz
#define PERIODIC_INTERVAL_MS    10          // Periodic task processing interval (ms)

#define TICKS_PER_MS            (STM_CLOCK_HZ / 1000UL)
#define PERIODIC_TICKS          (PERIODIC_INTERVAL_MS * TICKS_PER_MS)
#define TOF_TIMEOUT_TICKS       (TOF_TIMEOUT_MS * TICKS_PER_MS)

/*********************************************************************************************************************/
/*-----------------------------------------------Function Prototypes-------------------------------------------------*/
/*********************************************************************************************************************/
void stmInit(void);
void resetTofTimeoutTimer(void);

#endif /* BSW_DRIVER_STM_H_ */
