#include "secure_beacon.h"
#include "TDMA.h"
#include <mbedtls/aes.h>
#include <string.h>

// ------------------------------------------------------------
// 1) Key separation:
//    - ENC_KEY: AES-ECB encryption
//    - MIC_KEY: CMAC(MIC) calculation
// ------------------------------------------------------------
/* 101112131415161718191A1B1C1D1E1F */
/* This key to encrypt data */
static uint8_t BEACON_AES_KEY[16] =
{
    0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
    0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F
};
/* This key to mac cipher data */
static const uint8_t MIC_KEY[16] =
{
    0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28,
    0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F, 0x30
};

// ------------------------------------------------------------
// 2) AES-ECB one-block encrypt (mbedTLS)
// ------------------------------------------------------------
static void aes_ecb_decrypt_block(const uint8_t key[16],
                                  const uint8_t in[16],
                                  uint8_t out[16])
{
    mbedtls_aes_context ctx;
    mbedtls_aes_init(&ctx);

    mbedtls_aes_setkey_dec(&ctx, key, 128);
    mbedtls_aes_crypt_ecb(&ctx, MBEDTLS_AES_DECRYPT, in, out);

    mbedtls_aes_free(&ctx);
}

// ------------------------------------------------------------
// 3) CMAC (RFC 4493) - Tự cài đầy đủ
// ------------------------------------------------------------
static const uint8_t CMAC_Rb[16] =
{
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x87
};

static void leftshift128(const uint8_t in[16], uint8_t out[16]) {
    uint8_t carry = 0;
    for (int i=15; i>=0; --i) {
        uint8_t c = (in[i] & 0x80) ? 1 : 0;
        out[i] = (uint8_t)((in[i] << 1) | carry);
        carry = c;
    }
}

static void aes_cmac_128(const uint8_t key[16],
                         const uint8_t* msg, size_t len,
                         uint8_t out[16])
{
    uint8_t L[16], K1[16], K2[16];

    // 1) L = AES(key, 0^128)
    uint8_t zero[16] = {0};
    aes_ecb_encrypt_block(key, zero, L);

    // 2) Tạo K1, K2
    leftshift128(L, K1);
    if (L[0] & 0x80)
        for(int i=0;i<16;i++) K1[i] ^= CMAC_Rb[i];

    leftshift128(K1, K2);
    if (K1[0] & 0x80)
        for(int i=0;i<16;i++) K2[i] ^= CMAC_Rb[i];

    // 3) Chia block
    size_t n = (len + 15) / 16;
    int last_complete = (len != 0 && (len % 16 == 0));
    if (n == 0) { n=1; last_complete=0; }

    uint8_t X[16] = {0}, Y[16], blk[16];

    // 3a) CBC-MAC 0..n-2
    for (size_t i=0; i<n-1; i++) {
        memcpy(blk, msg + i*16, 16);
        for (int j=0;j<16;j++) X[j] ^= blk[j];
        aes_ecb_encrypt_block(key, X, Y);
        memcpy(X, Y, 16);
    }

    // 3b) Last block
    size_t off = (n-1)*16;
    size_t rem = (len > off ? len - off : 0);
    memset(blk, 0, 16);
    memcpy(blk, msg + off, rem);

    if (last_complete) {
        for (int j=0;j<16;j++) blk[j] ^= K1[j];
    } else {
        blk[rem] = 0x80;
        for (int j=0;j<16;j++) blk[j] ^= K2[j];
    }

    for (int j=0;j<16;j++) X[j] ^= blk[j];
    aes_ecb_encrypt_block(key, X, Y);
    memcpy(out, Y, 16);
}

// ------------------------------------------------------------
// 4) secure_beacon_decrypt() - SLAVE side
/*
1. Decrypt the 16-byte cipher using AES-ECB with BEACON_AES_KEY
2. Extract the 4-byte MIC from bytes 12-15 of decrypted data
3. Recompute CMAC on the first 12 bytes using MIC_KEY
4. Compare the first 4 bytes of computed CMAC with received MIC
5. Return true only if MIC matches (integrity verified) */
// ------------------------------------------------------------

bool secure_beacon_verify_mic(const uint8_t cipher[16], uint8_t decrypted_out[16])
{
    // 1) Decrypt AES-ECB block 16B
    aes_ecb_decrypt_block(BEACON_AES_KEY, cipher, decrypted_out);

    Serial.print("[CMAC] Decrypted: ");
    for (int i = 0; i < 16; i++) {
        Serial.print(decrypted_out[i], HEX);
        Serial.print(" ");
    }
    Serial.println();

    // 2) Extract received MIC (last 4 bytes)
    const size_t LEN_WO_CRC = sizeof(TDMABeacon) - sizeof(uint32_t);  // 12 bytes
    uint8_t received_mic[MIC_LEN];
    memcpy(received_mic, decrypted_out + LEN_WO_CRC, MIC_LEN);

    Serial.print("[CMAC] Received MIC: ");
    for (int i = 0; i < MIC_LEN; i++) {
        Serial.print(received_mic[i], HEX);
        Serial.print(" ");
    }
    Serial.println();

    // 3) Recompute CMAC on data portion (first 12 bytes)
    uint8_t full_cmac[16];
    aes_cmac_128(MIC_KEY, decrypted_out, LEN_WO_CRC, full_cmac);

    Serial.print("[CMAC] Computed CMAC: ");
    for (int i = 0; i < 16; i++) {
        Serial.print(full_cmac[i], HEX);
        Serial.print(" ");
    }
    Serial.println();

    // 4) Compare first 4 bytes of computed CMAC with received MIC
    bool mic_valid = (memcmp(full_cmac, received_mic, MIC_LEN) == 0);

    if (mic_valid) {
        Serial.println("[CMAC] MIC verification PASSED");
    } else {
        Serial.println("[CMAC] MIC verification FAILED");
    }

    return mic_valid;
}

bool secure_beacon_decrypt_and_verify(uint8_t input_test[16], const TDMABeacon* beacon_out, const uint8_t cipher[16])
{
    uint8_t decrypted[16];

    // 1) Verify MIC and get decrypted data
    if (!secure_beacon_verify_mic(cipher, decrypted)) {
        return false;
    }

    // 2) Copy decrypted data to beacon structure (without MIC field)
    memcpy(beacon_out, decrypted, sizeof(TDMABeacon) - sizeof(uint32_t));

    // 3) Recalculate CRC for the beacon structure
    const size_t len_wo_crc = sizeof(TDMABeacon) - sizeof(uint32_t);
    beacon_out->crc = calcCRC32(beacon_out, len_wo_crc);

    return true;
}
