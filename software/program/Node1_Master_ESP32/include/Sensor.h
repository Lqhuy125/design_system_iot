#ifndef _SENSOR_H
#define _SENSOR_H

#include "main.h"
#include <stdio.h>
#include "SensorData.h"
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <Wire.h>

void Init_MPU6050();

int sensor_read(IMUSample* out);

// FreeRTOS task entry (// FreeRTOS task entry (pv = QueueHandle_t*)
void sensor_task(void* pv);
#endif