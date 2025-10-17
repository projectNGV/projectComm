#ifndef ISOTP_H_
#define ISOTP_H_

#include "Ifx_Types.h"

/* ---- ISO-TP TX 상태 구조체 정의 ---- */
typedef struct
{
    uint8 buf[256];
    uint16 len;
    uint16 off;
    uint8 sn;
    uint8 bs;
    uint8 bs_cnt;
    uint8 stmin;
    uint8 active;
    uint8 waiting_fc;
} IsoTpTxCtx;

// --- 외부 공개 변수 및 함수 선언 ---
extern IsoTpTxCtx g_isotp_tx; // 다른 파일에서 이 변수에 접근할 수 있도록 extern 선언

void isotp_send_response(uint32 canId, const uint8 *payload, uint16 plen);
void isotp_send_next_block(uint32 canId);

#endif /* ISOTP_H_ */
