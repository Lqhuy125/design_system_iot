#include "security.h"

#define AES_TEXT_IN_0_REG   (AES_TEXT_BASE + 0U)
#define AES_TEXT_IN_1_REG   (AES_TEXT_BASE + 4U)
#define AES_TEXT_IN_2_REG   (AES_TEXT_BASE + 8U)
#define AES_TEXT_IN_3_REG   (AES_TEXT_BASE + 12U)

#define AES_TEXT_OUT_0_REG   AES_TEXT_IN_0_REG
#define AES_TEXT_OUT_1_REG   AES_TEXT_IN_1_REG
#define AES_TEXT_OUT_2_REG   AES_TEXT_IN_2_REG
#define AES_TEXT_OUT_3_REG   AES_TEXT_IN_3_REG

#define AES_KEY_0_REG       (AES_KEY_BASE + 0U)
#define AES_KEY_1_REG       (AES_KEY_BASE + 4U)
#define AES_KEY_2_REG       (AES_KEY_BASE + 8U)
#define AES_KEY_3_REG       (AES_KEY_BASE + 12U)
#define AES_KEY_4_REG       (AES_KEY_BASE + 16U)
#define AES_KEY_5_REG       (AES_KEY_BASE + 20U)
#define AES_KEY_6_REG       (AES_KEY_BASE + 24U)
#define AES_KEY_7_REG       (AES_KEY_BASE + 28U)

/* The following bits apply to DPORT_PERI_CLK_EN_REG, DPORT_PERI_RST_EN_REG
 */
#define DPORT_PERI_RST_AES (1<<0)

/* padlock.c and aesni.c rely on these values! */
#define ESP_AES_ENCRYPT     1
#define ESP_AES_DECRYPT     0

// AES-128 ECB hardware encryption
void aes_hw_encrypt_ecb(uint32_t key[16],
                        uint32_t input[16],
                        uint32_t output[16])
{
    // 1) Enable AES hardware clock
    DPORT_REG_SET_BIT(DPORT_PERI_CLK_EN_REG, DPORT_PERI_EN_AES);
    DPORT_REG_CLR_BIT(DPORT_PERI_RST_EN_REG, DPORT_PERI_RST_AES);

    // 2) Load key
    // DPORT_REG_WRITE(AES_KEY_0_REG, ((uint32_t*)key)[0]);
    // DPORT_REG_WRITE(AES_KEY_1_REG, ((uint32_t*)key)[1]);
    // DPORT_REG_WRITE(AES_KEY_2_REG, ((uint32_t*)key)[2]);
    // DPORT_REG_WRITE(AES_KEY_3_REG, ((uint32_t*)key)[3]);
    aes_ll_write_key((uint32_t *)key, 4);

    // 3) Load plaintext
    // DPORT_REG_WRITE(AES_TEXT_IN_0_REG, ((uint32_t*)input)[0]);
    // DPORT_REG_WRITE(AES_TEXT_IN_1_REG, ((uint32_t*)input)[1]);
    // DPORT_REG_WRITE(AES_TEXT_IN_2_REG, ((uint32_t*)input)[2]);
    // DPORT_REG_WRITE(AES_TEXT_IN_3_REG, ((uint32_t*)input)[3]);
    aes_ll_write_block((uint32_t *)input);

    // 4) Configure AES-128 + ECB mode
    aes_set_mode(ESP_AES_ENCRYPT, sizeof(key)/sizeof(key[0]));

    // 5) Start
    DPORT_REG_WRITE(AES_START_REG, 1);

    // 6) Wait done
    while (!(DPORT_REG_READ(AES_IDLE_REG) & 0x01)) {}

    // 7) Read ciphertext
    // ((uint32_t*)output)[0] = DPORT_REG_READ(AES_TEXT_OUT_0_REG);
    // ((uint32_t*)output)[1] = DPORT_REG_READ(AES_TEXT_OUT_1_REG);
    // ((uint32_t*)output)[2] = DPORT_REG_READ(AES_TEXT_OUT_2_REG);
    // ((uint32_t*)output)[3] = DPORT_REG_READ(AES_TEXT_OUT_3_REG);
    uint32_t *output_words = (uint32_t *)output;
    esp_dport_access_read_buffer(output_words, AES_TEXT_OUT_0_REG, 4);
}

/**
 * @brief Sets the mode
 *
 * @param mode ESP_AES_ENCRYPT = 1, or ESP_AES_DECRYPT = 0
 * @param key_bytes Number of bytes in the key
 */

static inline void aes_set_mode(int mode, uint32_t key_bytes)
{
    const uint32_t MODE_DECRYPT_BIT = 4;
    unsigned mode_reg_base = (mode == ESP_AES_ENCRYPT) ? 0 : MODE_DECRYPT_BIT;

    /* See TRM for the mapping between keylength and mode bit */
    DPORT_REG_WRITE(AES_MODE_REG, mode_reg_base + ((key_bytes / 8) - 2));
}

static inline void aes_ll_write_block(uint32_t *input)
{
    const uint32_t *input_words = (const uint32_t *)input;
    uint32_t i0;
    uint32_t i1;
    uint32_t i2;
    uint32_t i3;

    /* Storing i0,i1,i2,i3 in registers not an array
    helps a lot with optimisations at -Os level */
    i0 = input_words[0];
    DPORT_REG_WRITE(AES_TEXT_BASE, i0);

    i1 = input_words[1];
    DPORT_REG_WRITE(AES_TEXT_BASE + 4, i1);

    i2 = input_words[2];
    DPORT_REG_WRITE(AES_TEXT_BASE + 8, i2);

    i3 = input_words[3];
    DPORT_REG_WRITE(AES_TEXT_BASE + 12, i3);
}

static inline uint32_t aes_ll_write_key(uint32_t *key, size_t key_word_len)
{
    /* This variable is used for fault injection checks, so marked volatile to avoid optimisation */
    volatile uint32_t key_bytes_in_hardware = 0;

    /* Memcpy to avoid potential unaligned access */
    uint32_t key_word;

    for (int i = 0; i < key_word_len; i++) {
        memcpy(&key_word, key + 4 * i, 4);
        DPORT_REG_WRITE(AES_KEY_BASE + i * 4, key_word);
        key_bytes_in_hardware += 4;
    }
    return key_bytes_in_hardware;
}
