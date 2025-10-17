#ifndef ASW_CAN_HANDLER_H_
#define ASW_CAN_HANDLER_H_

#include "can.h"

/*
 * 파일명: can_handler.h
 * 목적: CAN 수신 메시지를 ID별로 분기하고,
 *       상위 애플리케이션 계층(Drive, AEB, AutoPark 등)으로 전달하기 위한 인터페이스 제공
 * 구조:
 *   - BSW(can.c)에서 수신된 메시지를 canHandleMessage()로 전달
 *   - ID별로 구분하여 각 기능 콜백 호출
 *   - 기능별 모듈에서 콜백 등록 (ex. canRegisterDriveCallback)
 *
 * 사용 예시:
 *   1. drive.c에서 초기화 시 → canRegisterDriveCallback(driveCallback);
 *   2. ISR → canRecvMsg() → canHandleMessage(rxID, rxData, rxLen);
 */

#ifdef __cplusplus
extern "C" {
#endif

/* ─────────────── CAN Message ID 정의 ───────────────
 *  veh_control_server.cpp와 1:1 매칭
 */
#define CAN_ID_DRIVE_CTRL      0x100   /* Drive 제어 */
#define CAN_ID_SPEED_CTRL      0x101   /* PWM 듀티 */
#define CAN_ID_AEB_CTRL        0x102   /* AEB 제어 */
#define CAN_ID_AUTOPARK_CTRL   0x103   /* AutoPark */
#define CAN_ID_AUTH_CTRL       0x104   /* Auth 인증 */
#define CAN_ID_FAULT_CTRL      0x1FE   /* Fault / Emergency */
#define CAN_ID_TOF_DATA        0x200   /* ToF 거리 데이터 */

/* ─────────────── 콜백 등록 함수 ───────────────
 *  각 기능 모듈이 초기화 시 자신 전용 콜백 등록
 */
void canRegisterDriveCallback(void (*callback)(unsigned char *));
void canRegisterSpeedCallback(void (*callback)(unsigned char *));
void canRegisterAebCallback(void (*callback)(unsigned char *));
void canRegisterAutoparkCallback(void (*callback)(unsigned char *));
void canRegisterAuthCallback(void (*callback)(unsigned char *));
void canRegisterFaultCallback(void (*callback)(unsigned char *));
void canRegisterTofCallback(void (*callback)(unsigned char *));

/* ─────────────── 수신 메시지 처리 함수 ───────────────
 *  ISR에서 canRecvMsg() 이후에 호출
 *  → ID별로 분기, 등록된 콜백 호출
 */
void canHandleMessage(unsigned int rxID, unsigned char *rxData, int rxLen);

#ifdef __cplusplus
}
#endif

#endif /* ASW_CAN_HANDLER_H_ */
