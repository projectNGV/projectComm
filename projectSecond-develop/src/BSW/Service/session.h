#ifndef SESSION_H_
#define SESSION_H_

#include "Ifx_Types.h"

// 1. 세션 종류 정의
typedef enum {
    SESSION_DEFAULT = 1, // default session
    SESSION_PROGRAMMING = 2, // 안 쓰긴 함
    SESSION_EXTENDED = 3 // extended session
} DiagnosticSession;

// 2. 외부 공개 함수 선언
void session_init(void);
void session_mainFunction(void); // 주기적으로 호출될 타이머 관리 함수
void session_resetTimer(void);   // 통신 수신 시 타이머 리셋 함수
DiagnosticSession session_getCurrent(void);
void session_setCurrent(DiagnosticSession new_session);

#endif /* SESSION_H_ */
