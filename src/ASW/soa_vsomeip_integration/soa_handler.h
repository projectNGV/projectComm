#ifndef ASW_SOA_VSOMEIP_INTEGRATION_SOA_HANDLER_H_
#define ASW_SOA_VSOMEIP_INTEGRATION_SOA_HANDLER_H_

#include "control.h"
#include "uart.h"
#include <string.h>

extern volatile boolean g_isLogin;  // 로그인 상태 전역 변수 선언

void canSOAHandler(unsigned char cmdType, unsigned char *payload, int len);
void canAuthHandler(unsigned char *payload, int len);

#endif /* ASW_SOA_VSOMEIP_INTEGRATION_SOA_HANDLER_H_ */
