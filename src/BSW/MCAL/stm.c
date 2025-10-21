/*********************************************************************************************************************/
/*-----------------------------------------------------Includes------------------------------------------------------*/
/*********************************************************************************************************************/
#include "stm.h"

/*********************************************************************************************************************/
/*------------------------------------------------------Macros-------------------------------------------------------*/
/*********************************************************************************************************************/
#define TOF_TIMEOUT_MS 150 // ToF 메시지 타임아웃 시간 (ms)
#define TOF_TIMEOUT_TICKS (TOF_TIMEOUT_MS * TICKS_PER_MS)

/*********************************************************************************************************************/
/*-------------------------------------------------Global variables--------------------------------------------------*/
/*********************************************************************************************************************/

/*********************************************************************************************************************/
/*------------------------------------------------Function Prototypes------------------------------------------------*/
/*********************************************************************************************************************/
void stmCMP0Init(void);
void stmCMP1Init(void);
void stmCMP2Init(void);

/*********************************************************************************************************************/
/*---------------------------------------------Function Implementations----------------------------------------------*/
/*********************************************************************************************************************/
IFX_INTERRUPT(stm0IsrHandler, 0, ISR_PRIORITY_STM0);
void stm0IsrHandler(void) {
    // 타임아웃 발생 처리
    g_TofValue = TOF_INVALID_VALUE; // ToF 값 무효화

    // 타임아웃 인터럽트 비활성화 (다음 메시지 수신 시 재활성화)
    MODULE_STM0.ICR.B.CMP0EN = 0; // CMP0 Interrupt disable
}

IFX_INTERRUPT(stm1IsrHandler, 0, ISR_PRIORITY_STM1);
void stm1IsrHandler(void) {
    boolean anyTaskActive = FALSE; // 활성화된 작업 있는지 확인용 플래그

    // 주기적 작업 타이머 감소 및 활성 작업 확인
    for (int i = 0; i < MAX_PERIODIC_TASKS; i++) {
        if (g_periodicTasks[i].isActive) {
            anyTaskActive = TRUE; // 활성화된 작업 발견!
            if (g_periodicTasks[i].timer > 0) {
                if (g_periodicTasks[i].timer <= PERIODIC_INTERVAL_MS) {
                    g_periodicTasks[i].timer = 0;
                } else {
                    g_periodicTasks[i].timer -= PERIODIC_INTERVAL_MS;
                }
            }
        }
    }

    // 활성화된 작업이 하나라도 있으면 다음 인터럽트 예약
    if (anyTaskActive == TRUE) {
        MODULE_STM0.CMP[1].U += PERIODIC_TICKS; // 다음 인터럽트 시점 계산 및 설정
        MODULE_STM0.ISCR.B.CMP1IRR = 1U; // 인터럽트 플래그 클리어
    } else {
        // 활성화된 작업이 없으면 인터럽트 비활성화
        MODULE_STM0.ICR.B.CMP1EN = 0U;
        // 인터럽트 플래그 클리어 (혹시 모를 상황 대비)
        MODULE_STM0.ISCR.B.CMP1IRR = 1U;
    }
}

int cnt = 0;
IFX_INTERRUPT(stm2IsrHandler, 0, ISR_PRIORITY_STM2);
void stm2IsrHandler(void)
{
    /* 다음 인터럽트 예약 (500ms 후) */
    MODULE_STM1.CMP[0].B.CMPVAL = MODULE_STM1.TIM0.U + STM_TICK_500MS;

    /* === 주기 작업 수행 === */
    sendTofDistancePeriodic();   // ToF 주기 송신
    myPrintf("%d\n", cnt++);
}

/*********************************************************************************************************************/
/*--------------------------------------------------------Init-------------------------------------------------------*/
/*********************************************************************************************************************/

void stmInit(void) {
    stmCMP0Init();
    stmCMP1Init();
    stmCMP2Init();
}

void stmCMP0Init(void) {  // TOF sensor timeout에 사용
    STM0_CMCON.B.MSIZE0 = 31; // 32비트 전체 비교
    STM0_CMCON.B.MSTART0 = 0; // STM 하위 32비트와 비교
    MODULE_STM0.ICR.B.CMP0OS = 0; // 인터럽트 출력 STMIR0 설정

    // 인터럽트 컨트롤러 설정
    MODULE_SRC.STM.STM[0].SR[0].B.TOS = 0; // CPU 0
    MODULE_SRC.STM.STM[0].SR[0].B.SRPN = ISR_PRIORITY_STM0;
    MODULE_SRC.STM.STM[0].SR[0].B.CLRR = 1; // clear Service Request Flag
    MODULE_SRC.STM.STM[0].SR[0].B.SRE = 1; // enable Service Request

    MODULE_STM0.ISCR.B.CMP0IRR = 1U; // clear CMP0 Interrupt Flag
    MODULE_STM0.ICR.B.CMP0EN = 1U; // CMP0 Interrupt Enable

    MODULE_STM0.CMP[0].U = (uint32)(MODULE_STM0.TIM0.U + TOF_TIMEOUT_TICKS); // 첫 인터럽트 시점 설정
}

void stmCMP1Init(void) {  // 주기적 출력에 사용
    STM0_CMCON.B.MSIZE1 = 31; // 32비트 전체 비교
    STM0_CMCON.B.MSTART1 = 0; // STM 하위 32비트와 비교
    MODULE_STM0.ICR.B.CMP1OS = 1; // 인터럽트 출력 STMIR1 설정

    // 인터럽트 컨트롤러 설정
    MODULE_SRC.STM.STM[0].SR[1].B.TOS = 0; // CPU 0
    MODULE_SRC.STM.STM[0].SR[1].B.SRPN = ISR_PRIORITY_STM1;
    MODULE_SRC.STM.STM[0].SR[1].B.CLRR = 1; // clear Service Request Flag
    MODULE_SRC.STM.STM[0].SR[1].B.SRE = 1; // enable Service Request

    MODULE_STM0.ISCR.B.CMP1IRR = 1U; // clear CMP1 Interrupt Flag
    MODULE_STM0.ICR.B.CMP1EN = 1U; // CMP1 Interrupt Enable

    MODULE_STM0.CMP[1].U = (uint32_t)(MODULE_STM0.TIM0.U + PERIODIC_TICKS); // 첫 인터럽트 시점 설정
}

void stmCMP2Init(void)
{
    /* 비교기 설정 */
    MODULE_STM1.CMCON.B.MSIZE0 = 0x1F;
    MODULE_STM1.CMCON.B.MSTART0 = 0;

    MODULE_STM1.ICR.B.CMP0OS = 0; // 인터럽트 출력 STMIR0 설정


    /* 인터럽트 소스 설정 */
    Ifx_SRC_SRCR_Bits *src = (Ifx_SRC_SRCR_Bits*)&MODULE_SRC.STM.STM[1].SR[0].B;
    src->SRPN = ISR_PRIORITY_STM2;
    src->TOS  = 0;
    src->CLRR = 1;
    src->SRE  = 1;

    MODULE_STM1.ISCR.B.CMP0IRR = 1U; // clear CMP0 Interrupt Flag
    MODULE_STM1.ICR.B.CMP0EN = 1U; // CMP0 Interrupt Enable

    /* 첫 비교값 설정 */
    MODULE_STM1.CMP[0].B.CMPVAL = MODULE_STM1.TIM0.U + STM_TICK_500MS;
}


// ToF 메시지 수신 시 타임아웃 타이머 리셋 함수
void resetTofTimeoutTimer(void) {
    // 다음 타임아웃 시점 계산 및 설정
    uint32 currentStm = MODULE_STM0.TIM0.U; // 현재 STM 값 읽기 (MODULE_ 접두사 사용)
    MODULE_STM0.CMP[0].U = (uint32)(currentStm + TOF_TIMEOUT_TICKS); // CMP0 업데이트

    // 타임아웃 인터럽트 플래그 클리어 및 활성화
    MODULE_STM0.ISCR.B.CMP0IRR = 1U; // CMP0 인터럽트 플래그 클리어
    MODULE_STM0.ICR.B.CMP0EN = 1U;   // CMP0 인터럽트 다시 활성화
}




