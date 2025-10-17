#include "isotp.h"
#include "can.h" // '배송 기사(canSend8)'를 호출하기 위해 포함
#include <string.h>

// --- 전역 변수의 '실체' ---
IsoTpTxCtx g_isotp_tx;


extern void canSend8(uint32 id, const uint8 *d);

/* ---- 헬퍼: UDS 응답 페이로드를 ISO-TP로 송신 시작 ---- */
void isotp_send_response (uint32 canId, const uint8 *payload, uint16 plen)
{
    // (기존 can.c에 있던 코드와 동일)
    uint8 tx[8] = {0};
    if (plen <= 7) {
        tx[0] = (uint8) (plen & 0x0F);
        for (uint8 i = 0; i < plen; i++) tx[1 + i] = payload[i];
        canSend8(canId, tx);
        g_isotp_tx.active = 0;
        return;
    }
    tx[0] = 0x10 | ((plen >> 8) & 0x0F);
    tx[1] = (uint8) (plen & 0xFF);
    uint8 ff_data = (plen >= 6) ? 6 : (uint8) plen;
    for (uint8 i = 0; i < ff_data; i++) tx[2 + i] = payload[i];
    canSend8(canId, tx);
    memset(&g_isotp_tx, 0, sizeof(g_isotp_tx));
    memcpy(g_isotp_tx.buf, payload, plen);
    g_isotp_tx.len = plen;
    g_isotp_tx.off = ff_data;
    g_isotp_tx.sn = 1;
    g_isotp_tx.active = 1;
    g_isotp_tx.waiting_fc = 1;
}

/* ---- 헬퍼: FC(CTS) 받은 뒤 CF를 한 블록 보내기 ---- */
void isotp_send_next_block (uint32 canId)
{
    // (기존 can.c에 있던 코드와 동일)
    if (!g_isotp_tx.active || g_isotp_tx.waiting_fc) return;
    uint8 tx[8];
    uint8 to_send = (g_isotp_tx.bs == 0) ? 0xFF : g_isotp_tx.bs;
    while ((g_isotp_tx.off < g_isotp_tx.len) && (to_send > 0)) {
        uint16 remain = g_isotp_tx.len - g_isotp_tx.off;
        uint8 chunk = (remain >= 7) ? 7 : (uint8) remain;
        memset(tx, 0, sizeof(tx));
        tx[0] = 0x20 | (g_isotp_tx.sn & 0x0F);
        for (uint8 i = 0; i < chunk; i++) tx[1 + i] = g_isotp_tx.buf[g_isotp_tx.off + i];
        canSend8(canId, tx);
        g_isotp_tx.off += chunk;
        g_isotp_tx.sn = (g_isotp_tx.sn % 15) + 1;
        if (g_isotp_tx.bs != 0) {
            to_send--;
            if (to_send == 0 && g_isotp_tx.off < g_isotp_tx.len) {
                g_isotp_tx.waiting_fc = 1;
                break;
            }
        }
    }
    if (g_isotp_tx.off >= g_isotp_tx.len) {
        g_isotp_tx.active = 0;
    }
}
