/*********************************************************************************************************************/
/*-----------------------------------------------------Includes------------------------------------------------------*/
/*********************************************************************************************************************/
#include "can.h"
#include "cantp.h"
#include "tof.h"
#include "session.h"
#include "uds.h"

#include <string.h>

/*********************************************************************************************************************/
/*-------------------------------------------------Global variables--------------------------------------------------*/
/*********************************************************************************************************************/
McmcanType g_mcmcan; /* Global MCMCAN configuration and control structure    */

// ★ 추가: 간단한 주행 상태 캐시 (속도/방향)
static uint8 g_drive_speed = 0; // 0~100 (%)
static uint8 g_drive_dir = 5;   // 설계서의 초기값(정지:5)

/*********************************************************************************************************************/
/*--------------------------------------------Private Function Prototypes--------------------------------------------*/
/*********************************************************************************************************************/
static const char* dirToText(uint8 dirByte);
static inline void canSend8(uint32 id, const uint8 *d);
static inline void canSendEvent(uint8 eid, const uint8 *payload, uint8 len);
static inline void sendDriveStateEvent(void);

/*********************************************************************************************************************/
/*---------------------------------------------Function Implementations----------------------------------------------*/
/*********************************************************************************************************************/

/* Callback 함수 포인터 */
static void (*tofCallback)(unsigned char *rxData) = 0;
void canRegisterTofCallback(void (*callback)(unsigned char*))
{
    tofCallback = callback;
}

/* Default CAN Tx Handler */
IFX_INTERRUPT(canTxIsrHandler, 0, ISR_PRIORITY_CAN_TX);
void canTxIsrHandler(void)
{
    /* Clear the "Transmission Completed" interrupt flag */
    IfxCan_Node_clearInterruptFlag(g_mcmcan.canSrcNode.node, IfxCan_Interrupt_transmissionCompleted);

    /* Send the next consecutive frame if multi-frame transmission is in progress */
    sendConsecutiveFrame();
}

/* Default CAN Rx Handler */
IFX_INTERRUPT(canRxIsrHandler, 0, ISR_PRIORITY_CAN_RX);
void canRxIsrHandler(void)
{
    unsigned int rxID;
    unsigned char rxData[8] = {0, };
    int rxLen;
    canRecvMsg(&rxID, rxData, &rxLen);

    switch (rxID)
    {
        case CAN_TOF_ID: // ToF Sensor Data (0x200)
            if (tofCallback != NULL)
            {
                tofCallback(rxData); // ToF 모듈에서 등록한 처리 함수 호출
            }
            break;

        case UDS_REQUEST_CAN_ID: // UDS Diagnostic Request (0x7E0)
        {
            // --- 세션 타임아웃 리셋 로직 (기존 코드 유지) ---
            // 0x22 F186 (ActiveDiagnosticSession) 요청은 세션 상태 확인이므로 타이머를 리셋하지 않음
            bool isSessionStatusPoll = (rxLen >= 4 && rxData[0] == 0x03 && rxData[1] == 0x22 && rxData[2] == 0xF1 && rxData[3] == 0x86);
            if (!isSessionStatusPoll)
            {
                session_resetTimer();
            }

            // --- CAN-TP 프레임 타입 분석 및 처리 위임 ---
            uint8 pci_type = (rxData[0] & 0xF0) >> 4; // Extract frame type

            switch (pci_type)
            {
                case 0: // Single Frame (SF)
                    handleSingleFrame(rxData);
                    break;
                case 1: // First Frame (FF)
                    handleFirstFrame(rxData);
                    break;
                case 2: // Consecutive Frame (CF)
                    handleConsecutiveFrame(rxData);
                    break;
                case 3: // Flow Control Frame (FC)
                    handleFlowControl(rxData);
                    break;
            }
            break;
        }

        case 0x123: // RPi -> ECU Control Frame
        {
            if (rxLen < 1) return; // Minimum length check

            const uint8 cmd = rxData[0];
            switch (cmd)
            {
                case 0x10: /* SET_SPEED: Byte1 = PWM% (0~100) */
                {
                    if (rxLen >= 2)
                    {
                        uint8 pct = rxData[1];
                        g_drive_speed = pct;
                        // TODO: 실제 모터 PWM 함수가 있다면 호출하세요 (예: Motor_SetPwm(pct);)
                        sendDriveStateEvent(); // E0 [speed][dir]
                        myPrintf("SET_SPEED: %u\n", pct);
                    }
                    break;
                }
                case 0x11: /* SET_DIR: Byte1 = 방향코드 ('8','2','4','6','7','9','1','3') */
                {
                    if (rxLen >= 2)
                    {
                        uint8 dir = rxData[1];
                        g_drive_dir = dir;
                        // TODO: 실제 방향 제어 함수 (예: Drive_SetDir(dir);)
                        sendDriveStateEvent();
                        const char *text = dirToText(dir);
                        myPrintf("SET_DIR: %s (raw=0x%02X)\n", text, dir);
                    }
                    break;
                }
                // --- (기존의 다른 case 문들은 여기에 그대로 유지) ---
                case 0x12: myPrintf("SOFT_STOP\n"); break;
                case 0x20: myPrintf("AEB_ENABLE\n"); break;
                case 0x21: myPrintf("AEB_STATUS\n"); break;
                case 0x30: myPrintf("FCW_WARN\n"); break;
                case 0x40: myPrintf("AUTOPARK_START\n"); break;
                case 0x41: myPrintf("AUTOPARK_CANCEL\n"); break;
                case 0x42: myPrintf("AUTOPARK_STATUS\n"); break;
                case 0x50: myPrintf("AUTH_LOGIN_PWD\n"); break;
                case 0x60: myPrintf("SYS_HEARTBEAT\n"); break;
                default:
                    myPrintf("Unknown Command from RPi\n");
                    break;
            }
            break;
        }
        default:
            // Handle other CAN IDs if necessary
            break;
    }
}


/* Function to initialize MCMCAN module and nodes */
void canInit(CAN_BAUDRATES ls_baudrate, CAN_NODE CAN_Node)
{
    /* wake up transceiver (node 0) */
    IfxPort_setPinModeOutput(&MODULE_P20, 6, IfxPort_OutputMode_pushPull, IfxPort_OutputIdx_general);
    MODULE_P20.OUT.B.P6 = 0;

    IfxCan_Can_initModuleConfig(&g_mcmcan.canConfig, &MODULE_CAN0);
    IfxCan_Can_initModule(&g_mcmcan.canModule, &g_mcmcan.canConfig);
    IfxCan_Can_initNodeConfig(&g_mcmcan.canNodeConfig, &g_mcmcan.canModule);

    switch (ls_baudrate)
    {
        case BD_NOUSE:
            g_mcmcan.canNodeConfig.busLoopbackEnabled = TRUE;
            break;
        case BD_500K:
            g_mcmcan.canNodeConfig.baudRate.baudrate = 500000;
            break;
        case BD_1M:
            g_mcmcan.canNodeConfig.baudRate.baudrate = 1000000;
            break;
    }

    g_mcmcan.canNodeConfig.busLoopbackEnabled = FALSE;

    if (CAN_Node == CAN_NODE0)
    { /* CAN Node 0 for lite kit */
        g_mcmcan.canNodeConfig.nodeId = IfxCan_NodeId_0;
        const IfxCan_Can_Pins pins = {&IfxCan_TXD00_P20_8_OUT, IfxPort_OutputMode_pushPull,
                                      &IfxCan_RXD00B_P20_7_IN, IfxPort_InputMode_pullUp,
                                      IfxPort_PadDriver_cmosAutomotiveSpeed1};
        g_mcmcan.canNodeConfig.pins = &pins;
    }
    else if (CAN_Node == CAN_NODE2)
    { /* CAN Node 2 for mikrobus */
        g_mcmcan.canNodeConfig.nodeId = IfxCan_NodeId_2;
        const IfxCan_Can_Pins pins = {&IfxCan_TXD02_P15_0_OUT, IfxPort_OutputMode_pushPull,
                                      &IfxCan_RXD02A_P15_1_IN, IfxPort_InputMode_pullUp,
                                      IfxPort_PadDriver_cmosAutomotiveSpeed1};
        g_mcmcan.canNodeConfig.pins = &pins;
    }

    g_mcmcan.canNodeConfig.frame.type = IfxCan_FrameType_transmitAndReceive;
    g_mcmcan.canNodeConfig.interruptConfig.transmissionCompletedEnabled = TRUE;
    g_mcmcan.canNodeConfig.interruptConfig.traco.priority = ISR_PRIORITY_CAN_TX;
    g_mcmcan.canNodeConfig.interruptConfig.traco.interruptLine = IfxCan_InterruptLine_0;
    g_mcmcan.canNodeConfig.interruptConfig.traco.typeOfService = IfxSrc_Tos_cpu0;
    IfxCan_Can_initNode(&g_mcmcan.canSrcNode, &g_mcmcan.canNodeConfig);

    /* Reception handling configuration */
    g_mcmcan.canNodeConfig.rxConfig.rxMode = IfxCan_RxMode_sharedFifo0;
    g_mcmcan.canNodeConfig.rxConfig.rxBufferDataFieldSize = IfxCan_DataFieldSize_8;
    g_mcmcan.canNodeConfig.rxConfig.rxFifo0DataFieldSize = IfxCan_DataFieldSize_8;
    g_mcmcan.canNodeConfig.rxConfig.rxFifo0Size = 15;
    g_mcmcan.canNodeConfig.filterConfig.messageIdLength = IfxCan_MessageIdLength_standard;
    g_mcmcan.canNodeConfig.filterConfig.standardListSize = 8;
    g_mcmcan.canNodeConfig.filterConfig.standardFilterForNonMatchingFrames = IfxCan_NonMatchingFrame_reject;
    g_mcmcan.canNodeConfig.filterConfig.rejectRemoteFramesWithStandardId = TRUE;
    g_mcmcan.canNodeConfig.interruptConfig.rxFifo0NewMessageEnabled = TRUE;
    g_mcmcan.canNodeConfig.interruptConfig.rxf0n.priority = ISR_PRIORITY_CAN_RX;
    g_mcmcan.canNodeConfig.interruptConfig.rxf0n.interruptLine = IfxCan_InterruptLine_1;
    g_mcmcan.canNodeConfig.interruptConfig.rxf0n.typeOfService = IfxSrc_Tos_cpu0;
    IfxCan_Can_initNode(&g_mcmcan.canDstNode, &g_mcmcan.canNodeConfig);

    /* Rx filter configuration (default: all messages accepted) */
    canSetFilterRange(0x0, 0x7FF);
}

void canSetFilterRange(uint32 start, uint32 end)
{
    g_mcmcan.canFilter.number = 0;
    g_mcmcan.canFilter.type = IfxCan_FilterType_range;
    g_mcmcan.canFilter.elementConfiguration = IfxCan_FilterElementConfiguration_storeInRxFifo0;
    g_mcmcan.canFilter.id1 = start;
    g_mcmcan.canFilter.id2 = end;
    IfxCan_Can_setStandardFilter(&g_mcmcan.canDstNode, &g_mcmcan.canFilter);
}

void canSetFilterMask(uint32 id, uint32 mask)
{
    g_mcmcan.canFilter.number = 0;
    g_mcmcan.canFilter.type = IfxCan_FilterType_classic;
    g_mcmcan.canFilter.elementConfiguration = IfxCan_FilterElementConfiguration_storeInRxFifo0;
    g_mcmcan.canFilter.id1 = id;
    g_mcmcan.canFilter.id2 = mask;
    IfxCan_Can_setStandardFilter(&g_mcmcan.canDstNode, &g_mcmcan.canFilter);
}

void canSendMsg(unsigned int id, const char *txData, int len)
{
    IfxCan_Can_initMessage(&g_mcmcan.txMsg);
    g_mcmcan.txMsg.messageId = id;
    g_mcmcan.txMsg.dataLengthCode = len;

    memcpy(g_mcmcan.txData, txData, len);

    while (IfxCan_Status_notSentBusy == IfxCan_Can_sendMessage(&g_mcmcan.canSrcNode, &g_mcmcan.txMsg, (uint32*)g_mcmcan.txData))
    {
    }
}

int canRecvMsg(unsigned int *id, unsigned char *rxData, int *len)
{
    IfxCan_Node_clearInterruptFlag(g_mcmcan.canDstNode.node, IfxCan_Interrupt_rxFifo0NewMessage);
    g_mcmcan.rxMsg.readFromRxFifo0 = TRUE;
    g_mcmcan.rxMsg.readFromRxFifo1 = FALSE;

    IfxCan_Can_readMessage(&g_mcmcan.canDstNode, &g_mcmcan.rxMsg, (uint32*)g_mcmcan.rxData);

    *id = g_mcmcan.rxMsg.messageId;
    memcpy(rxData, g_mcmcan.rxData, g_mcmcan.rxMsg.dataLengthCode);
    *len = g_mcmcan.rxMsg.dataLengthCode;

    return 0;
}


/*********************************************************************************************************************/
/*-------------------------------------------Private Function Implementations----------------------------------------*/
/*********************************************************************************************************************/

static const char* dirToText(uint8 dirByte)
{
    uint8 d = dirByte;
    if (d >= '0' && d <= '9') d = (uint8)(d - '0');
    switch (d)
    {
        case 8: return "전진";
        case 2: return "후진";
        case 4: return "좌";
        case 6: return "우";
        case 7: return "좌전진";
        case 9: return "우전진";
        case 1: return "좌후진";
        case 3: return "우후진";
        case 5: return "정지";
        default: return "알수없음";
    }
}

static inline void canSend8(uint32 id, const uint8 *d)
{
    canSendMsg(id, (const char*)d, 8);
}

static inline void canSendEvent(uint8 eid, const uint8 *payload, uint8 len)
{
    uint8 tx[8] = {0};
    tx[0] = eid;
    if(len > 7) len = 7;
    memcpy(&tx[1], payload, len);
    canSend8(0x124, tx);
}

static inline void sendDriveStateEvent(void)
{
    uint8 data[2] = {g_drive_speed, g_drive_dir};
    canSendEvent(0xE0, data, 2);
}
