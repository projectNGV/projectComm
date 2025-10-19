#ifndef BSW_SERVICE_CANTP_H_
#define BSW_SERVICE_CANTP_H_

#include <stdint.h>

/*********************************************************************************************************************/
/*------------------------------------------------------Macros-------------------------------------------------------*/
/*********************************************************************************************************************/
#define CANTP_RX_BUFFER_SIZE 4095
#define CANTP_TX_BUFFER_SIZE 4095
#define FLOW_CONTROL_BLOCK_SIZE 10 // Block size (BS=10)

/*********************************************************************************************************************/
/*--------------------------------------------------Data Structures--------------------------------------------------*/
/*********************************************************************************************************************/
typedef enum {
    ISO_TP_STATE_IDLE,          // 유휴 상태
    ISO_TP_STATE_WAIT_CF,       // 연속 프레임(CF) 수신 대기 상태
    ISO_TP_STATE_SENDING_CF,    // 연속 프레임(CF) 송신 중 상태
    ISO_TP_STATE_WAIT_FC        // Flow Control 수신 대기 상태
} IsoTpState;

/*********************************************************************************************************************/
/*-----------------------------------------------Function Prototypes-------------------------------------------------*/
/*********************************************************************************************************************/
// Called by can.c
void handleSingleFrame(const unsigned char* can_data);
void handleFirstFrame(const unsigned char* can_data);
void handleConsecutiveFrame(const unsigned char* can_data);
void handleFlowControl(const unsigned char* can_data);
void sendConsecutiveFrame(void);

// Called by uds.c
void CANTP_SendResponse(const uint8_t* data, uint16_t length);

#endif /* BSW_SERVICE_CANTP_H_ */
