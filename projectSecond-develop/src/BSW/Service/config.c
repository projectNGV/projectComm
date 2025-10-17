// 파일명: config.c

#include "config.h"
// 나중에 DFlash 관련 헤더를 여기에 추가합니다.
//#include "IfxFlash.h"

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
        DID_LASER_SENSOR_DISTANCE, // 레이저 센서 길이 측정
        DID_ULTRASONIC_LEFT_DISTANCE, // 좌측 초음파 센서 측정
        DID_ULTRASONIC_RIGHT_DISTANCE, // 우측 초음파 센서 측정
        DID_ULTRASONIC_REAR_DISTANCE, // 후방 초음파 센서 측정
        DID_VEHICLE_MANUFACTURER_ECU_PART_NUMBER, // ECU 부품 번호
        DID_ECU_SERIAL_NUMBER, // ECU 시리얼 번호
        DID_VEHICLE_IDENTIFICATION_NUMBER, // 차대번호(VIN)
        DID_ECU_SUPPLIER_INFORMATION, // ECU 공급업체 정보
        DID_ECU_MANUFACTURING_DATE, // ECU 제조 날짜
        DID_SUPPORTED_DIDS_LIST,  // 지원 DID 목록
        DID_ACTIVE_DIAGNOSTIC_SESSION // Diagnostic Session Identifier(세션 정보 요청)
};

uint8 NUM_SUPPORTED_DIDS = sizeof(SUPPORTED_DIDS) / sizeof(uint16);


// 주기적으로 호출될 관리 함수
void config_mainFunction(void)
{
    // 나중에는 여기에 'g_config에 변경이 생겼으면 DFlash에 저장'하는
    // 로직을 추가하게 됩니다.
}
