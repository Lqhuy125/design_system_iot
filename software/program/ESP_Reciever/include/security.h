#ifndef _SECURITY_H
#define _SECURITY_H


#include "soc/soc.h"
#include "soc/dport_reg.h"
#include "soc/dport_access.h"
#include "soc/hwcrypto_reg.h"
#include <string.h>
#include "Arduino.h"

void aes_hw_encrypt_ecb(uint32_t key[16],
                        uint32_t input[16],
                        uint32_t output[16]);
static inline void aes_set_mode(int mode, uint32_t key_bytes);
static inline void aes_ll_write_block(uint32_t *input);
static inline uint32_t aes_ll_write_key(uint32_t *key, size_t key_word_len);
#endif
