/*********************************************************************************************************************/
/*-----------------------------------------------------Includes------------------------------------------------------*/
/*********************************************************************************************************************/

#include "autopark.h"

#include "asclin1.h"
#include "ultrasonic.h"
#include "buzzer.h"
#include "led.h"
#include "gpt12.h"

#include <stdlib.h>
#include <stdio.h>

/*********************************************************************************************************************/
/*------------------------------------------------------Macros-------------------------------------------------------*/
/*********************************************************************************************************************/

#define MOTOR_STOP_DELAY 500

/*********************************************************************************************************************/
/*-------------------------------------------------Global variables--------------------------------------------------*/
/*********************************************************************************************************************/

extern volatile boolean g_rx_getLine;
extern volatile char g_rx_buffer[RX_BUFFER_SIZE];
extern volatile uint16 g_beepInterval;

/*********************************************************************************************************************/
/*--------------------------------------------Private Variables/Constants--------------------------------------------*/
/*********************************************************************************************************************/

// Parking Distance
static int parkingDistance = 250000;

// Parking Speed
static int parkingSpeedForward = 350;
static int parkingSpeedBackward = 350;

// Parking Tick
static volatile int parkingFoundTick = 4;

// Forward & Rotate 90 Degree Rightward
static int goForwardDelay = 0;
static int rotateDelay = 375;

// Backward Stop Distance
static int stopDistance = 120000;

/*********************************************************************************************************************/
/*------------------------------------------------Function Prototypes------------------------------------------------*/
/*********************************************************************************************************************/

static void foundSpace ();
static void rotate ();
static void goBackWard ();

static void tuneParkingDistance ();
static void tuneParkingSpeed ();
static void tuneParkingFoundTick ();
static void tuneRotate ();
static void tuneStopDistance ();

void autoParkTune ();
void autoPark ();

/*********************************************************************************************************************/
/*---------------------------------------------Function Implementations----------------------------------------------*/
/*********************************************************************************************************************/

// 주차 공간 확보된 지점 탐색하는 로직
static void foundSpace ()
{
    int curTick = 0;

    // 1) 전진 시작
    motorMoveForward(parkingSpeedForward);

    // 2) 주차 공간 탐색 루프
    while (curTick < parkingFoundTick)
    {
        // 왼쪽 초음파 센서 거리 확인
        if (parkingDistance <= getDistanceByUltra(ULT_LEFT))
        {
            // 측정하면서 parkingdistance가 더 작다면 tick 카운트 계속 증가
            curTick++;
            if (curTick >= parkingFoundTick)
                break;
        }
        else
        {
            // 주차 가능 거리보다 좁을 시 카운트 리셋
            curTick = 0;
        }
        delayMs(50);
    }

    // 3) 조건 충족 시 차량 정지
    motorStop();
}

static void rotate ()
{
    motorMoveForward(parkingSpeedForward);
    delayMs(goForwardDelay);
    motorStop();
    delayMs(MOTOR_STOP_DELAY);
    motorMovChAPwm(0, 1);
    motorMovChBPwm(1000, 0);
    delayMs(rotateDelay);
    motorStop();
}

static void goBackWard ()
{
    // 1) 후진 시작
    motorMoveReverse(parkingSpeedBackward);

    // 2) 뒤쪽 거리 측정
    int rearDis = getDistanceByUltra(ULT_REAR);

    // 3) 거리 기반 경고음 간격 계산
    g_beepInterval = rearDis / 100;
    buzzerOn();
    Gpt1_Interrupt_Enable ();

    // 4) stopDistance에 도달할 때까지 반복
    while (rearDis > stopDistance)
    {
        delayMs(50);
        rearDis = getDistanceByUltra(ULT_REAR);
        // 가까울 수록 경고음 간격 짧아짐
        g_beepInterval = rearDis / 100;
    }

    // 5) 정지 및 경고 동작 수행
    motorStop();
    Gpt1_Interrupt_Disable();
    buzzerOn();
    ledStartBlinking(LED_BOTH);
    delayMs(2500);
    buzzerOff();

}

// 주차 공간 거리 설정 및 조정
static void tuneParkingDistance ()
{
    while (1)
    {
        // 현재 상태 안내 메시지 출력
        bluetoothPrintf("?주차 공간 입력 [c] - 왼쪽 초음파 거리, [y] - 확인 (현재거리: %d)\n", parkingDistance);

        // 1) 블루투스 수신 대기
        while (!g_rx_getLine);

        // 2) 입력 명령어 해석
        if (g_rx_buffer[0] == 'c')
        {
            // 'c'입력 시 왼쪽 초음파 센서 거리 측정
            int leftDis = getDistanceByUltra(ULT_LEFT);
            bluetoothPrintf("현재 초음파 거리: %d\n", leftDis);
        }
        else if (g_rx_buffer[0] == 'y')
        {
            // 'y' 입력 시 현재 parkingDistance 값 확정
            bluetoothPrintf("주차 공간 설정 완료: %d\n", parkingDistance);
            rxBufferFlush(); // 버퍼 정리
            break; // while (1) 루프 종료
        }
        else
        {
            // 그 외 입력 -> 숫자로 간주하여 parkingDistance 변경
            parkingDistance = atoi(g_rx_buffer);
            bluetoothPrintf("parkingDistance 변경 완료: %d\n", parkingDistance);
        }
        // 3) 매번 버퍼 정리
        rxBufferFlush();
    }
}

// 주차 시 전진/후진 속도 조절
static void tuneParkingSpeed ()
{
    while (1)
    {
        // 1) 사용자 안내 메시지 출력
        bluetoothPrintf("[주차 공간 찾기] 직진 후진 속도 조절\n");
        bluetoothPrintf("?[y] - 확인\t(현재 직진: %d, 현재 후진: %d)\n", parkingSpeedForward, parkingSpeedBackward);

        // 2) 사용자 입력 대기 (블루투스 수신 완료 플래그 기다림)
        while (!g_rx_getLine);

        // 3) 입력 명령 해석
        if (g_rx_buffer[0] == 'y')
        {
            // 'y' 입력 -> 현재 속도 확정하고 루프 종료
            bluetoothPrintf("속도 설정 완료\n");
            bluetoothPrintf("직진: %d\t후진: %d\n", parkingSpeedForward, parkingSpeedBackward);
            rxBufferFlush();
            break;
        }
        else
        {
            // 숫자 2개 입력 -> 직진, 후진 속도 파싱
            sscanf(g_rx_buffer, "%d %d", &parkingSpeedForward, &parkingSpeedBackward);
        }
        rxBufferFlush();

        /**
         * 직진 보여줘야 되는데 몇초나 할지
         */

        //4) 입력된 속도로 실제 모터 시연
        motorMoveForward(parkingSpeedForward); // 전진
        delayMs(2000);
        motorStop();
        delayMs(MOTOR_STOP_DELAY);
        motorMoveReverse(parkingSpeedBackward); // 후진
        delayMs(2000);
        motorStop();
        delayMs(MOTOR_STOP_DELAY);
    }
}

// 주차 공간 찾기 알고리즘에서 사용할 Tick 값 튜닝용
static void tuneParkingFoundTick ()
{

    while (1)
    {
        // 1) 현재 Tick 값 안내 메시지 출력
        bluetoothPrintf("?[주차 공간 찾기] Tick 값 설정 [y] - 확인 (현재 Tick: %d)\n", parkingFoundTick);

        // 2) 블루투스 입력 대기
        while (!g_rx_getLine);

        // 3) 입력 처리
        if (g_rx_buffer[0] == 'y')
        {
            bluetoothPrintf("Tick 값 설정 완료: %d\n", parkingFoundTick);
            rxBufferFlush();
            break;
        }
        else
        {
            parkingFoundTick = atoi(g_rx_buffer);
        }
        rxBufferFlush();

        // 4) 현재 설정값으로 실제 주차공간 탐색 로직 실행
        foundSpace();
    }
}

// 주차 동작 시 전진/후진 구간에서 사용할 딜레이값 조정 및 실제 회전 동작 테스트
static void tuneRotate ()
{
    while (1)
    {
        // 1) 안내 메시지 출력
        bluetoothPrintf("[주차] 직진 & 후진 딜레이 조절 (현재 직진 딜레이: %d, 후진 딜레이: %d)\n", goForwardDelay, rotateDelay);
        bluetoothPrintf("?[y] - 확인 [r] - 주차 공간 찾기\n");

        // 2) 사용자 입력 대기
        while (!g_rx_getLine);

        // 3) 입력 명령 처리
        if (g_rx_buffer[0] == 'y')
        {
            bluetoothPrintf("딜레이 설정 완료 직진: %d\t후진: %d\n", goForwardDelay, rotateDelay);
            rxBufferFlush();
            break;
        }
        else if (g_rx_buffer[0] == 'r')
        {
            foundSpace();
        }
        else
        {
            // 직진, 후진 딜레이 값 갱신
            sscanf(g_rx_buffer, "%d %d", &goForwardDelay, &rotateDelay);
        }
        rxBufferFlush();

        // 4) 현재 딜레이값으로 rotate 동작 실행
        rotate();
    }
}

// 주차 시 후진 정지 거리 튜닝
static void tuneStopDistance ()
{
    while (1)
    {
        // 1) 현재 값 출력과 안내
        bluetoothPrintf("[주차] 후진 거리 조절 (현재 후진 거리: %d)\n", stopDistance);
        bluetoothPrintf("?[c] - 뒤쪽 거리 출력\t[y] - 확인 \n");

        // 2) 블루투스 입력 대기
        while (!g_rx_getLine);

        // 3) 입력 처리
        if (g_rx_buffer[0] == 'y')
        {
            bluetoothPrintf("후진 거리 설정 완료: %d\n", stopDistance);
            rxBufferFlush();
            break;
        }
        else if (g_rx_buffer[0] == 'c')
        {
            int rearDis = getDistanceByUltra(ULT_REAR);
            bluetoothPrintf("현재 초음파 거리: %d\n", rearDis);
        }
        else
        {
            stopDistance = atoi(g_rx_buffer);
            goBackWard();
        }

        // 4) 버퍼 정리
        rxBufferFlush();
    }
}

void autoParkTune (void)
{
    boolean isTuned = FALSE;

    while (1)
    {
        bluetoothPrintf("\n");
        bluetoothPrintf("\n");
        bluetoothPrintf("===========현재 값===========\n");
        bluetoothPrintf("1. [주차 공간 찾기] 주차공간: %d\n", parkingDistance);
        bluetoothPrintf("2. [주차 공간 찾기] 전진속도: %d\t", parkingSpeedForward);
        bluetoothPrintf("후진속도: %d\n", parkingSpeedBackward);
        bluetoothPrintf("3. [주차 공간 찾기] Tick: %d\n", parkingFoundTick);
        bluetoothPrintf("4. [주차 90도 들어가기] 전진 딜레이: %d\t", goForwardDelay);
        bluetoothPrintf("4. 후진 우회전 딜레이: %d\n", rotateDelay);
        bluetoothPrintf("5. [주차] 후진 정지 거리: %d\n", stopDistance);
        bluetoothPrintf("?[r] - 시험 주행\t[c]- 확인\t[#]- 재설정\n");
        while (!g_rx_getLine)
            ;
        switch (g_rx_buffer[0])
        {
            case 'r' :
                rxBufferFlush();
                autoPark();
                break;
            case 'c' :
                rxBufferFlush();
                isTuned = true;
                break;
            case '1' :
                rxBufferFlush();
                tuneParkingDistance();
                break;
            case '2' :
                rxBufferFlush();
                tuneParkingSpeed();
                break;
            case '3' :
                rxBufferFlush();
                tuneParkingFoundTick();
                break;
            case '4' :
                rxBufferFlush();
                tuneRotate();
                break;
            case '5' :
                rxBufferFlush();
                tuneStopDistance();
                break;
            default :
                rxBufferFlush();
                bluetoothPrintf("UNKNOWN COMMAND\n");
                break;
        }



        if (isTuned)
            break;
    }
}

void autoPark (void)
{
    foundSpace();
    rotate();
    delayMs(MOTOR_STOP_DELAY);
    goBackWard();
}
