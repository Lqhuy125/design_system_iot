#ifndef _RECIEVE_LORA_H
#define _RECIEVE_LORA_H

#include "main.h"
#include <SPI.h>
#include <LoRa.h>
#include "SensorData.h"

#define MAX_NODES 5

// ESP32S SS = 5 rst=14
#define ss 5
#define rst 14
#define dio0 2
// ESP32 devkitv1 SS=15 rst=4
// #define ss 15
// #define rst 4
// #define dio0 2

extern void publishNodeData(const SensorData &d);

void InitLora(void);

void RecieveData(void);

uint32_t calcCRC32(const void *data, size_t length);

void printSensorData_RECIEVE(const SensorData &d);

void deserializeSensorData(SensorData &d, const uint8_t *buffer);

#endif