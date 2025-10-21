#ifndef BSW_MCAL_CAN_H_
#define BSW_MCAL_CAN_H_


#include "IfxPort.h"
#include "IfxCan.h"
#include "IfxCan_Can.h"


#include "priority.h"
#include "tof.h"
#include "soa_handler.h"

/*********************************************************************************************************************/
/*------------------------------------------------------Macros-------------------------------------------------------*/
/*********************************************************************************************************************/
#define CAN_MESSAGE_ID              (uint32)0x777           /* Message ID that will be used in arbitration phase    */
#define MAXIMUM_CAN_DATA_PAYLOAD    2                       /* Define maximum classical CAN payload in 4-byte words */

/*********************************************************************************************************************/
/*------------------------------------------------------CAN ID-------------------------------------------------------*/
/*********************************************************************************************************************/
/* ID 값이 작을수록 우선순위(Priority)가 높음 */
/* 진단 (UDS / ISO 14229) */
#define UDS_REQUEST_CAN_ID          0x7E0

/* 서비스 지향 제어 (SOA: Service-Oriented Architecture) */
#define CAN_SOA_CONTROL_ID          0x300    // SOME/IP 기반 제어 명령 (Drive, AEB, Auth 등)
#define CAN_SOA_STATUS_ID           0x310    // SOME/IP 기반 상태 publish

/* 센서 피드백 */
#define CAN_TOF_ID                  0x200    // ToF 거리 데이터 전송

/*********************************************************************************************************************/
/*--------------------------------------------------Data Structures--------------------------------------------------*/
/*********************************************************************************************************************/
typedef struct
{
    IfxCan_Can_Config canConfig;                            /* CAN module configuration structure                   */
    IfxCan_Can canModule;                                   /* CAN module handle                                    */
    IfxCan_Can_Node canSrcNode;                             /* CAN source node handle data structure                */
    IfxCan_Can_Node canDstNode;                             /* CAN destination node handle data structure           */
    IfxCan_Can_NodeConfig canNodeConfig;                    /* CAN node configuration structure                     */
    IfxCan_Filter canFilter;                                /* CAN filter configuration structure                   */
    IfxCan_Message txMsg;                                   /* Transmitted CAN message structure                    */
    IfxCan_Message rxMsg;                                   /* Received CAN message structure                       */
    uint8 txData[8];                                        /* Transmitted CAN data array                           */
    uint8 rxData[8];                                        /* Received CAN data array                              */
} McmcanType;

typedef enum {
    BD_NOUSE = 0,
    BD_500K = 1,
    BD_1M = 2
} CAN_BAUDRATES;

typedef enum {
    CAN_NODE0 = 0, /* CAN Node 0 for lite kit */
    CAN_NODE2 = 2  /* CAN Node 2 for mikrobus */
} CAN_NODE;

/*********************************************************************************************************************/
/*-----------------------------------------------Function Prototypes-------------------------------------------------*/
/*********************************************************************************************************************/
void canRegisterTofCallback(void (*callback)(unsigned char *));

void canInit(CAN_BAUDRATES ls_baudrate, CAN_NODE CAN_Node);
void canSetFilterRange(uint32 start, uint32 end);
void canSetFilterMask(uint32 id, uint32 mask);

void canSendMsg(unsigned int id, const unsigned char *txData, int len);
int canRecvMsg (unsigned int *id, unsigned char *rxData, int *len);


#endif
