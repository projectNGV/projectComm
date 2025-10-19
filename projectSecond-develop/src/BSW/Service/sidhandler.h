#ifndef BSW_SERVICE_DIAGNOSTICS_HANDLER_SIDHANDLER_H_
#define BSW_SERVICE_DIAGNOSTICS_HANDLER_SIDHANDLER_H_

#include <stdint.h>

/*********************************************************************************************************************/
/*------------------------------------------------------Macros-------------------------------------------------------*/
/*********************************************************************************************************************/
#define UDS_POSITIVE_RESPONSE_SID(sid) ((sid) + 0x40)
#define UDS_NEGATIVE_RESPONSE_SID 0x7F

/*********************************************************************************************************************/
/*--------------------------------------------------Data Structures--------------------------------------------------*/
/*********************************************************************************************************************/
typedef enum
{
    UDS_SERVICE_DIAGNOSTIC_SESSION_CONTROL = 0x10,
    UDS_SERVICE_READ_DTC_INFORMATION = 0x19,
    UDS_SERVICE_READ_DATA_BY_IDENTIFIER = 0x22,
    UDS_SERVICE_READ_DATA_BY_PERIODIC_IDENTIFIER = 0x2A,
    UDS_SERVICE_WRITE_DATA_BY_IDENTIFIER = 0x2E,
    UDS_SERVICE_ROUTINE_CONTROL = 0x31,
    UDS_SERVICE_TESTER_PRESENT = 0x3E,
} UDS_ServiceType;

typedef enum
{
    UDS_NRC_GENERAL_REJECT = 0x10,
    UDS_NRC_SERVICE_NOT_SUPPORTED = 0x11,
    UDS_NRC_SUB_FUNCTION_NOT_SUPPORTED = 0x12,
    UDS_NRC_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT = 0x13,
    UDS_NRC_CONDITIONS_NOT_CORRECT = 0x22,
    UDS_NRC_REQUEST_OUT_OF_RANGE = 0x31,
} UDS_NegativeResponseCode;

/*********************************************************************************************************************/
/*-----------------------------------------------Function Prototypes-------------------------------------------------*/
/*********************************************************************************************************************/
void handleSID10(const uint8_t* data, uint16_t length, const uint8_t sid); // Diagnostic Session Control
void handleSID19(const uint8_t* data, uint16_t length, const uint8_t sid); // Read DTC Information
void handleSID22(const uint8_t* data, uint16_t length, const uint8_t sid); // Read Data By Identifier
void handleSID2A(const uint8_t* data, uint16_t length, const uint8_t sid); // Read Data By Periodic Identifier
void handleSID2E(const uint8_t* data, uint16_t length, const uint8_t sid); // Write Data By Identifier
void handleSID31(const uint8_t* data, uint16_t length, const uint8_t sid); // Routine Control
void handleSID3E(const uint8_t* data, uint16_t length, const uint8_t sid); // Tester Present

#endif /* BSW_SERVICE_DIAGNOSTICS_HANDLER_SIDHANDLER_H_ */
