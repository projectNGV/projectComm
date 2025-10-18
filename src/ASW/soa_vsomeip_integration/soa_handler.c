#include "soa_handler.h"

/* ============================================================
 *  SOA Handler (Service-Oriented Architecture CAN Command)
 *  - SOME/IP → CAN 변환 후 TC375 수신
 *  - 각 cmdType별로 전역 motorState 또는 시스템 상태를 갱신
 * ============================================================ */

static const char *PASSWORD = "1234";      // 인증용 비밀번호
volatile bool g_isLogin = false;        // 인증 상태 플래그 (TRUE: 로그인됨)
unsigned char g_authPw[8];                 // 수신된 AUTH 문자열 저장 (최대 7B + NULL)


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
        case CMD_SET_DIR:
            motorState.currentDir = payload[0];
            myPrintf("[SOA] DIR = %d\n", motorState.currentDir);
            break;

        /* ──────────────── 0x02: 속도(Duty) 설정 ──────────────── */
        case CMD_SET_SPEED:
            motorState.currentDuty = payload[0] * 10;
            myPrintf("[SOA] SPEED = %d\n", motorState.currentDuty);
            break;

        /* ──────────────── 0x03: AEB 제어 ──────────────── */
        case CMD_CTRL_AEB:
            myPrintf("[SOA] AEB = %s\n", aebEnableFlag ? "ON" : "OFF");
            aebEnableFlag = (payload[0] != 0) ? true : false;
            break;

        /* ──────────────── 0x04: AutoPark 제어 ──────────────── */
        case CMD_CTRL_AUTOPARK:
            myPrintf("[SOA] AUTOPARK START\n");
            motorState.autoParkFlag = true;
            break;

        /* ──────────────── 0x05: AUTH Password 비교 ──────────────── */
        case CMD_AUTH_PASSWORD:
            /* 수신된 비밀번호 문자열 저장 */
            memset((void*)g_authPw, 0, sizeof(g_authPw));
            memcpy((void*)g_authPw, payload, len);

            myPrintf("[SOA] AUTH PW = %s\n", g_authPw);
            canAuthHandler(payload, len);  // 비밀번호 비교 수행
            break;

        /* ──────────────── 0xFE: 긴급 정지 ──────────────── */
        case CMD_EMERGENCY_STOP:
            myPrintf("[SOA] EMERGENCY STOP!!\n");
            motorState.currentDir = '5';
            motorState.currentDuty = 0;
            motorState.aebActiveFlag = true;
            sendAebStateIfChanged();
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

    bool prevLogin = g_isLogin;

    if (strcmp(input, PASSWORD) == 0)
    {
        g_isLogin = true;
//        myPrintf("[SOA] AUTH OK (%s)\n", input);
    }
    else
    {
        g_isLogin = false;
//        myPrintf("[SOA] AUTH FAIL (%s)\n", input);
    }

    // 상태 변화 감지 후 송신
    if (g_isLogin != prevLogin)
        sendAuthStateIfChanged(g_isLogin);
}
