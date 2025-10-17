#include "can_handler.h"
#include <string.h>

/*
 * 파일명: can_handler.c
 * 목적: CAN 수신 메시지를 ID별로 분기하고, 등록된 콜백을 호출하여 상위 로직 처리
 * 구조:
 *   ISR → canRecvMsg() → canHandleMessage()
 *   → (ID 구분) → 각 기능 콜백 호출
 */

/* ─────────────── 콜백 함수 포인터 ─────────────── */
static void (*driveCallback)(unsigned char *)      = 0;
static void (*speedCallback)(unsigned char *)      = 0;
static void (*aebCallback)(unsigned char *)        = 0;
static void (*autoparkCallback)(unsigned char *)   = 0;
static void (*authCallback)(unsigned char *)       = 0;
static void (*faultCallback)(unsigned char *)      = 0;
static void (*tofCallback)(unsigned char *)        = 0;

/* ─────────────── 콜백 등록 함수 ─────────────── */
void canRegisterDriveCallback(void (*callback)(unsigned char *))     { driveCallback = callback; }
void canRegisterSpeedCallback(void (*callback)(unsigned char *))     { speedCallback = callback; }
void canRegisterAebCallback(void (*callback)(unsigned char *))       { aebCallback = callback; }
void canRegisterAutoparkCallback(void (*callback)(unsigned char *))  { autoparkCallback = callback; }
void canRegisterAuthCallback(void (*callback)(unsigned char *))      { authCallback = callback; }
void canRegisterFaultCallback(void (*callback)(unsigned char *))     { faultCallback = callback; }
void canRegisterTofCallback(void (*callback)(unsigned char *))       { tofCallback = callback; }

/* ─────────────── 메시지 처리기 ───────────────
 * 1. ISR에서 canRecvMsg()로 데이터 수신
 * 2. canHandleMessage()에서 ID별 분기 및 콜백 호출
 * 3. 각 기능 모듈에서 등록한 콜백 실행
 */
void canHandleMessage(unsigned int rxID, unsigned char *rxData, int rxLen)
{
    switch (rxID)
    {
        case 0x100:  /* Drive Control */
            /*
             * [Drive Control 명령 수신]
             * 예: rxData[0] = 방향코드 (8=전진, 2=후진 등)
             */
            if (driveCallback)
                driveCallback(rxData);
            break;

        case 0x101:  /* Drive Speed */
            /*
             * [Drive Speed 제어]
             * 예: rxData[0] = PWM Duty (0~100%)
             */
            if (speedCallback)
                speedCallback(rxData);
            break;

        case 0x102:  /* AEB Control */
            /*
             * [AEB 제어]
             * 예: rxData[0] = 0x00 (OFF) / 0x01 (ON)
             */
            if (aebCallback)
                aebCallback(rxData);
            break;

        case 0x103:  /* AutoPark Control */
            /*
             * [AutoPark 명령 수신]
             * 예: rxData[0] = 단계 코드 (0x01=Start 등)
             */
            if (autoparkCallback)
                autoparkCallback(rxData);
            break;

        case 0x104:  /* Auth Password */
            /*
             * [Auth 인증 요청]
             * 예: rxData[0~3] = '1','2','3','4'
             */
            if (authCallback)
                authCallback(rxData);
            break;

        case 0x1FE:  /* Fault / Emergency */
            /*
             * [Fault / Emergency Stop]
             * 예: rxData[0] = 0x01 (E-Stop), 0x02 (Reset)
             */
            if (faultCallback)
                faultCallback(rxData);
            break;

        case 0x200:  /* ToF Sensor Feedback */
            /*
             * [ToF 거리 데이터 수신]
             * 예: rxData[0~1] = 거리(cm) (0~400)
             * ※ TC375 → RPi 방향일 수도 있으므로 유지
             */
            if (tofCallback)
                tofCallback(rxData);
            break;

        default:
            /*
             * [미등록 ID]
             * - 디버깅 로그 출력
             * - 추후 신규 서비스용 메시지 추가 가능
             */
            break;
    }
}
