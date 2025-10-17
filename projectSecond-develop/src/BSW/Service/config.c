// 파일명: config.c

#include "config.h"
// 나중에 DFlash 관련 헤더를 여기에 추가합니다.
//#include "IfxFlash.h"

// .h 파일에서 extern으로 선언했던 변수의 실체

SystemConfig g_config;

// 설정값을 초기화하는 함수 (부팅 시 1회 호출)
void config_init(void)
{
    // 나중에는 여기서 DFlash의 값을 읽어와 g_config를 채웁니다.

    // 지금은 우선 기본값으로 초기화합니다.
    g_config.isAebEnabled = true; // 기본값은 ON
    strcpy(g_config.partNumber, "RC-CAR-CRTL-V1.0");
    strcpy(g_config.serialNumber, "20250428-001");
    strcpy(g_config.vin, "KR01CAR00PS000001");
    strcpy(g_config.manufacturingDate, "2025-10-22");
    strcpy(g_config.supplier, "Hyundai Autoever");
}

// ECU가 SID 0x22로 지원하는 모든 DID 목록
uint16 SUPPORTED_DIDS[] = {
        0x1000, // 레이저 센서 거리
        0x2000, // 초음파(좌) 센서 거리
        0x2001, // 초음파(우) 센서 거리
        0x2002, // 초음파(후방) 센서 거리
        0xF187, // ECU 부품 번호
        0xF18C, // ECU 시리얼 번호
        0xF190, // 차대번호(VIN)
        0xF192, // ECU 공급업체 정보
        0xF193, // ECU 제조 날짜
        0xF1A0, // 지원 DID 목록
        0xF186 //  Diagnostic Session Identifier(세션 정보 요청)
};

uint8 NUM_SUPPORTED_DIDS = sizeof(SUPPORTED_DIDS) / sizeof(uint16);


// 주기적으로 호출될 관리 함수
void config_mainFunction(void)
{
    // 나중에는 여기에 'g_config에 변경이 생겼으면 DFlash에 저장'하는
    // 로직을 추가하게 됩니다.
}
