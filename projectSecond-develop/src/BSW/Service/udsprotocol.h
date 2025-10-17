// uds_protocol.h

#ifndef UDS_PROTOCOL_H
#define UDS_PROTOCOL_H

#include <stdint.h>

// UDS Service Identifiers (SID)
typedef enum {
    // Diagnostic and Communications Management
    SESSION_CONTROL                 = 0x10,
    ECU_RESET                       = 0x11,
    SECURITY_ACCESS                 = 0x27,
    COMMUNICATION_CONTROL           = 0x28,
    TESTER_PRESENT                  = 0x3E,
    CONTROL_DTC_SETTING             = 0x85,

    // Data Transmission
    READ_DATA_BY_IDENTIFIER         = 0x22,
    WRITE_DATA_BY_IDENTIFIER        = 0x2E,

    // Stored Data Transmission
    READ_DTC_INFORMATION            = 0x19,
    CLEAR_DIAGNOSTIC_INFORMATION    = 0x14,

    // Input/Output Control
    INPUT_OUTPUT_CONTROL_BY_IDENTIFIER = 0x2F,

    // Routine Control
    ROUTINE_CONTROL                 = 0x31,

    // Upload/Download
    REQUEST_DOWNLOAD                = 0x34,
    REQUEST_UPLOAD                  = 0x35,
    TRANSFER_DATA                   = 0x36,
    REQUEST_TRANSFER_EXIT           = 0x37,

    // Positive Response SID offset
    POSITIVE_RESPONSE_SID_OFFSET    = 0x40,

    // Negative Response SID
    NEGATIVE_RESPONSE               = 0x7F
} UdsServiceId;


// UDS Negative Response Codes (NRC)
typedef enum {
    GENERAL_REJECT                                  = 0x10,
    SERVICE_NOT_SUPPORTED                           = 0x11,
    SUB_FUNCTION_NOT_SUPPORTED                      = 0x12,
    INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT      = 0x13,
    RESPONSE_TOO_LONG                               = 0x14,
    CONDITIONS_NOT_CORRECT                          = 0x22,
    REQUEST_SEQUENCE_ERROR                          = 0x24,
    REQUEST_OUT_OF_RANGE                            = 0x31,
    SECURITY_ACCESS_DENIED                          = 0x33,
    INVALID_KEY                                     = 0x35,
    UPLOAD_DOWNLOAD_NOT_ACCEPTED                    = 0x70,
    GENERAL_PROGRAMMING_FAILURE                     = 0x72,
    SUB_FUNCTION_NOT_SUPPORTED_IN_ACTIVE_SESSION    = 0x7E,
    SERVICE_NOT_SUPPORTED_IN_ACTIVE_SESSION         = 0x7F,
} UdsNegativeResponseCode;

// Data Identifiers (DID) for ReadDataByIdentifier (SID 0x22)
typedef enum {
    DID_LASER_SENSOR_DISTANCE       = 0x1000,
    DID_ULTRASONIC_LEFT_DISTANCE    = 0x2000,
    DID_ULTRASONIC_RIGHT_DISTANCE   = 0x2001,
    DID_ULTRASONIC_REAR_DISTANCE    = 0x2002,

    // Standard DIDs (F1xx range)
    DID_ACTIVE_DIAGNOSTIC_SESSION   = 0xF186,
    DID_VEHICLE_MANUFACTURER_ECU_PART_NUMBER = 0xF187,
    DID_ECU_SERIAL_NUMBER           = 0xF18C,
    DID_VEHICLE_IDENTIFICATION_NUMBER = 0xF190,
    DID_ECU_SUPPLIER_INFORMATION    = 0xF192,
    DID_ECU_MANUFACTURING_DATE      = 0xF193,

    // DID for listing supported DIDs
    DID_SUPPORTED_DIDS_LIST         = 0xF1A0
} DataIdentifier;


#endif // UDS_PROTOCOL_H
