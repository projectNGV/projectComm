#include "soa_handler.h"

/* ============================================================
 *  SOA Handler (Service-Oriented Architecture CAN Command)
 *  - SOME/IP → CAN 변환 후 TC375 수신
 *  - 각 cmdType별로 전역 motorState 또는 시스템 상태를 갱신
 * ============================================================ */

static const char *PASSWORD = "1234";      // 인증용 비밀번호
volatile boolean g_isLogin = FALSE;        // 인증 상태 플래그 (TRUE: 로그인됨)
unsigned char g_authPw[8];        // 수신된 AUTH 문자열 저장 (최대 7B + NULL)


/* ============================================================
 * @brief  CAN 명령 처리기 (SOA 기반)
 * @param  cmdType : 명령 유형
 * @param  payload : 수신된 데이터 포인터
 * @param  len     : 데이터 길이
 * ============================================================ */
void canSOAHandler(unsigned char cmdType, unsigned char *payload, int len)
{
    switch (cmdType)
    {
        /* ──────────────── 0x01: 주행 방향 설정 ──────────────── */
        case 0x01:
            myPrintf("[SOA] DIR = %d\n", motorState.currentDir);
            motorState.currentDir = payload[0];
            break;

        /* ──────────────── 0x02: 속도(Duty) 설정 ──────────────── */
        case 0x02:
            myPrintf("[SOA] SPEED = %d\n", motorState.currentDuty);
            motorState.currentDuty = payload[0];
            break;

        /* ──────────────── 0x03: AEB 제어 ──────────────── */
        case 0x03:
            myPrintf("[SOA] AEB = %s\n", motorState.aebActiveFlag ? "ON" : "OFF");
            motorState.aebActiveFlag = (payload[0] != 0) ? TRUE : FALSE;
            break;

        /* ──────────────── 0x04: AutoPark 제어 ──────────────── */
        case 0x04:
            myPrintf("[SOA] AUTOPARK START\n");
            motorState.autoParkFlag = TRUE;
            break;

        /* ──────────────── 0x05: AUTH Password 비교 ──────────────── */
        case 0x05:
            /* 수신된 비밀번호 문자열 저장 */
            memset((void*)g_authPw, 0, sizeof(g_authPw));
            memcpy((void*)g_authPw, payload, len);

            myPrintf("[SOA] AUTH PW = %s\n", g_authPw);
            canAuthHandler(payload, len);  // 비밀번호 비교 수행
            break;

        /* ──────────────── 0xFE: 긴급 정지 ──────────────── */
        case 0xFE:
            myPrintf("[SOA] EMERGENCY STOP!!\n");
            motorState.currentDir = '5';
            motorState.currentDuty = 0;
            break;

        /* ──────────────── 예외 처리 ──────────────── */
        default:
            myPrintf("[SOA] Unknown cmdType: 0x%02X\n", cmdType);
            break;
    }
}


/* ============================================================
 * @brief  AUTH 비밀번호 비교기
 * @param  payload : 수신된 문자열
 * @param  len     : 문자열 길이
 * ============================================================ */
void canAuthHandler(unsigned char *payload, int len)
{
    char input[8] = {0};
    memcpy(input, payload, len);

    if (strcmp(input, PASSWORD) == 0)
    {
        g_isLogin = TRUE;
        myPrintf("[SOA] AUTH OK (%s)\n", input);
    }
    else
    {
        g_isLogin = FALSE;
        myPrintf("[SOA] AUTH FAIL (%s)\n", input);
    }
}
