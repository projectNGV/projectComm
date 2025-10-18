#include "aeb.h"

/* ==============================================================
 * AEB (Autonomous Emergency Braking)
 * - aebEnableFlag: 기능 ON/OFF 스위치 (SOA 명령 or 사용자 제어)
 * - aebActiveFlag: 현재 긴급제동이 실제로 작동 중인지 표시
 * ============================================================== */

/* ─────────────── 긴급 부저 경고 ─────────────── */
void emergencyBuzzer(void)
{
    buzzerOn(); delayMs(100);
    buzzerOff(); delayMs(150);
    buzzerOn(); delayMs(100);
    buzzerOff();
}

/* ─────────────── 긴급 제동 수행 ───────────────
 * - AEB가 활성화된 경우 현재 속도로 후진 후 정지
 */
void performEmergencyStop(void)
{
    if (!motorState.aebActiveFlag)
    {
        motorState.aebActiveFlag = true;
        sendAebStateIfChanged();
    }

    emergencyBuzzer();
    motorMoveReverse(motorState.currentDuty); // 현재 속도로 후진
    delayMs(REVERSE_TIME_MS);

    motorState.currentDuty = 0;
    motorStop();    // 정지

//    myPrintf("[AEB] Emergency stop performed.\n");
}

/* ─────────────── AEB 상태 업데이트 ───────────────
 * @param g_TofValue : 전방 거리(mm)
 * ---------------------------------------------------
 * 1. aebEnableFlag == false  → 기능 꺼짐, 제동 비활성화
 * 2. DUTY_LIMIT_DISTANCE_MM 이하 → 속도 제한
 * 3. AEB_DISTANCE_MM 이하 → 긴급제동 작동
 * 4. SAFETY_DISTANCE_MM 이상 → 긴급제동 해제
 */
void updateAebFlagByTof(unsigned int g_TofValue)
{
    // (1) 기능 자체가 꺼져 있으면 긴급제동 해제
    if (!aebEnableFlag) {
        motorState.aebActiveFlag = false;
        sendAebStateIfChanged();
        return;
    }

    // (2) AEB 기능 ON 상태 → 거리 기반 판단
    if (!motorState.aebActiveFlag) {
        // 근접 시 속도 제한
        if (g_TofValue <= DUTY_LIMIT_DISTANCE_MM)
            motorState.currentDuty = 500;

        // 위험 거리 → 긴급 제동 ON
        if (g_TofValue < AEB_DISTANCE_MM) {
            motorState.aebActiveFlag = true;
            sendAebStateIfChanged();
//            myPrintf("[AEB] Activated! Distance=%u mm\n", g_TofValue);
        }
    }
    // (3) AEB 이미 작동 중 → 안전 거리 확보 시 해제
    else {
        if (g_TofValue >= SAFETY_DISTANCE_MM) {
            motorState.aebActiveFlag = false;
            sendAebStateIfChanged();
//            myPrintf("[AEB] Released. Distance=%u mm\n", g_TofValue);
        }
    }
}
