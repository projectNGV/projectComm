#include "systeminit.h"


void systemInit(void) {
    bluetoothInit();
    motorInit();
    asclin0InitUart();
    tofInit();
    uartInit();
    ultrasonicInit();
    ledInit();
    gpt12Init();
    buzzerInit();
    stmInit();
    soaPublisherInit();
    session_init();
    config_init();
}
