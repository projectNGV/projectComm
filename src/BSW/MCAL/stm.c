#include "stm.h"

IFX_INTERRUPT(stm1IsrHandler, 0, ISR_PRIORITY_STM1);
void stm1IsrHandler(void)
{
    /* 다음 인터럽트 예약 (500ms 후) */
    MODULE_STM1.CMP[0].B.CMPVAL = MODULE_STM1.TIM0.U + STM_TICK_500MS;

    /* === 주기 작업 수행 === */
    sendTofDistancePeriodic();   // ToF 주기 송신
}

void stmInterruptInit(void)
{
    /* 비교기 설정 */
    MODULE_STM1.CMCON.B.MSIZE0 = 0x1F;
    MODULE_STM1.CMCON.B.MSTART0 = 0;

    MODULE_STM1.ICR.B.CMP0OS = 0; // 인터럽트 출력 STMIR0 설정


    /* 인터럽트 소스 설정 */
    Ifx_SRC_SRCR_Bits *src = (Ifx_SRC_SRCR_Bits*)&MODULE_SRC.STM.STM[1].SR[0].B;
    src->SRPN = ISR_PRIORITY_STM1;
    src->TOS  = 0;
    src->CLRR = 1;
    src->SRE  = 1;

    MODULE_STM1.ISCR.B.CMP0IRR = 1U; // clear CMP0 Interrupt Flag
    MODULE_STM1.ICR.B.CMP0EN = 1U; // CMP0 Interrupt Enable

    /* 첫 비교값 설정 */
    MODULE_STM1.CMP[0].B.CMPVAL = MODULE_STM1.TIM0.U + STM_TICK_500MS;
}
