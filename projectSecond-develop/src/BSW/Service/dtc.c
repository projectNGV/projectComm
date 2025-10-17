#include "dtc.h" // dtc.h의 모든 선언과 정의(MAX_DTCS 등)를 가져옴

// --- 1. DTC 저장소의 '실체' (정의) ---
// dtc.h의 extern 선언에 대응하는 실제 메모리 공간입니다.
DtcRecord g_dtcStorage[MAX_DTCS] = {
    { DTC_TOF_TIMEOUT,    0x00 }, // 초기 상태: 고장 없음
    { DTC_TOF_OUTOFRANGE, 0x00 },
    { DTC_ULT_LEFT_TIMEOUT, 0x00 },
    { DTC_ULT_LEFT_OUTOFRANGE, 0x00 },
    { DTC_ULT_RIGHT_TIMEOUT, 0x00 },
    { DTC_ULT_RIGHT_OUTOFRANGE, 0x00 },
    { DTC_ULT_REAR_TIMEOUT, 0x00 },
    { DTC_ULT_REAR_OUTOFRANGE, 0x00 },

};

// --- 2. DTC 상태 업데이트 함수의 '실체' (구현) ---
// dtc.h에 선언된 함수의 실제 동작 내용입니다.
void dtc_updateStatus(uint32 dtc_code, bool is_faulty) {
    for (int i = 0; i < MAX_DTCS; i++) {
        // 장부(배열)에서 해당 DTC 코드를 찾습니다.
        if (g_dtcStorage[i].dtc_code == dtc_code) {
            if (is_faulty) {
                // 고장이 발생했다면, status의 'testFailed' 비트를 1로 설정합니다.
                g_dtcStorage[i].status |= DTC_STATUS_TEST_FAILED;
            } else {
                // 고장이 아니라면, 'testFailed' 비트를 0으로 되돌립니다.
                g_dtcStorage[i].status &= ~DTC_STATUS_TEST_FAILED;
            }
            break; // 해당 DTC를 찾았으므로 루프 종료
        }
    }
}
