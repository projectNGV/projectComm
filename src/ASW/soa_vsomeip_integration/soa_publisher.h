#ifndef ASW_SOA_VSOMEIP_INTEGRATION_SOA_PUBLISHER_H_
#define ASW_SOA_VSOMEIP_INTEGRATION_SOA_PUBLISHER_H_

#include "can.h"
#include "motor.h"
#include "tof.h"
#include "stm.h"
#include <stdbool.h>

/* ============================================================
 *  Status Type Definitions (status_type)
 *  - TC375 → RPi → Client (Publisher 기준)
 * ============================================================ */
#define STATUS_AEB_STATE        0x01  // AEB on/off
#define STATUS_AUTOPARK_STATE   0x02  // 자동주차 단계
#define STATUS_TOF_DISTANCE     0x03  // ToF 거리 (mm)
#define STATUS_AUTH_STATE       0x04  // 인증 상태


void soaPublisherInit(void);

void sendAebStateIfChanged(void);
void sendAutoparkStateIfChanged(unsigned char state);
void sendAuthStateIfChanged(bool newState);
void sendTofDistancePeriodic(void);

#endif /* ASW_SOA_VSOMEIP_INTEGRATION_SOA_PUBLISHER_H_ */
