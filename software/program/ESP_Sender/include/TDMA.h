#ifndef _TDMA_H
#define _TDMA_H

#include "main.h"

/* Total bytes of the element in TDMABeacon struct  */
#define BEACON_TOTAL_LEN 15
#define BEACON_PAYLOAD_LEN 11  // except for crc

// === Thêm: guard/margin mặc định (có thể chỉnh theo môi trường của bạn) ===
#define TDMA_GUARD_MS   8   // bù trễ/jitter trước khi vào slot
#define TDMA_MARGIN_MS  4   // kết thúc sớm để tránh chồng lấn

#define ARRAY_SIZE(x) (sizeof(x)/sizeof(x[0]))

// Cấu hình cho scheduler beacon
struct TDMA_BeaconConfig {
  uint16_t slot_len_ms;
  uint8_t  total_slots;
  bool     broadcast_mode; // true: node_id=0; false: phát riêng từng node
  uint8_t  max_node_id;    // nếu broadcast_mode=false, sẽ phát lần lượt node_id=1..max_node_id
  uint16_t start_frame_id; // frame bắt đầu
};

struct __attribute__((packed)) TDMABeacon {
  uint8_t   sync;               // = 0xAA
  uint16_t  frame_id;           // ID frame hiện tại (do master sinh)
  uint32_t  beacon_timestamp;   // millis() tại master khi gửi
  uint16_t  slot_len_ms;        // độ dài 1 slot
  uint8_t   node_id;            // node này thuộc slot = node_id - 1; 0 = broadcast toàn mạng
  uint8_t   total_slots;        // tổng số slot trong frame
  uint32_t  crc;                // CRC32 trên [sync..total_slots]
};

extern uint32_t calcCRC32(const void *data, size_t length);

static  uint16_t tdma_u16_le(const uint8_t* p);
static  uint32_t tdma_u32_le(const uint8_t* p);

static  void tdma_beacon_fill_crc(TDMABeacon& b);

static  bool tdma_beacon_verify(const TDMABeacon& b);

bool lora_receive_beacon(TDMABeacon& out);

void TDMA_UpdateFromBeacon(const TDMABeacon& b);

static  bool tdma_beacon_deserialize(const uint8_t* buf, size_t len, TDMABeacon& out);

uint8_t tdma_choose_slot(uint8_t my_id, const TDMABeacon& b);

extern void radio_config_beacon();
extern void radio_config_uplink();
#endif