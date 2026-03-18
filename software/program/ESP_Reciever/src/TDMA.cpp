#include "TDMA.h"

extern TDMA_BeaconConfig cfg;



static inline void tdma_beacon_fill_crc(TDMABeacon& b) {
  const size_t len_wo_crc = sizeof(TDMABeacon) - sizeof(uint32_t);
  b.crc = calcCRC32(&b, len_wo_crc);
}

static inline bool tdma_beacon_verify(const TDMABeacon& b) {
  if (b.sync != 0xAA) return false;
  const size_t len_wo_crc = sizeof(TDMABeacon) - sizeof(uint32_t);
  return calcCRC32(&b, len_wo_crc) == b.crc;
}

/* API */
// Tạo beacon từ tham số; sync mặc định 0xAA; timestamp dùng millis() hiện tại nếu bạn không truyền.
static inline TDMABeacon tdma_make_beacon(
  uint16_t frame_id,
  uint16_t slot_len_ms,
  uint8_t  total_slots,
  uint8_t  node_id,                // 0 = broadcast toàn mạng; >0 = beacon riêng cho node
  uint32_t beacon_timestamp = 0    // nếu =0 sẽ lấy millis() tại thời điểm gọi
) 
{
    TDMABeacon b{};
    b.sync = 0xAA;
    b.frame_id = frame_id;
    b.beacon_timestamp = millis();
    b.slot_len_ms = slot_len_ms;
    b.node_id = node_id;
    b.total_slots = total_slots;
    tdma_beacon_fill_crc(b);
    return b;
}

// Gửi beacon đã tạo sẵn (serialize byte-by-byte)
static inline uint8_t tdma_send_beacon(const TDMABeacon& b) {
  // Có thể verify trước khi gửi
  if (!tdma_beacon_verify(b)) {
    
    return false;
  }

  xSemaphoreTake(gLoraMutex, portMAX_DELAY);
  // Serialize struct thành mảng byte và transmit
  // const uint8_t* raw = reinterpret_cast<const uint8_t*>(&b);
  /* This is the sample data to testing */
  uint8_t raw[16] =
  {
    0xAB, 0x00, 0x01, 0x00, 0xFC, 0x0F, 0x00, 0x00, 
    0x72, 0x01, 0x00, 0x04, 0xFD, 0x46, 0xC3, 0x9F
  };
  // Encrypt 16 bytes ADC-ECB
  uint8_t cipher[16];
  (void)secure_beacon_encrypt(raw, &b, cipher);
  radio_config_beacon();
  int state = radio.transmit((byte*)cipher, sizeof(TDMABeacon));
  radio_config_uplink();
  uint8_t buff[15];
  xSemaphoreGive(gLoraMutex);

  return state;
}

// Gửi beacon broadcast (node_id=0) — tất cả slave nhận thông số chung
uint8_t tdma_send_beacon_broadcast(
  uint16_t frame_id,
  uint16_t slot_len_ms,
  uint8_t  total_slots,
  uint32_t beacon_timestamp = 0
) 
{
  TDMABeacon b = tdma_make_beacon(frame_id, slot_len_ms, total_slots, /*node_id=*/0, beacon_timestamp);
  return tdma_send_beacon(b);
}

void BeaconConfiguration()
{  
  cfg.slot_len_ms   = 435; /* Calculate time run of one node */
  cfg.total_slots   = SLAVE_NODE_ID+1; /* Recommend to config the number of nodes + 1 */
  cfg.broadcast_mode= true;   // phát 1 beacon chung
  cfg.max_node_id   = 0;      // không dùng khi broadcast
  cfg.start_frame_id= 1;
}