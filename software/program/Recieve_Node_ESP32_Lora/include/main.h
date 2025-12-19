#ifndef _MAIN_H
#define _MAIN_H

#include <Arduino.h>
#include <Wire.h>


#include "MQTT.h"
#include "Sensor.h"
#include "MahonyAHRS.h"
#include "Custom_Lora.h"

/* Library for freeRTOS */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"


/* Define the number of nodes in system */
#define MAX_NODES 5




void telemetry_task(void* pv);

void transmit_task(void* pv);

void transmit_without_rtos();

#endif