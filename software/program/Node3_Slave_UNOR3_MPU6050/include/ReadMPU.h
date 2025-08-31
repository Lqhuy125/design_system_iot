#ifndef _READ_MPU_H
#define _READ_MPU_H

#include "main.h"
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include "SensorData.h"

/* non functinal -> need to change value in lib */
#ifdef MPU6050_DEVICE_ID
    #undef MPU6050_DEVICE_ID
    #define MPU6050_DEVICE_ID 0x70
#endif

extern Adafruit_MPU6050 mpu;
extern SensorData data;

void InitMPU(void);
void readMPU(void);

uint32_t calcCRC32(const void *data, size_t length);

void printSensorData_READ(const SensorData &d);

int serializeSensorData(const SensorData &d, uint8_t *buffer);

#endif