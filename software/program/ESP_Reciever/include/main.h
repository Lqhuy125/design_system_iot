#ifndef _MAIN_H
#define _MAIN_H

#include <Arduino.h>
#include <Wire.h>

#include "MQTT.h"
#include "Sensor.h"
#include "Custom_Lora.h"

/* Library for freeRTOS */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

/* Define the number of nodes in system */
#define MAX_NODES 5

/* Define the function of RTOS task */
extern void lora_rx_task(void* pv);
extern void aggregator_task(void* pv);
extern void mqtt_push_task(void* pv);

/* Define the function be used in program*/
void transmit_without_rtos();

#endif