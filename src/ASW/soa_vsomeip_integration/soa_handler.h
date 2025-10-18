#ifndef ASW_SOA_VSOMEIP_INTEGRATION_SOA_HANDLER_H_
#define ASW_SOA_VSOMEIP_INTEGRATION_SOA_HANDLER_H_

#include "control.h"
#include "uart.h"
#include "tof.h"
#include <string.h>
#include <stdbool.h>

#define CMD_SET_DIR          0x01   // 주행 방향 설정
#define CMD_SET_SPEED        0x02   // 속도 설정 (0~100)
#define CMD_CTRL_AEB         0x03   // AEB 기능 ON/OFF
#define CMD_CTRL_AUTOPARK    0x04   // AutoPark 시작
#define CMD_AUTH_PASSWORD    0x05   // 인증 비밀번호
#define CMD_EMERGENCY_STOP   0xFE   // 긴급 정지

extern volatile bool g_isLogin;  // 로그인 상태 전역 변수 선언

void canSOAHandler(unsigned char cmdType, unsigned char *payload, int len);
void canAuthHandler(unsigned char *payload, int len);

#endif /* ASW_SOA_VSOMEIP_INTEGRATION_SOA_HANDLER_H_ */
