#ifndef SECURE_BEACON_H
#define SECURE_BEACON_H

#include <stdint.h>
#include <stddef.h>

struct TDMABeacon;

#define MIC_LEN 4

bool secure_beacon_encrypt(uint8_t input_test[16], const TDMABeacon* b, uint8_t out[16]);

#endif