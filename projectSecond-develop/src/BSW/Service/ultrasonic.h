/*
 * Ultrasonic.h
 *
 *  Created on: 2025. 6. 26.
 *      Author: USER
 */

#ifndef BSW_IO_ULTRASONIC_H_
#define BSW_IO_ULTRASONIC_H_

#include "IfxPort.h"

#include "port.h"
#include "util.h"

#include "dtc.h"

typedef struct
{
    GpioPin trigger;
    GpioPin echo;
} UltPin;


typedef enum ultradir
{
    ULT_LEFT, ULT_RIGHT, ULT_REAR, ULT_SENSORS_NUM
} UltraDir;



void ultrasonicInit(void);
int getDistanceByUltra(UltraDir dir);

// 새로 추가할 함수 선언 (cm 단위 거리를 float으로 반환)
float ultrasonic_getDistanceCm(UltraDir dir);

// dtc 진단 함수 선언 추가
void diagnoseUltrasonicSensor(void);

#endif /* BSW_IO_ULTRASONIC_H_ */
