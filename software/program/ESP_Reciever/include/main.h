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

#include "TDMA.h"
#include "secure_beacon.h"
/* Define the number of nodes in system */
#define MAX_NODES 5
#define SLAVE_NODE_ID 3

/* Define the function of RTOS task */
void lora_process_task(void* pv);
void tdma_scheduler_task(void* pv);
void mqtt_push_task(void* pv);

/* Define the function be used in program*/
void transmit_without_rtos();

extern void radio_config_beacon();
extern void radio_config_uplink();

#endif