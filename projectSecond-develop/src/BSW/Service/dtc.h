#ifndef DTC_H_
#define DTC_H_

#include "Ifx_Types.h"
#include <stdbool.h>

// --- 수정된 부분: 이 정의도 헤더 파일로 이동 ---
#define MAX_DTCS 10

// 1. 진단할 DTC 코드 정의
// Tof 센서
#define DTC_TOF_TIMEOUT     0xC10000 // U0100: ToF 센서 통신 두절
#define DTC_TOF_OUTOFRANGE  0x810000 // B0100: ToF 센서 측정값 범위 초과

// 초음파 센서(좌)
#define DTC_ULT_LEFT_TIMEOUT    0xC20000 // U0200 : Left_Ultrasonic 센서 통신 두절
#define DTC_ULT_LEFT_OUTOFRANGE 0x820000 // B0200 : Left_Ultrasonic 센서 측정값 범위 초과

// 초음파 센서(우)
#define DTC_ULT_RIGHT_TIMEOUT    0xC20001 // U0201 : Right_Ultrasonic 센서 통신 두절
#define DTC_ULT_RIGHT_OUTOFRANGE 0x820001 // B0201 : Right_Ultrasonic 센서 측정값 범위 초과

// 초음파 센서(후방)
#define DTC_ULT_REAR_TIMEOUT    0xC20002 // U0202 : Rear_Ultrasonic 센서 통신 두절
#define DTC_ULT_REAR_OUTOFRANGE 0x820002 // B0202 : Rear_Ultrasonic 센서 측정값 범위 초과

// 2. DTC 상태 비트 마스크 정의
#define DTC_STATUS_TEST_FAILED (1 << 0)

// 3. DTC 기록을 저장할 구조체
typedef struct {
    uint32 dtc_code;
    uint8  status;
} DtcRecord;

// --- 수정된 부분 ---
// "g_dtcStorage라는 DtcRecord 타입의 배열이 다른 파일(dtc.c)에 실제로 존재합니다"
// 라고 컴파일러에게 알려주는 '명함' 역할을 합니다.
extern DtcRecord g_dtcStorage[MAX_DTCS];
// --- 수정된 부분 끝 ---


// 4. 외부에서 사용할 함수 선언
void dtc_updateStatus(uint32 dtc_code, bool is_faulty);

#endif /* DTC_H_ */
