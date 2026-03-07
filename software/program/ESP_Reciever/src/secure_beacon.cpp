#include "secure_beacon.h"
#include "TDMA.h"
#include <mbedtls/aes.h>
#include <string.h>

// ------------------------------------------------------------
// 1) Key separation (2 khóa khác nhau):
//    - ENC_KEY: AES-ECB encryption
//    - MIC_KEY: CMAC(MIC) calculation
// ------------------------------------------------------------
/* 101112131415161718191A1B1C1D1E1F */
/* This key to encrypt data */
static uint8_t BEACON_AES_KEY[16] = {
  0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
  0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F
};
/* This key to mac cipher data */
static const uint8_t MIC_KEY[16] = {
    0x21,0x22,0x23,0x24, 0x25,0x26,0x27,0x28,
    0x29,0x2A,0x2B,0x2C, 0x2D,0x2E,0x2F,0x30
};

// ------------------------------------------------------------
// 2) AES-ECB one-block encrypt (mbedTLS)
// ------------------------------------------------------------
static void aes_ecb_encrypt_block(const uint8_t key[16],
                                  const uint8_t in[16],
                                  uint8_t out[16])
{
    mbedtls_aes_context ctx;
    mbedtls_aes_init(&ctx);

    mbedtls_aes_setkey_enc(&ctx, key, 128);
    mbedtls_aes_crypt_ecb(&ctx, MBEDTLS_AES_ENCRYPT, in, out);

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
// 4) secure_beacon_encrypt() - MASTER side
//    ECB + CMAC(MIC 4 byte)
// ------------------------------------------------------------
bool secure_beacon_encrypt(uint8_t input_test[16], const TDMABeacon* b, uint8_t out[16])
{
    // 1) Copy plaintext beacon structure
    uint8_t raw[sizeof(TDMABeacon)];
    memcpy(raw, b, sizeof(TDMABeacon));
    /* This line open to test input */
    // memcpy(raw, input_test, 16);

    // 2) MIC = 4 byte đầu của CMAC(MIC_KEY, beacon_without_crc)
    const size_t LEN_WO_CRC = sizeof(TDMABeacon) - sizeof(uint32_t);

    uint8_t full_cmac[16];
    /* Cmac generate with plain data */
    aes_cmac_128(MIC_KEY, raw, LEN_WO_CRC, full_cmac);
#if DEBUG_APP == 1
    /* for (int i = 0; i < 16; i++) {
        Serial.print(full_cmac[i], HEX);
        Serial.print(" ");
    }
    Serial.println(); */
#endif
    /* overwrite 4 byte MIC to CRC field */
    memcpy(raw + LEN_WO_CRC, full_cmac, MIC_LEN);  // MIC_LEN=4

    // 3) Encrypt AES-ECB block 16B
    aes_ecb_encrypt_block(BEACON_AES_KEY, raw, out);
#if DEBUG_APP == 1
    /* for (int i = 0; i < 16; i++) {
        Serial.print(out[i], HEX);
        Serial.print(" ");
    }
    Serial.println(); */
#endif
    return true;
}
