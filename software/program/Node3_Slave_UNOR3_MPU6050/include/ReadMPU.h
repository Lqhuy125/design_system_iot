#ifndef _READ_MPU_H
#define _READ_MPU_H

#include "main.h"
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include "SensorData.h"

extern Adafruit_MPU6050 mpu;
extern SensorData data;

void InitMPU(void);
void readMPU(void);

uint32_t calcCRC32(const void *data, size_t length);
void printSensorData_READ(const SensorData &d);

#endif