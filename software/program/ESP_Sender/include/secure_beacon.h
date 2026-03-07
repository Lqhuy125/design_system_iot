#ifndef SECURE_BEACON_H
#define SECURE_BEACON_H

#include <stdint.h>
#include <stddef.h>

struct TDMABeacon;

#define MIC_LEN         4
#define SECURE_BEACON_LEN  16   // AES block size (encrypted beacon)

/**
 * @brief Decrypt beacon and verify MIC
 * @param cipher       Input: 16-byte encrypted beacon from LoRa
 * @param decrypted_out Output: 16-byte decrypted data
 * @return true if MIC verification passed
 */
bool secure_beacon_verify_mic(const uint8_t cipher[16], uint8_t decrypted_out[16]);

/**
 * @brief Decrypt beacon, verify MIC, and parse into TDMABeacon structure
 * @param cipher      Input: 16-byte encrypted beacon
 * @param beacon_out  Output: Parsed beacon structure
 * @return true if decryption and MIC verification succeeded
 */
bool secure_beacon_decrypt(const uint8_t cipher[16], TDMABeacon* beacon_out);
#endif