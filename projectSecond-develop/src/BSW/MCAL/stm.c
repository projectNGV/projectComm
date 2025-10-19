#include "stm.h"
#include "uds.h"
#include "tof.h"


/*********************************************************************************************************************/
/*---------------------------------------------Function Implementations----------------------------------------------*/
/*********************************************************************************************************************/

IFX_INTERRUPT(stm0IsrHandler, 0, ISR_PRIORITY_STM0);
void stm0IsrHandler(void)
{
    // ToF Timeout occurred
    g_TofValue = TOF_INVALID_VALUE;
    // Disable timeout interrupt until the next message is received
    MODULE_STM0.ICR.B.CMP0EN = 0;
}

IFX_INTERRUPT(stm1IsrHandler, 0, ISR_PRIORITY_STM1);
void stm1IsrHandler(void)
{
    boolean anyTaskActive = FALSE;

    // Decrement timers for active periodic tasks
    for (int i = 0; i < MAX_PERIODIC_TASKS; i++)
    {
        if (g_periodicTasks[i].isActive)
        {
            anyTaskActive = TRUE;
            if (g_periodicTasks[i].timer > 0)
            {
                if (g_periodicTasks[i].timer <= PERIODIC_INTERVAL_MS)
                {
                    g_periodicTasks[i].timer = 0;
                }
                else
                {
                    g_periodicTasks[i].timer -= PERIODIC_INTERVAL_MS;
                }
            }
        }
    }

    if (anyTaskActive)
    {
        // Schedule the next interrupt
        MODULE_STM0.CMP[1].U += PERIODIC_TICKS;
        MODULE_STM0.ISCR.B.CMP1IRR = 1U; // Clear interrupt flag
    }
    else
    {
        // No active tasks, disable the interrupt
        MODULE_STM0.ICR.B.CMP1EN = 0U;
    }
}

void stmInit(void)
{
    // CMP0 for ToF sensor timeout
    STM0_CMCON.B.MSIZE0 = 31;
    STM0_CMCON.B.MSTART0 = 0;
    MODULE_STM0.ICR.B.CMP0OS = 0; // Interrupt to STMIR0
    MODULE_SRC.STM.STM[0].SR[0].B.TOS = 0; // CPU0
    MODULE_SRC.STM.STM[0].SR[0].B.SRPN = ISR_PRIORITY_STM0;
    MODULE_SRC.STM.STM[0].SR[0].B.CLRR = 1;
    MODULE_SRC.STM.STM[0].SR[0].B.SRE = 1;
    MODULE_STM0.ISCR.B.CMP0IRR = 1U;
    MODULE_STM0.ICR.B.CMP0EN = 1U;
    MODULE_STM0.CMP[0].U = (uint32)(MODULE_STM0.TIM0.U + TOF_TIMEOUT_TICKS);

    // CMP1 for periodic UDS transmission
    STM0_CMCON.B.MSIZE1 = 31;
    STM0_CMCON.B.MSTART1 = 0;
    MODULE_STM0.ICR.B.CMP1OS = 1; // Interrupt to STMIR1
    MODULE_SRC.STM.STM[0].SR[1].B.TOS = 0; // CPU0
    MODULE_SRC.STM.STM[0].SR[1].B.SRPN = ISR_PRIORITY_STM1;
    MODULE_SRC.STM.STM[0].SR[1].B.CLRR = 1;
    MODULE_SRC.STM.STM[0].SR[1].B.SRE = 1;
    MODULE_STM0.ISCR.B.CMP1IRR = 1U;
    MODULE_STM0.ICR.B.CMP1EN = 1U;
    MODULE_STM0.CMP[1].U = (uint32_t)(MODULE_STM0.TIM0.U + PERIODIC_TICKS);
}

void resetTofTimeoutTimer(void)
{
    uint32 currentStm = MODULE_STM0.TIM0.U;
    MODULE_STM0.CMP[0].U = (uint32)(currentStm + TOF_TIMEOUT_TICKS);
    MODULE_STM0.ISCR.B.CMP0IRR = 1U;
    MODULE_STM0.ICR.B.CMP0EN = 1U;
}
