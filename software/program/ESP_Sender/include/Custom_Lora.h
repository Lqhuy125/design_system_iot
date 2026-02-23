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



// --- PHY beacon (SX1278 @ 433 MHz) ---
#define F_BCN         433.5      // MHz (KHÔNG dùng Hz nguyên lớn)
#define SF_BCN        10             // ví dụ SF9 cho beacon
#define BW_BCN        250        // kHz
#define SW_BCN        0x14          // "private" sync word
#define PREAMBLE_BCN  15            // beacon preamble dài hơn uplink

// --- PHY uplink (SX1278 @ 433 MHz) ---
#define F_UL          434      // MHz (có thể chọn kênh khác trong dải 433)
#define SF_UL         9             // ví dụ SF7 cho uplink (ToA ngắn hơn)
#define BW_UL         125        // kHz
#define SW_UL         0x12         // "public/LoRaWAN" sync word

void InitLora(void);

uint32_t calcCRC32(const void *data, size_t length);

/* Send Lora */
void lora_send_imusample(const IMUSample& s);
static int serializeIMUSample(const IMUSample& s, uint8_t* out);

/* Recieve Lora */
void lora_recieve_imusample(IMUSample &s);
extern void publishNodeData(const IMUSample &d);
static int deserializeIMUSample(IMUSample& s, const uint8_t *buffer);


void radio_config_beacon();
void radio_config_uplink();

#endif