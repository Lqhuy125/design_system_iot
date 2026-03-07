#ifndef _TDMA_H
#define _TDMA_H

#include "Arduino.h"
#include "Custom_Lora.h"
#include "stdint.h"
#include <RadioLib.h>
#include "mbedtls/aes.h"

// Cấu hình cho scheduler beacon
struct TDMA_BeaconConfig {
  uint16_t slot_len_ms;
  uint8_t  total_slots;
  bool     broadcast_mode; // true: node_id=0; false: phát riêng từng node
  uint8_t  max_node_id;    // nếu broadcast_mode=false, sẽ phát lần lượt node_id=1..max_node_id
  uint16_t start_frame_id; // frame bắt đầu
};

struct __attribute__((packed)) TDMABeacon {
  uint16_t  sync;               // = 0xAA
  uint16_t  frame_id;           // ID frame hiện tại (do master sinh)
  uint32_t  beacon_timestamp;   // millis() tại master khi gửi
  uint16_t  slot_len_ms;        // độ dài 1 slot
  uint8_t   node_id;            // node này thuộc slot = node_id - 1; 0 = broadcast toàn mạng
  uint8_t   total_slots;        // tổng số slot trong frame
  uint32_t  crc;                // CRC32 trên [sync..total_slots]
};

extern inline uint32_t calcCRC32(const void *data, size_t length);
extern SemaphoreHandle_t gLoraMutex;
extern SX1278 radio;

static inline void tdma_beacon_fill_crc(TDMABeacon& b);
static inline bool tdma_beacon_verify(const TDMABeacon& b);

/* API */
// Tạo beacon từ tham số; sync mặc định 0xAA; timestamp dùng millis() hiện tại nếu bạn không truyền.
static inline TDMABeacon tdma_make_beacon(
  uint16_t frame_id,
  uint16_t slot_len_ms,
  uint8_t  total_slots,
  uint8_t  node_id,                // 0 = broadcast toàn mạng; >0 = beacon riêng cho node
  uint32_t beacon_timestamp    // nếu =0 sẽ lấy millis() tại thời điểm gọi
);

// Gửi beacon đã tạo sẵn (serialize byte-by-byte)
static inline uint8_t tdma_send_beacon(const TDMABeacon& b);

uint8_t tdma_send_beacon_broadcast(
  uint16_t frame_id,
  uint16_t slot_len_ms,
  uint8_t  total_slots,
  uint32_t beacon_timestamp
);

void BeaconConfiguration();

#endif