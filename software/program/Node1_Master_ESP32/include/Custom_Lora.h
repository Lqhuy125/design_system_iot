#ifndef _CUSTOM_LORA_H
#define _CUSTOM_LORA_H

#include "main.h"
#include <SPI.h>
#include <LoRa.h>
#include "SensorData.h"

// ESP32S SS = 5 rst=14
#define ss 5
#define rst 14
#define dio0 2

void InitLora(void);

static inline uint32_t calcCRC32(const void *data, size_t length);

/* Send Lora */
void lora_send_imusample(const IMUSample& s, const SensorData &data);
static int serializeIMUSample(const IMUSample& s, uint8_t* out);

/* Recieve Lora */
void lora_recieve_imusample(IMUSample &s);
extern void publishNodeData(const SensorData &d);
static int deserializeIMUSample(IMUSample& s, const uint8_t *buffer);

void SenData(const SensorData &data);
int serializeSensorData(const SensorData &d, uint8_t *buffer);
#endif