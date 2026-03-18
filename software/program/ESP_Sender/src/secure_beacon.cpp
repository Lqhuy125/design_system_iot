#include "secure_beacon.h"
#include "TDMA.h"
#include "Custom_Lora.h"
#include <mbedtls/aes.h>
#include <string.h>
#include <Arduino.h>

// ------------------------------------------------------------
// Key separation:
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

static const uint8_t CMAC_Rb[16] =
{
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x87
};

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

static void leftshift128(const uint8_t in[16], uint8_t out[16]) {
    uint8_t carry = 0;
    for (int i = 15; i >= 0; --i) {
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

    uint8_t zero[16] = {0};
    aes_ecb_encrypt_block(key, zero, L);

    leftshift128(L, K1);
    if (L[0] & 0x80)
        for (int i = 0; i < 16; i++) K1[i] ^= CMAC_Rb[i];

    leftshift128(K1, K2);
    if (K1[0] & 0x80)
        for (int i = 0; i < 16; i++) K2[i] ^= CMAC_Rb[i];

    size_t n = (len + 15) / 16;
    int last_complete = (len != 0 && (len % 16 == 0));
    if (n == 0) { n = 1; last_complete = 0; }

    uint8_t X[16] = {0}, Y[16], blk[16];

    for (size_t i = 0; i < n - 1; i++) {
        memcpy(blk, msg + i * 16, 16);
        for (int j = 0; j < 16; j++) X[j] ^= blk[j];
        aes_ecb_encrypt_block(key, X, Y);
        memcpy(X, Y, 16);
    }

    size_t off = (n - 1) * 16;
    size_t rem = (len > off ? len - off : 0);
    memset(blk, 0, 16);
    memcpy(blk, msg + off, rem);

    if (last_complete) {
        for (int j = 0; j < 16; j++) blk[j] ^= K1[j];
    } else {
        blk[rem] = 0x80;
        for (int j = 0; j < 16; j++) blk[j] ^= K2[j];
    }

    for (int j = 0; j < 16; j++) X[j] ^= blk[j];
    aes_ecb_encrypt_block(key, X, Y);
    memcpy(out, Y, 16);
}

// ------------------------------------------------------------
// Decrypt and verify MIC
// ------------------------------------------------------------
bool secure_beacon_verify_mic(const uint8_t cipher[16], uint8_t decrypted_out[16])
{
    // 1) Decrypt AES-ECB block 16B
    aes_ecb_decrypt_block(BEACON_AES_KEY, cipher, decrypted_out);

#if DEBUG_APP == 1
    
    for (int i = 0; i < 16; i++) {
        
        
    }
    
#endif
    // 2) Beacon data is 12 bytes, MIC is last 4 bytes
    const size_t DATA_LEN = 12;  // sync(1) + frame_id(2) + timestamp(4) + slot_len(2) + node_id(1) + total_slots(1) + reserved(1)
    uint8_t received_mic[MIC_LEN];
    memcpy(received_mic, decrypted_out + DATA_LEN, MIC_LEN);

#if DEBUG_APP == 1
    
    for (int i = 0; i < MIC_LEN; i++) {
        
        
    }
    
#endif
    // 3) Recompute CMAC on data portion (first 12 bytes)
    uint8_t full_cmac[16];
    aes_cmac_128(MIC_KEY, decrypted_out, DATA_LEN, full_cmac);

#if DEBUG_APP == 1
    
    for (int i = 0; i < 4; i++) {
        
        
    }
    
#endif
    // 4) Compare first 4 bytes of computed CMAC with received MIC
    bool mic_valid = (memcmp(full_cmac, received_mic, MIC_LEN) == 0);

    if (mic_valid) {
        
    } else {
        
    }

    return mic_valid;
}

// ------------------------------------------------------------
// Full decrypt + parse into TDMABeacon
// ------------------------------------------------------------
bool secure_beacon_decrypt(const uint8_t cipher[16], TDMABeacon* beacon_out)
{
    uint8_t decrypted[16];

    // 1) Decrypt and verify MIC
    if (!secure_beacon_verify_mic(cipher, decrypted)) {
        return false;
    }

    // 2) Parse decrypted data into beacon structure
    // Layout: sync(1) + frame_id(2) + beacon_timestamp(4) + slot_len_ms(2) + node_id(1) + total_slots(1) + reserved(1) = 12 bytes
    int idx = 0;
    memcpy(&beacon_out->sync,             &decrypted[idx], sizeof(beacon_out->sync));             idx += sizeof(beacon_out->sync);
    memcpy(&beacon_out->frame_id,         &decrypted[idx], sizeof(beacon_out->frame_id));         idx += sizeof(beacon_out->frame_id);
    memcpy(&beacon_out->beacon_timestamp, &decrypted[idx], sizeof(beacon_out->beacon_timestamp)); idx += sizeof(beacon_out->beacon_timestamp);
    memcpy(&beacon_out->slot_len_ms,      &decrypted[idx], sizeof(beacon_out->slot_len_ms));      idx += sizeof(beacon_out->slot_len_ms);
    memcpy(&beacon_out->node_id,          &decrypted[idx], sizeof(beacon_out->node_id));          idx += sizeof(beacon_out->node_id);
    memcpy(&beacon_out->total_slots,      &decrypted[idx], sizeof(beacon_out->total_slots));      idx += sizeof(beacon_out->total_slots);

    // 3) Recalculate CRC for internal use
    const size_t len_wo_crc = sizeof(TDMABeacon) - sizeof(uint32_t);
    beacon_out->crc = calcCRC32(beacon_out, len_wo_crc);

    // 4) Validate sync byte
    if (beacon_out->sync != 0xAA) {
        
        return false;
    }

#if DEBUG_APP == 1
    
    
    
    
    
    
    
    
#endif
    return true;
}
