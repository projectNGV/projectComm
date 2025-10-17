#include "gpt12.h"


volatile uint16 g_beepInterval = 8000;

// 10ms 주기 스케줄러 ISR: 진단 관련 함수들을 주기적으로 호출합니다.
IFX_INTERRUPT(IsrGpt12T2Handler, 0, ISR_PRIORITY_GPT12_SCHEDULER);
void IsrGpt12T2Handler(void)
{
    session_mainFunction();
    //diagnoseTofSensor();
    //diagnoseUltrasonicSensor();

}

// 부저용 ISR
IFX_INTERRUPT(IsrGpt1T3Handler, 0, ISR_PRIORITY_GPT1T3_TIMER);
void IsrGpt1T3Handler(void)
{
    buzzerToggle();
    MODULE_GPT120.T3.B.T3 = g_beepInterval;
}

// LED용 ISR
IFX_INTERRUPT(IsrGpt2T6Handler, 0, ISR_PRIORITY_GPT2T6_TIMER);
void IsrGpt2T6Handler(void)
{
    ledUpdateBlinking();
}

// GPT1 인터럽트 Enable/Disable 제어
void Gpt1_Interrupt_Enable (void)
{
    MODULE_SRC.GPT12.GPT12[0].T3.B.SRE = 1;
}

void Gpt1_Interrupt_Disable (void)
{
    MODULE_SRC.GPT12.GPT12[0].T3.B.SRE = 0;
}

// GPT2 인터럽트 Enable/Disable 제어
void Gpt2_Interrupt_Enable (void)
{
    MODULE_SRC.GPT12.GPT12[0].T6.B.SRE = 1;
}

void Gpt2_Interrupt_Disable (void)
{
    MODULE_SRC.GPT12.GPT12[0].T6.B.SRE = 0;
}


// =========================================================================
// 6. 각 타이머 초기화 함수 (static으로 선언하여 내부에서만 사용)
// =========================================================================

// 10ms 주기 스케줄러 타이머(T2) 초기화
static void gpt12_scheduler_init(void)
{
    // T2가 속한 GPT1 블록의 분주기(BPS1)를 T3CON 레지스터를 통해 설정
    MODULE_GPT120.T3CON.B.BPS1 = 2; // (시스템 클럭에 따라 값 조정 필요)

    // T2 타이머 개별 설정
    MODULE_GPT120.T2CON.B.T2M = 0;   // 타이머 모드
    MODULE_GPT120.T2CON.B.T2UD = 0;  // 카운트 업
    MODULE_GPT120.CAPREL.U = 488; // 10ms에 해당하는 타이머 값 => 왜 이렇게 했냐? 분주기 2일 때 T6가

    // T6가 이렇게 했을 때 0.5s라고 했으니까 10ms 는 488일 것이다.
    //MODULE_GPT120.T6.B.T6 = 24414;
    //MODULE_GPT120.CAPREL.B.CAPREL = 24414;



    // T2 인터럽트 설정
    volatile Ifx_SRC_SRCR* src = &MODULE_SRC.GPT12.GPT12[0].T2; // GPT12의 [0]에 속한 T2타이머 전용 회선을 찾아 회선 설정판 src 제어 준비
    src->B.SRPN = ISR_PRIORITY_GPT12_SCHEDULER; // T2 회선 사용할 때 얼마나 긴급한지 우선순위 설정
    src->B.TOS = IfxSrc_Tos_cpu0; // 어떤 cpu가 처리할지 설정
    src->B.CLRR = 1; // 교환원이 전화 받을 시 통화 요청 버튼 자동으로 꺼줘라는 의미로 한 번만 연결하게 함
    src->B.SRE = 1; // 인터럽트 활성화

    // T2 타이머 시작
    MODULE_GPT120.T2CON.B.T2R = 1;
}

// 부저용 타이머(T3) 초기화
static void gpt12_buzzer_init(void)
{
    Ifx_GPT12_T3CON_Bits *t3con = (Ifx_GPT12_T3CON_Bits*) &MODULE_GPT120.T3CON.B;

    // BPS1은 scheduler_init에서 이미 설정했으므로 중복 설정 불필요
    t3con->T3M = 0;     // 타이머 모드
    t3con->T3UD = 1;    // 카운트 다운 모드
    MODULE_GPT120.T3.B.T3 = g_beepInterval;

    // 인터럽트 설정 (초기에는 비활성화)
    volatile Ifx_SRC_SRCR* src = &MODULE_SRC.GPT12.GPT12[0].T3;
    src->B.SRPN = ISR_PRIORITY_GPT1T3_TIMER;
    src->B.TOS = IfxSrc_Tos_cpu0;
    src->B.CLRR = 1;
    src->B.SRE = 0; // 필요할 때 외부에서 Enable

    // 타이머 시작
    t3con->T3R = 1;
}

// LED용 타이머(T6) 초기화
static void gpt12_led_init(void)
{
    Ifx_GPT12_T6CON_Bits *t6con = (Ifx_GPT12_T6CON_Bits*) &MODULE_GPT120.T6CON.B;

    t6con->T6M = 0;
    t6con->BPS2 = 2; // GPT2 블록의 분주기
    t6con->T6UD = 1;
    t6con->T6SR = 1; // 자동 리로드

    // 0.5초 타이머 값
    MODULE_GPT120.T6.B.T6 = 24414;
    MODULE_GPT120.CAPREL.B.CAPREL = 24414;

    // 인터럽트 설정
    volatile Ifx_SRC_SRCR *src = &MODULE_SRC.GPT12.GPT12[0].T6;
    src->B.SRPN = ISR_PRIORITY_GPT2T6_TIMER;
    src->B.TOS = IfxSrc_Tos_cpu0;
    src->B.CLRR = 1;
    src->B.SRE = 1; // 즉시 활성화

    // 타이머 시작
    t6con->T6R = 1;
}


// =========================================================================
// 7. GPT12 모듈 전체 초기화 함수 (외부에 공개되는 유일한 함수)
// =========================================================================
void gpt12Init (void)
{
    // GPT12 모듈의 클럭을 활성화
    IfxScuWdt_clearCpuEndinit(IfxScuWdt_getCpuWatchdogPassword());
    MODULE_GPT120.CLC.U = 0;
    IfxScuWdt_setCpuEndinit(IfxScuWdt_getCpuWatchdogPassword());

    // 필요한 모든 타이머를 순서대로 초기화하고 시작
    gpt12_scheduler_init();
    gpt12_buzzer_init();
    gpt12_led_init();
}
