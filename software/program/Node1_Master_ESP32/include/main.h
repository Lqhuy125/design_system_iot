#ifndef _MAIN_H
#define _MAIN_H

#include <Arduino.h>
#include <Wire.h>

#include "ReciveLora.h"
#include "MQTT.h"
#include "Sensor.h"
#include "MahonyAHRS.h"

/* Library for freeRTOS */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

void telemetry_task(void* pv);
void ahrs_task(void* pv);

void read_without_rtos();
#endif