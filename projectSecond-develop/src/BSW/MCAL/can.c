#include "can.h"
#include <string.h>



/*********************************************************************************************************************/
/*-------------------------------------------------Global variables--------------------------------------------------*/
/*********************************************************************************************************************/
McmcanType g_mcmcan; /* Global MCMCAN configuration and control structure    */

// ★ 추가: 간단한 주행 상태 캐시 (속도/방향)
static uint8 g_drive_speed = 0;   // 0~100 (%)
static uint8 g_drive_dir = 5;   // 설계서의 초기값(정지:5)

static const char* dirToText (uint8 dirByte)
{
    // ASCII('8')도, 숫자(8)도 모두 대응
    uint8 d = dirByte;
    if (d >= '0' && d <= '9')
        d = (uint8) (d - '0');

    switch (d)
    {
        case 8 :
            return "전진";
        case 2 :
            return "후진";
        case 4 :
            return "좌";
        case 6 :
            return "우";
        case 7 :
            return "좌전진";
        case 9 :
            return "우전진";
        case 1 :
            return "좌후진";
        case 3 :
            return "우후진";
        case 5 :
            return "정지";
        default :
            return "알수없음";
    }
}

/*********************************************************************************************************************/
/*---------------------------------------------Function Implementations----------------------------------------------*/
/*********************************************************************************************************************/

/* Callback 함수 포인터 */
static void (*tofCallback) (unsigned char *rxData) = 0;

void canRegisterTofCallback (void (*callback) (unsigned char*))
{
    tofCallback = callback;
}

/* Default CAN Tx Handler */
IFX_INTERRUPT(canTxIsrHandler, 0, ISR_PRIORITY_CAN_TX);
void canTxIsrHandler (void)
{
    /* Clear the "Transmission Completed" interrupt flag */
    IfxCan_Node_clearInterruptFlag(g_mcmcan.canSrcNode.node, IfxCan_Interrupt_transmissionCompleted);
}

static IsoTpTxCtx g_isotp_tx;


/* ---- 헬퍼: CAN 보냄 ---- */
static inline void canSend8 (uint32 id, const uint8 *d)
{
    canSendMsg(id, (const char*) d, 8);
}

// ★ 추가: RPi로 이벤트(E0~E5) 보내는 헬퍼 (STD ID 0x124)
static inline void canSendEvent (uint8 eid, const uint8 *payload, uint8 len)
{
    uint8 tx[8] = {0};
    tx[0] = eid;                          // Byte0 = EID (0xE0~0xE5)
    for (uint8 i = 0; i < len && i < 7; ++i)
        tx[1 + i] = payload[i];           // Byte1.. = 데이터(최대 7B)
    canSend8(0x124, tx);
}

// ★ 추가: drive 상태 이벤트(E0) 발행
static inline void sendDriveStateEvent (void)
{
    uint8 data[2] = {g_drive_speed, g_drive_dir}; // Byte0=speed, Byte1=dir
    canSendEvent(0xE0, data, 2);
}




/* ---- RX ISR ---- */
// ==========================================================================
/* ---- 센서 값 획득 함수들 (외부 선언) ---- */
extern unsigned int tofGetValue (void);
extern int getDistanceByUltra (UltraDir dir);

/* ==========================================================================
 * 최종 RX ISR: 모든 CAN 메시지를 수신하여 분배하는 교통정리 담당관
 * ========================================================================== */
IFX_INTERRUPT(canRxIsrHandler, 0, ISR_PRIORITY_CAN_RX);
void canRxIsrHandler (void)
{
    unsigned int rxID;
    unsigned char rxData[8] = {0, };
    int rxLen;

    canRecvMsg(&rxID, rxData, &rxLen);

    // --- ID를 보고 교통정리 시작 ---
    if (rxID == 0x7E0) // UDS 진단 요청 ID인가?
    {
        // 0x7E0이면 세션 연장해줘야함
        // 근데 0x22 F186은 세션 상태 확인인데 얘도 들어가면 시간 초기화되니까 얘는 예외 처리 해줘야함
        bool isSessionStatusPoll = (rxData[1] == 0x22 && rxData[2] == 0xF1 && rxData[3] == 0x86);

        // '세션 상태 확인' 요청이 아닐 경우에만 세션 타이머를 리셋합니다.
        if (!isSessionStatusPoll)
        {
            session_resetTimer();
        }

        // UDS 전문 담당자를 호출합니다.
        udsHandler(rxData, rxLen);
    }
    else if (rxID == 0x200) // ToF 센서 데이터 ID인가?
    {
        // ToF 전문 담당자(tof.c의 함수)를 호출합니다.
        tofUpdateFromCAN(rxData);
    }
    else if (rxID == 0x123) // ★ RPi -> ECU 제어 명령 프레임
    {
        if (rxLen < 1)
            return;              // 최소 길이 확인
        const uint8 cmd = rxData[0];

        switch (cmd)
        {
            case 0x10 : /* SET_SPEED: Byte1 = PWM% (0~100) */
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

            case 0x11 : /* SET_DIR: Byte1 = 방향코드 ('8','2','4','6','7','9','1','3') */
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

            case 0x12 : /* SOFT_STOP */
            {
                if (rxLen >= 2)
                {
                    g_drive_speed = 0;
                    // TODO: 실제 감속/정지 함수 (예: Drive_SoftStop();)
                    sendDriveStateEvent();
                    myPrintf("SOFT_STOP\n");
                }
                break;
            }

            case 0x20 : /* AEB_ENABLE: Byte1 = 0/1 */
            {
                if (rxLen >= 2)
                {
                    uint8 onoff = rxData[1] ? 1 : 0;
                    // 프로젝트에 이미 있는 마스터 스위치 사용
                    g_config.isAebEnabled = onoff;
                    // AEB 상태 이벤트(E1): 설계서대로 0:OFF, 1:ON, 2:DECEL/STOP 등 필요 시 확장
                    uint8 st = onoff;
                    canSendEvent(0xE1, &st, 1);
                    myPrintf("AEB_ENABLE: %s\n", onoff ? "ON" : "OFF");
                }
                break;
            }

            case 0x21 : /* AEB_STATUS */
            {
                if (rxLen >= 2)
                {
                    // 현재 상태를 그대로 반송 (간단 구현)
                    uint8 st = g_config.isAebEnabled ? 1 : 0;
                    canSendEvent(0xE1, &st, 1);
                    myPrintf("AEB_STATUS: %s\n", st ? "ON" : "OFF");
                }
                break;
            }

            case 0x30 : /* FCW_WARN (부저/LED 경고 트리거) */
            {
                if (rxLen >= 2)
                {
                    // TODO: 실제 부저/LED 경고 트리거
                    uint8 alert = 1;
                    canSendEvent(0xE2, &alert, 1); // FCW Alert 이벤트
                    myPrintf("FCW_WARN\n");
                }
                break;
            }

            case 0x40 : /* AUTOPARK_START */
            {
                if (rxLen >= 2)
                {
                    // TODO: 자율주차 FSM 시작
                    // 진행률 이벤트 예시 발행 (필요시 FSM에서 주기적으로 보내세요)
                    uint8 pr = 0;
                    canSendEvent(0xE3, &pr, 1);
                    myPrintf("AUTOPARK_START\n");
                }
                break;
            }

            case 0x41 : /* AUTOPARK_CANCEL */
            {
                if (rxLen >= 2)
                {
                    // TODO: 자율주차 FSM 취소
                    uint8 pr = 0;
                    canSendEvent(0xE3, &pr, 1);
                    myPrintf("AUTOPARK_CANCEL\n");
                }
                break;
            }

            case 0x42 : /* AUTOPARK_STATUS */
            {
                if (rxLen >= 2)
                {
                    // TODO: 실제 진행률 조회 함수로 교체 (예: Autopark_Progress())
                    uint8 pr = 60;
                    canSendEvent(0xE3, &pr, 1);
                    myPrintf("AUTOPARK_STATUS: %u%%\n", pr);
                }
                break;
            }

            case 0x50 : /* AUTH_LOGIN: Byte1..4 = ASCII PW (최대 4B) */
            {
                if (rxLen >= 2)
                {
                    char pwd[5] = {0, 0, 0, 0, 0};
                    uint8 copy = (rxLen - 1) > 4 ? 4 : (rxLen - 1); // ★ 실제 받은 길이만큼만
                    for (int i = 0; i < copy; i++)
                        pwd[i] = rxData[1 + i];
                    pwd[copy] = '\0';

                    uint8 ok = (strcmp(pwd, "1234") == 0) ? 1 : 0;
                    // TODO: 시스템 인증 상태 갱신
                    canSendEvent(0xE4, &ok, 1);

                    myPrintf("AUTH_LOGIN_PWD: \"%s\"  [", pwd);
                    for (uint8 i = 0; i < copy; ++i)
                        myPrintf("%s0x%02X", (i ? ", " : ""), (uint8) pwd[i]);
                    myPrintf("]\n");
                    myPrintf("AUTH_LOGIN_STATUS: %s\n", ok ? "OK" : "NOT OK");
                }
                break;
            }

            case 0x60 : /* SYS_HEARTBEAT */
            {
                if (rxLen >= 2)
                {
                    // 필요하면 아무 동작 없이도 OK
                    uint8 hb = 0x00;
                    canSendEvent(0xE4 /*또는 별도 EID 필요시*/, &hb, 1);
                    myPrintf("SYS_HEARTBEAT\n");
                }
                break;
            }

            default :
                // 알 수 없는 명령은 무시 (원하면 0x90 ACK 프레임 규격 추가 가능)
                myPrintf("Unknown Command\n");
                break;
        }
    }
    // else if (rxID == 0xXXX) { ... }
    // 나중에 다른 CAN ID를 사용하는 기능이 추가되면 여기에 분기문만 추가하면 됩니다.
}
/* Function to initialize MCMCAN module and nodes related for this application use case */
void canInit (CAN_BAUDRATES ls_baudrate, CAN_NODE CAN_Node)
{
    /* wake up transceiver (node 0) */
    IfxPort_setPinModeOutput(&MODULE_P20, 6, IfxPort_OutputMode_pushPull, IfxPort_OutputIdx_general);
    MODULE_P20.OUT.B.P6 = 0;

    IfxCan_Can_initModuleConfig(&g_mcmcan.canConfig, &MODULE_CAN0);
    IfxCan_Can_initModule(&g_mcmcan.canModule, &g_mcmcan.canConfig);
    IfxCan_Can_initNodeConfig(&g_mcmcan.canNodeConfig, &g_mcmcan.canModule);

    switch (ls_baudrate)
    {
        case BD_NOUSE :
            g_mcmcan.canNodeConfig.busLoopbackEnabled = TRUE;
            break;
        case BD_500K :
            g_mcmcan.canNodeConfig.baudRate.baudrate = 500000;
            break;
        case BD_1M :
            g_mcmcan.canNodeConfig.baudRate.baudrate = 1000000;
            break;
    }

    g_mcmcan.canNodeConfig.busLoopbackEnabled = FALSE;

    if (CAN_Node == CAN_NODE0)
    { /* CAN Node 0 for lite kit */
        g_mcmcan.canNodeConfig.nodeId = IfxCan_NodeId_0;
        const IfxCan_Can_Pins pins = {&IfxCan_TXD00_P20_8_OUT, IfxPort_OutputMode_pushPull, /* TX Pin for lite kit (can node 0) */
        &IfxCan_RXD00B_P20_7_IN, IfxPort_InputMode_pullUp, /* RX Pin for lite kit (can node 0) */
        IfxPort_PadDriver_cmosAutomotiveSpeed1};
        g_mcmcan.canNodeConfig.pins = &pins;
    }
    else if (CAN_Node == CAN_NODE2)
    { /* CAN Node 2 for mikrobus */
        g_mcmcan.canNodeConfig.nodeId = IfxCan_NodeId_2;
        const IfxCan_Can_Pins pins = {&IfxCan_TXD02_P15_0_OUT, IfxPort_OutputMode_pushPull, /* TX Pin for mikrobus (can node 2) */
        &IfxCan_RXD02A_P15_1_IN, IfxPort_InputMode_pullUp, /* RX Pin for mikrobus (can node 2) */
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
    /* General filter configuration */
    g_mcmcan.canNodeConfig.filterConfig.messageIdLength = IfxCan_MessageIdLength_standard;
    g_mcmcan.canNodeConfig.filterConfig.standardListSize = 8;
    g_mcmcan.canNodeConfig.filterConfig.standardFilterForNonMatchingFrames = IfxCan_NonMatchingFrame_reject;
    g_mcmcan.canNodeConfig.filterConfig.rejectRemoteFramesWithStandardId = TRUE;
    /* Interrupt configuration */
    g_mcmcan.canNodeConfig.interruptConfig.rxFifo0NewMessageEnabled = TRUE;
    g_mcmcan.canNodeConfig.interruptConfig.rxf0n.priority = ISR_PRIORITY_CAN_RX;
    g_mcmcan.canNodeConfig.interruptConfig.rxf0n.interruptLine = IfxCan_InterruptLine_1;
    g_mcmcan.canNodeConfig.interruptConfig.rxf0n.typeOfService = IfxSrc_Tos_cpu0;
    IfxCan_Can_initNode(&g_mcmcan.canDstNode, &g_mcmcan.canNodeConfig);

    /* Rx filter configuration (default: all messages accepted) */
    canSetFilterRange(0x0, 0x7FF);
}

void canSetFilterRange (uint32 start, uint32 end)
{
    g_mcmcan.canFilter.number = 0;
    g_mcmcan.canFilter.type = IfxCan_FilterType_range;
    g_mcmcan.canFilter.elementConfiguration = IfxCan_FilterElementConfiguration_storeInRxFifo0;
    g_mcmcan.canFilter.id1 = start;
    g_mcmcan.canFilter.id2 = end;
    IfxCan_Can_setStandardFilter(&g_mcmcan.canDstNode, &g_mcmcan.canFilter);
}

void canSetFilterMask (uint32 id, uint32 mask)
{
    g_mcmcan.canFilter.number = 0;
    g_mcmcan.canFilter.type = IfxCan_FilterType_classic;
    g_mcmcan.canFilter.elementConfiguration = IfxCan_FilterElementConfiguration_storeInRxFifo0;
    g_mcmcan.canFilter.id1 = id;
    g_mcmcan.canFilter.id2 = mask;
    IfxCan_Can_setStandardFilter(&g_mcmcan.canDstNode, &g_mcmcan.canFilter);
}

void canSendMsg (unsigned int id, const char *txData, int len)
{
    /* Initialization of the TX message with the default configuration */
    IfxCan_Can_initMessage(&g_mcmcan.txMsg);

    g_mcmcan.txMsg.messageId = id;
    g_mcmcan.txMsg.dataLengthCode = len;

    /* Define the content of the data to be transmitted */
    for (int i = 0; i < 8; i++)
    {
        g_mcmcan.txData[i] = txData[i];
    }

    /* Send the CAN message with the previously defined TX message content */
    while (IfxCan_Status_notSentBusy
            == IfxCan_Can_sendMessage(&g_mcmcan.canSrcNode, &g_mcmcan.txMsg, (uint32*) &g_mcmcan.txData[0]))
    {
    }
}

int canRecvMsg (unsigned int *id, unsigned char *rxData, int *len)
{
    int err = 0;
    /* Clear the "RX FIFO 0 new message" interrupt flag */
    IfxCan_Node_clearInterruptFlag(g_mcmcan.canDstNode.node, IfxCan_Interrupt_rxFifo0NewMessage);

    /* Received message content should be updated with the data stored in the RX FIFO 0 */
    g_mcmcan.rxMsg.readFromRxFifo0 = TRUE;
    g_mcmcan.rxMsg.readFromRxFifo1 = FALSE;

    /* Read the received CAN message */
    IfxCan_Can_readMessage(&g_mcmcan.canDstNode, &g_mcmcan.rxMsg, (uint32*) &g_mcmcan.rxData);

    *id = g_mcmcan.rxMsg.messageId;
    for (int i = 0; i < 8; i++)
    {
        rxData[i] = g_mcmcan.rxData[i];
    }
    *len = g_mcmcan.rxMsg.dataLengthCode;

    return err;
}
