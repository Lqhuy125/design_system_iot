#ifndef SECURE_BEACON_H
#define SECURE_BEACON_H

#include <stdint.h>
#include <stddef.h>

struct TDMABeacon;

#define MIC_LEN 4

bool secure_beacon_verify_mic(const uint8_t cipher[16], uint8_t decrypted_out[16]);
bool secure_beacon_decrypt_and_verify(uint8_t input_test[16], const TDMABeacon* beacon_out, const uint8_t cipher[16]);

#endif