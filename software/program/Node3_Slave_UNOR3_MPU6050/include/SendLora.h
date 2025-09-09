#ifndef _SEND_LORA_H
#define _SEND_LORA_H

#include "main.h"
#include <LoRa.h>
#include "SensorData.h"

void InitLora(void);

void SenData(const SensorData &d);

void printSensorData_SEND(const SensorData &data);

#endif