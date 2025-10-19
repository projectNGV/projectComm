/*********************************************************************************************************************/
/*-----------------------------------------------------Includes------------------------------------------------------*/
/*********************************************************************************************************************/
#include "cantp.h"
#include "uds.h" // UDS_HandleMessage 호출
#include "can.h"
#include "uart.h"
#include <string.h>

/*********************************************************************************************************************/
/*------------------------------------------------------Macros-------------------------------------------------------*/
/*********************************************************************************************************************/
#define FLOW_CONTROL_SEPARATION_TIME_MS 0x0F // 15ms

/*********************************************************************************************************************/
/*-------------------------------------------------Global variables--------------------------------------------------*/
/*********************************************************************************************************************/
// --- 수신(Rx) 상태 관리 변수 ---
static volatile IsoTpState g_iso_tp_rx_state = ISO_TP_STATE_IDLE;
static volatile uint16_t g_uds_rx_total_size = 0;
static volatile uint8_t g_rx_expected_seq_num = 0;
static volatile uint8_t g_uds_rx_buffer[CANTP_RX_BUFFER_SIZE];
static volatile uint16_t g_uds_rx_received_size = 0;
static volatile uint16_t g_rx_block_frame_count = 0;

// --- 송신(Tx) 상태 관리 변수 ---
static volatile IsoTpState g_iso_tp_tx_state = ISO_TP_STATE_IDLE;
static volatile uint8_t g_uds_tx_buffer[CANTP_TX_BUFFER_SIZE];
static volatile uint16_t g_uds_tx_total_size = 0;
static volatile uint16_t g_uds_tx_sent_size = 0;
static volatile uint8_t g_tx_seq_num = 0;

/*********************************************************************************************************************/
/*--------------------------------------------Private Function Prototypes--------------------------------------------*/
/*********************************************************************************************************************/
static void sendSingleFrame(const uint8_t* data, uint16_t length);
static void sendFirstFrame(const uint8_t* data, uint16_t length);
static void sendFlowControl(uint8_t flowStatus, uint8_t blockSize, uint8_t separationTime);

/*********************************************************************************************************************/
/*---------------------------------------------Function Implementations----------------------------------------------*/
/*********************************************************************************************************************/

void CANTP_SendResponse(const uint8_t* data, uint16_t length)
{
    if (g_iso_tp_tx_state != ISO_TP_STATE_IDLE) return; // Ignore if busy

    if (length <= 7)
    {
        sendSingleFrame(data, length);
    }
    else
    {
        sendFirstFrame(data, length);
    }
}

void handleSingleFrame(const unsigned char* canData)
{
    if (g_iso_tp_rx_state == ISO_TP_STATE_IDLE)
    {
        uint8_t msg_len = canData[0] & 0x0F;
        UDS_HandleMessage(&canData[1], msg_len);
    }
}

void handleFirstFrame(const unsigned char* canData)
{
    if (g_iso_tp_rx_state == ISO_TP_STATE_IDLE)
    {
        memset((void*)g_uds_rx_buffer, 0, CANTP_RX_BUFFER_SIZE);
        g_uds_rx_total_size = ((canData[0] & 0x0F) << 8) | canData[1];
        memcpy((void*)g_uds_rx_buffer, &canData[2], 6);
        g_uds_rx_received_size = 6;
        g_rx_expected_seq_num = 1;
        g_rx_block_frame_count = 0;
        g_iso_tp_rx_state = ISO_TP_STATE_WAIT_CF;

        sendFlowControl(0, FLOW_CONTROL_BLOCK_SIZE, FLOW_CONTROL_SEPARATION_TIME_MS); // FS=0 (Continue)
    }
}

void handleConsecutiveFrame(const unsigned char* canData)
{
    if (g_iso_tp_rx_state != ISO_TP_STATE_WAIT_CF) return;

    uint8_t sequenceNumber = canData[0] & 0x0F;

    if (sequenceNumber == g_rx_expected_seq_num)
    {
        uint16_t remainingBytes = g_uds_rx_total_size - g_uds_rx_received_size;
        uint8_t bytesToCopy = (remainingBytes > 7) ? 7 : remainingBytes;

        memcpy((void*)&g_uds_rx_buffer[g_uds_rx_received_size], &canData[1], bytesToCopy);

        g_uds_rx_received_size += bytesToCopy;
        g_rx_expected_seq_num = (g_rx_expected_seq_num + 1) % 16;
        g_rx_block_frame_count++;

        if (g_uds_rx_received_size >= g_uds_rx_total_size)
        {
            myPrintf("Multi-frame Message Reassembled. Total Length: %d bytes\n", g_uds_rx_total_size);
            UDS_HandleMessage((uint8_t*)g_uds_rx_buffer, g_uds_rx_total_size);
            g_iso_tp_rx_state = ISO_TP_STATE_IDLE;
        }
        else if (FLOW_CONTROL_BLOCK_SIZE > 0 && g_rx_block_frame_count == FLOW_CONTROL_BLOCK_SIZE)
        {
            myPrintf("Block complete. Sending next Flow Control.\n");
            g_rx_block_frame_count = 0;
            sendFlowControl(0, FLOW_CONTROL_BLOCK_SIZE, FLOW_CONTROL_SEPARATION_TIME_MS);
        }
    }
    else
    {
        g_iso_tp_rx_state = ISO_TP_STATE_IDLE;
        myPrintf("Consecutive frame sequence error\n");
    }
}

void handleFlowControl(const unsigned char* canData)
{
    if (g_iso_tp_tx_state == ISO_TP_STATE_WAIT_FC)
    {
        if ((canData[0] & 0x0F) == 0) // FlowStatus is ContinueToSend
        {
            g_iso_tp_tx_state = ISO_TP_STATE_SENDING_CF;
            sendConsecutiveFrame();
        }
    }
}

void sendConsecutiveFrame(void)
{
    if (g_iso_tp_tx_state == ISO_TP_STATE_SENDING_CF)
    {
        if (g_uds_tx_sent_size < g_uds_tx_total_size)
        {
            uint8_t cfFrame[8] = {0,};
            cfFrame[0] = 0x20 | (g_tx_seq_num & 0x0F);

            uint16_t remainingSize = g_uds_tx_total_size - g_uds_tx_sent_size;
            uint8_t bytesToSend = (remainingSize > 7) ? 7 : remainingSize;

            memcpy(&cfFrame[1], (const void*)&g_uds_tx_buffer[g_uds_tx_sent_size], bytesToSend);
            canSendMsg(UDS_RESPONSE_CAN_ID, (char*)cfFrame, bytesToSend + 1);

            g_uds_tx_sent_size += bytesToSend;
            g_tx_seq_num = (g_tx_seq_num + 1) % 16;
        }
        else
        {
            g_iso_tp_tx_state = ISO_TP_STATE_IDLE;
            myPrintf("All consecutive frames sent.\n");
        }
    }
}

static void sendSingleFrame(const uint8_t* data, uint16_t length)
{
    uint8_t sfFrame[8] = {0,};
    sfFrame[0] = 0x00 | length;
    memcpy(&sfFrame[1], data, length);
    canSendMsg(UDS_RESPONSE_CAN_ID, (char*)sfFrame, 8);
    myPrintf("Single frame sent.\n");
}

static void sendFirstFrame(const uint8_t* data, uint16_t length)
{
    memcpy((void*)g_uds_tx_buffer, data, length);
    g_uds_tx_total_size = length;
    g_uds_tx_sent_size = 6;
    g_tx_seq_num = 1;

    uint8_t ffFrame[8] = {0,};
    ffFrame[0] = 0x10 | ((length >> 8) & 0x0F);
    ffFrame[1] = length & 0xFF;
    memcpy(&ffFrame[2], data, 6);

    g_iso_tp_tx_state = ISO_TP_STATE_WAIT_FC;
    canSendMsg(UDS_RESPONSE_CAN_ID, (char*)ffFrame, 8);
    myPrintf("First frame sent.\n");
}

static void sendFlowControl(uint8_t flowStatus, uint8_t blockSize, uint8_t separationTime)
{
    uint8_t fcFrame[8] = {0, };
    fcFrame[0] = 0x30 | (flowStatus & 0x0F);
    fcFrame[1] = blockSize;
    fcFrame[2] = separationTime;
    canSendMsg(UDS_RESPONSE_CAN_ID, (char*)fcFrame, 8); // Note: UDS spec uses Tester's request ID for FC response
    myPrintf("Flow control frame sent.\n");
}
