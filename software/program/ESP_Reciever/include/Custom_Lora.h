#ifndef _CUSTOM_LORA_H
#define _CUSTOM_LORA_H

#include "main.h"
#include <SPI.h>
#include <RadioLib.h>
#include "SensorData.h"

// SX1278 has the following connections:
// NSS pin:   10-5
// DIO0 pin:  2
// RESET pin: 9-14
// DIO1 pin:  3-null
#define ss 5
#define rst 14
#define dio0 2


static const int IMU_PAYLOAD_LEN = 33;               // id + ax..gz + dt + t_s
static const int IMU_TOTAL_LEN   = IMU_PAYLOAD_LEN + sizeof(uint32_t); // + CRC32 = 40


void InitLora(void);

static inline uint32_t calcCRC32(const void *data, size_t length);

/* Send Lora */
void lora_send_imusample(const IMUSample& s);
static int serializeIMUSample(const IMUSample& s, uint8_t* out);

/* Recieve Lora */
bool lora_receive_once(IMUSample &out);
extern void publishNodeData(const IMUSample &d);
void lora_rx_task(void* pv);

#endif