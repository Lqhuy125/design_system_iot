#include "secure_data.h"
#include <string.h>
#include <mbedtls/aes.h>
#include <Arduino.h>

extern int serializeIMUSample(const IMUSample& s, uint8_t* out);
extern int deserializeIMUSample(IMUSample& s, const uint8_t *buffer);
// AES-128 key for data encryption (should be different from beacon key)
/* 3132333435363738393A3B3C3D3E3F40 */
static const uint8_t DATA_AES_KEY[16] = {
    0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38,
    0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F, 0x40
};

// Key for CMAC MIC calculation
/* 4142434445464748494A4B4C4D4E4F50 */
static const uint8_t DATA_MIC_KEY[16] = {
    0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48,
    0x49, 0x4A, 0x4B, 0x4C, 0x4D, 0x4E, 0x4F, 0x50
};

// IV for CBC mode (can be fixed or derived from frame_id for replay protection)
/* 000102030405060708090A0B0C0D0E0F */
static const uint8_t DATA_IV[16] = {
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
    0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F
};

static const uint8_t CMAC_Rb[16] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x87
};

// ============== AES Helper Functions ==============

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

static void aes_cbc_encrypt(const uint8_t key[16],
                            const uint8_t iv[16],
                            const uint8_t* input,
                            uint8_t* output,
                            size_t len)
{
    mbedtls_aes_context ctx;
    uint8_t iv_copy[16];
    memcpy(iv_copy, iv, 16);

    mbedtls_aes_init(&ctx);
    mbedtls_aes_setkey_enc(&ctx, key, 128);
    mbedtls_aes_crypt_cbc(&ctx, MBEDTLS_AES_ENCRYPT, len, iv_copy, input, output);
    mbedtls_aes_free(&ctx);
}

static void aes_cbc_decrypt(const uint8_t key[16],
                            const uint8_t iv[16],
                            const uint8_t* input,
                            uint8_t* output,
                            size_t len)
{
    mbedtls_aes_context ctx;
    uint8_t iv_copy[16];
    memcpy(iv_copy, iv, 16);

    mbedtls_aes_init(&ctx);
    mbedtls_aes_setkey_dec(&ctx, key, 128);
    mbedtls_aes_crypt_cbc(&ctx, MBEDTLS_AES_DECRYPT, len, iv_copy, input, output);
    mbedtls_aes_free(&ctx);
}

static void leftshift128(const uint8_t in[16], uint8_t out[16])
{
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

// ============== Serialize/Deserialize IMUSample ==============

/* static size_t serializeIMUSample(const IMUSample& s, uint8_t* buffer)
{
    size_t idx = 0;

    memcpy(&buffer[idx], &s.id, sizeof(s.id));   idx += sizeof(s.id);
    memcpy(&buffer[idx], &s.ax, sizeof(s.ax));   idx += sizeof(s.ax);
    memcpy(&buffer[idx], &s.ay, sizeof(s.ay));   idx += sizeof(s.ay);
    memcpy(&buffer[idx], &s.az, sizeof(s.az));   idx += sizeof(s.az);
    memcpy(&buffer[idx], &s.gx, sizeof(s.gx));   idx += sizeof(s.gx);
    memcpy(&buffer[idx], &s.gy, sizeof(s.gy));   idx += sizeof(s.gy);
    memcpy(&buffer[idx], &s.gz, sizeof(s.gz));   idx += sizeof(s.gz);
    memcpy(&buffer[idx], &s.dt, sizeof(s.dt));   idx += sizeof(s.dt);
    memcpy(&buffer[idx], &s.t_s, sizeof(s.t_s)); idx += sizeof(s.t_s);

    return idx;
} */

/* static void deserializeIMUSample(IMUSample& s, const uint8_t* buffer)
{
    size_t idx = 0;

    memcpy(&s.id, &buffer[idx], sizeof(s.id));   idx += sizeof(s.id);
    memcpy(&s.ax, &buffer[idx], sizeof(s.ax));   idx += sizeof(s.ax);
    memcpy(&s.ay, &buffer[idx], sizeof(s.ay));   idx += sizeof(s.ay);
    memcpy(&s.az, &buffer[idx], sizeof(s.az));   idx += sizeof(s.az);
    memcpy(&s.gx, &buffer[idx], sizeof(s.gx));   idx += sizeof(s.gx);
    memcpy(&s.gy, &buffer[idx], sizeof(s.gy));   idx += sizeof(s.gy);
    memcpy(&s.gz, &buffer[idx], sizeof(s.gz));   idx += sizeof(s.gz);
    memcpy(&s.dt, &buffer[idx], sizeof(s.dt));   idx += sizeof(s.dt);
    memcpy(&s.t_s, &buffer[idx], sizeof(s.t_s)); idx += sizeof(s.t_s);
} */

// ============== Public API ==============

bool secure_data_encrypt(const IMUSample& sample, uint8_t cipher_out[SECURE_DATA_TOTAL_LEN])
{
    // 1) Serialize IMUSample to buffer
    uint8_t plaintext[SECURE_DATA_TOTAL_LEN];
    memset(plaintext, 0, sizeof(plaintext));

    uint8_t data_len = serializeIMUSample(sample, plaintext);

    /* Serial.print("[ENCRYPT] PLAIN DATA: ");
    for (int i = 0; i < data_len; i++) {
        Serial.print(plaintext[i], HEX);
        Serial.print(" ");
    }
    Serial.println(); */
    // 2) Compute CMAC on serialized data
    uint8_t full_cmac[16];
    aes_cmac_128(DATA_MIC_KEY, plaintext, data_len, full_cmac);

    // 3) Append 4-byte MIC after data (at offset 32)
    memcpy(&plaintext[SECURE_DATA_PAYLOAD_LEN], full_cmac, SECURE_DATA_MIC_LEN);

    // 4) Pad remaining bytes with PKCS7 style (fill with padding length)
    // Data is 33 bytes, we pad to 32, then add 4 MIC = 36 bytes
    // We need to encrypt 48 bytes (3 blocks), so pad 36->48 with zeros or 0x0C
    for (size_t i = SECURE_DATA_PAYLOAD_LEN + SECURE_DATA_MIC_LEN; i < SECURE_DATA_TOTAL_LEN; i++) {
        plaintext[i] = 0;
    }

    /* Serial.print("[ENCRYPT] PLAIN DATA AFTER CMAC AND PADDING: ");
    for (int i = 0; i < SECURE_DATA_TOTAL_LEN; i++) {
        Serial.print(plaintext[i], HEX);
        Serial.print(" ");
    } */

    // 5) Encrypt with AES-128-CBC
    aes_cbc_encrypt(DATA_AES_KEY, DATA_IV, plaintext, cipher_out, SECURE_DATA_TOTAL_LEN);

    /* Serial.print("[ENCRYPT] CIPHER DATA: ");
    for (int i = 0; i < SECURE_DATA_TOTAL_LEN; i++) {
        Serial.print(cipher_out[i], HEX);
        Serial.print(" ");
    }
    Serial.println(); */
    
    /* Serial.print("[ENCRYPT] Data len: ");
    Serial.println(data_len);
    Serial.print("[ENCRYPT] MIC: ");
    for (int i = 0; i < SECURE_DATA_MIC_LEN; i++) {
        Serial.print(full_cmac[i], HEX);
        Serial.print(" ");
    }
    Serial.println(); */

    return true;
}

/* bool secure_data_decrypt(const uint8_t cipher[SECURE_DATA_TOTAL_LEN], IMUSample& sample_out)
{
    // 1) Decrypt with AES-128-CBC
    uint8_t plaintext[SECURE_DATA_TOTAL_LEN];
    aes_cbc_decrypt(DATA_AES_KEY, DATA_IV, cipher, plaintext, SECURE_DATA_TOTAL_LEN);

    Serial.print("[DECRYPT] Raw: ");
    for (int i = 0; i < 16; i++) {
        Serial.print(plaintext[i], HEX);
        Serial.print(" ");
    }
    Serial.println();

    // 2) Extract MIC (4 bytes at offset 32)
    uint8_t received_mic[SECURE_DATA_MIC_LEN];
    memcpy(received_mic, &plaintext[SECURE_DATA_PAYLOAD_LEN], SECURE_DATA_MIC_LEN);

    // 3) Recompute CMAC on data portion
    // We need original data length - IMUSample serialized is 33 bytes
    const size_t DATA_LEN = 33;  // id(1) + ax,ay,az,gx,gy,gz,dt,t_s (8*4=32) = 33 bytes
    uint8_t full_cmac[16];
    aes_cmac_128(DATA_MIC_KEY, plaintext, DATA_LEN, full_cmac);

    Serial.print("[DECRYPT] Received MIC: ");
    for (int i = 0; i < SECURE_DATA_MIC_LEN; i++) {
        Serial.print(received_mic[i], HEX);
        Serial.print(" ");
    }
    Serial.println();

    Serial.print("[DECRYPT] Computed MIC: ");
    for (int i = 0; i < SECURE_DATA_MIC_LEN; i++) {
        Serial.print(full_cmac[i], HEX);
        Serial.print(" ");
    }
    Serial.println();

    // 4) Verify MIC
    if (memcmp(full_cmac, received_mic, SECURE_DATA_MIC_LEN) != 0) {
        Serial.println("[DECRYPT] MIC FAILED!");
        return false;
    }

    Serial.println("[DECRYPT] MIC OK");

    // 5) Deserialize IMUSample
    deserializeIMUSample(sample_out, plaintext);

    return true;
} */

