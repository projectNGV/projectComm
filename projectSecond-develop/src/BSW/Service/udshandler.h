#ifndef BSW_SERVICE_UDSHANDLER_H_
#define BSW_SERVICE_UDSHANDLER_H_

#include "isotp.h"
#include "session.h"
#include "config.h"
#include "dtc.h"
#include "tof.h"
#include "ultrasonic.h"
#include "routine_control.h"
#include "udsprotocol.h" // uds 표준 enum으로 설정

// 함수 선언
void udsHandler (unsigned char *rxData, int rxLen);

#endif /* BSW_SERVICE_UDSHANDLER_H_ */
