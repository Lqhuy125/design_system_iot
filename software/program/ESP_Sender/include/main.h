#ifndef _MAIN_H
#define _MAIN_H

#include <Arduino.h>
#include <Wire.h>

#include "MQTT.h"
#include "Sensor.h"
#include "Custom_Lora.h"
#include "secure_beacon.h"
#include "secure_data.h"

/* Library for freeRTOS */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "TDMA.h"
/* Define the number of nodes in system */
#define MAX_NODES 5
#define SLAVE_NODE
#define SLAVE_NODE_ID 3

/* Define the function of RTOS task */
void lora_process_task(void* pv);

/* Define the function be used in program*/
void transmit_without_rtos();

struct TDMABeacon;
extern bool lora_receive_beacon(TDMABeacon& out);
extern uint8_t tdma_choose_slot(uint8_t my_id, const TDMABeacon& b);


#endif