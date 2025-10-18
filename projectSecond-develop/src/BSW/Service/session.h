#ifndef SESSION_H_
#define SESSION_H_

#include "Ifx_Types.h"

// --- 서버 타이밍 파라미터 정의 (단위: ms) ---
// P2: 서버가 요청을 받고 응답을 보내기까지의 최대 시간
#define P2_SERVER_MAX_MS    50
// P2*: 서버가 Response Pending(0x78)을 보낸 후 다음 응답까지의 최대 시간
#define P2_STAR_SERVER_MAX_MS 5000


// 1. 세션 종류 정의
typedef enum {
    SESSION_DEFAULT = 1, // default session
    SESSION_PROGRAMMING = 2, // programming session
    SESSION_EXTENDED = 3 // extended session
} DiagnosticSession;

// 2. 외부 공개 함수 선언
void session_init(void);
void session_mainFunction(void); // 주기적으로 호출될 타이머 관리 함수
void session_resetTimer(void);   // 통신 수신 시 타이머 리셋 함수
DiagnosticSession session_getCurrent(void);
void session_setCurrent(DiagnosticSession new_session);

#endif /* SESSION_H_ */
