#include "session.h"

// --- 상수 정의 ---
// 세션 타임아웃 시간 (5초).
// session_mainFunction()이 20ms마다 호출되므로, 250번 호출되면 5초가 경과한 것입니다.
#define S3_SERVER_TIMEOUT_TICKS 250

// --- 모듈 내부 전역 변수 (static) ---
// 현재 진단 세션 상태를 저장하는 변수
static DiagnosticSession g_currentSession = SESSION_DEFAULT;
// 세션 타임아웃을 세기 위한 10ms 단위 카운터
static uint16 g_sessionTimeoutCounter = 0;

// =========================================================================
// 함수 구현
// =========================================================================

// 세션 관리 모듈 초기화
void session_init(void) {
    g_currentSession = SESSION_DEFAULT;
    g_sessionTimeoutCounter = 0; // 카운터를 0으로 초기화
}



// GPT 타이머 인터럽트에 의해 10ms마다 주기적으로 호출되어야 한다.
void session_mainFunction(void) {
    // Default 세션 상태에서는 타임아웃을 검사할 필요가 없습니다.
    if (g_currentSession == SESSION_DEFAULT) {
        return;
    }

    // 10ms가 지났으므로 카운터를 1 증가시킵니다.
    g_sessionTimeoutCounter++;

    // 카운터가 500번 (5초)을 초과했는지 확인합니다.
    if (g_sessionTimeoutCounter > S3_SERVER_TIMEOUT_TICKS) {
        // 타임아웃이 발생했으므로, 보안을 위해 Default 세션으로 강제 복귀합니다.
        myPrintf("[SESSION] Timeout! Reverting to Default Session.\n");
        g_currentSession = SESSION_DEFAULT;
    }
}

// 세션 타임아웃 카운터를 리셋한다. CAN 핸들러에서 UDS 요청 수신시마다 호출되어야 한다.
void session_resetTimer(void) {
    // 통신이 수신되었으므로 카운터를 0으로 리셋합니다.
    g_sessionTimeoutCounter = 0;
}

// 현재 세션 상태를 반환한다.
DiagnosticSession session_getCurrent(void) {
    return g_currentSession;
}


// 현재 세션 상태를 변경한다.
void session_setCurrent(DiagnosticSession new_session) {
    // 세션 상태가 실제로 변경되었을 때만 동작합니다.
    if (g_currentSession != new_session) {
        g_currentSession = new_session;
        session_resetTimer(); // 세션이 변경되면 즉시 타임아웃 카운터를 리셋합니다.

        if (g_currentSession == SESSION_DEFAULT) {
            myPrintf("DEFAULT SESSION\n");
        }
        else if (g_currentSession == SESSION_EXTENDED) {
            myPrintf("EXTENDED SESSION START TIME COUNT START\n");
        }
    }
}

