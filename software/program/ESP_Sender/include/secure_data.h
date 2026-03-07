#ifndef SECURE_DATA_H
#define SECURE_DATA_H

#include <stdint.h>
#include <stddef.h>
#include "main.h"

// IMUSample serialized: id(1) + ax(4) + ay(4) + az(4) + gx(4) + gy(4) + gz(4) + dt(4) + t_s(4) = 33 bytes
// Padded to 32 bytes data + 4 bytes MIC = 36 bytes -> encrypt as 48 bytes (3 AES blocks)
#define SECURE_DATA_PAYLOAD_LEN  33   // Data portion (padded)
#define SECURE_DATA_MIC_LEN      4    // MIC length
#define SECURE_DATA_TOTAL_LEN    48   // 3 x 16-byte AES blocks

/**
 * @brief Encrypt IMUSample with AES-128-CBC and append CMAC-based MIC
 * @param sample Input IMUSample structure
 * @param cipher_out Output encrypted buffer (48 bytes)
 * @return true on success
 */
bool secure_data_encrypt(const IMUSample& sample, uint8_t cipher_out[SECURE_DATA_TOTAL_LEN]);

/**
 * @brief Decrypt and verify MIC of received sensor data
 * @param cipher Input encrypted buffer (48 bytes)
 * @param sample_out Output decrypted IMUSample
 * @return true if decryption and MIC verification succeed
 */
/* bool secure_data_decrypt(const uint8_t cipher[SECURE_DATA_TOTAL_LEN], IMUSample& sample_out); */

#endif // SECURE_DATA_H
