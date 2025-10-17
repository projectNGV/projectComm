// 파일명: config.h

#ifndef CONFIG_H_
#define CONFIG_H_
#include "Ifx_Types.h"
#include <stdbool.h>

// 시스템의 모든 설정값을 모아둘 구조체
typedef struct {
    bool isAebEnabled; // AEB ON/OFF
    char partNumber[20]; // ECU 하드웨어 부품 번호
    char serialNumber[20]; // ECU 고유 시리얼 번호
    char vin[18]; // 차대번호
    char manufacturingDate[11]; // ECU 제조 날짜
    char supplier[20]; // ECU 공급업체

} SystemConfig;

// '설정값 변수가 있다'고 외부에 알리는 선언 (extern) 실제는 .c파일에 존재
extern SystemConfig g_config;

// 지원 DID 목록 배열이 외부에 존재한다고 선언
extern uint16 SUPPORTED_DIDS[];
// 지원 DID 개수도 외부에 존재한다고 선언
extern uint8 NUM_SUPPORTED_DIDS;


// 설정값을 초기화하는 함수 선언
void config_init(void);

// 설정값을 주기적으로 관리하는 함수 선언 (DFlash 저장 등)
void config_mainFunction(void);

#endif /* CONFIG_H_ */
