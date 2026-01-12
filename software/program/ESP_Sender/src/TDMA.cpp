#include "TDMA.h"

extern SemaphoreHandle_t gLoraMutex;
extern SX1278 radio;

static  uint16_t tdma_u16_le(const uint8_t* p) {
  return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}
static  uint32_t tdma_u32_le(const uint8_t* p) {
  return (uint32_t)p[0]
       | ((uint32_t)p[1] << 8)
       | ((uint32_t)p[2] << 16)
       | ((uint32_t)p[3] << 24);
}

static  void tdma_beacon_fill_crc(TDMABeacon& b) {
  const size_t len_wo_crc = sizeof(TDMABeacon) - sizeof(uint32_t);
  b.crc = calcCRC32(&b, len_wo_crc);
}

static  bool tdma_beacon_verify(const TDMABeacon& b) {
  if (b.sync != 0xAA)
  {
    Serial.println("sync id error "); Serial.print(b.sync);
    return false;
  }
  const size_t len_wo_crc = sizeof(TDMABeacon) - sizeof(uint32_t);
  return (calcCRC32(&b, len_wo_crc) == b.crc);
}

static  bool tdma_beacon_deserialize(TDMABeacon &out, const uint8_t* buf, size_t len) {
  uint32_t recv_crc;
  memcpy(&recv_crc, &buf[BEACON_PAYLOAD_LEN], sizeof(recv_crc));
  uint32_t calc_crc = calcCRC32(buf, BEACON_PAYLOAD_LEN);
  if (calc_crc != recv_crc) {
    Serial.println(F("❌ CRC ERROR (Beacon)"));
    for (int i=0; i<sizeof(TDMABeacon); i++) {
        Serial.print(buf[i], HEX); Serial.print(" ");
    }
    Serial.println(" ");
    return false;
  }

  int idx = 0;
  memcpy(&out.sync,             &buf[idx], sizeof(out.sync));             idx += sizeof(out.sync);
  memcpy(&out.frame_id,         &buf[idx], sizeof(out.frame_id));         idx += sizeof(out.frame_id);
  memcpy(&out.beacon_timestamp, &buf[idx], sizeof(out.beacon_timestamp)); idx += sizeof(out.beacon_timestamp);
  memcpy(&out.slot_len_ms,      &buf[idx], sizeof(out.slot_len_ms));      idx += sizeof(out.slot_len_ms);
  memcpy(&out.node_id,          &buf[idx], sizeof(out.node_id));          idx += sizeof(out.node_id);
  memcpy(&out.total_slots,      &buf[idx], sizeof(out.total_slots));      idx += sizeof(out.total_slots);
  out.crc = recv_crc;   // lưu lại CRC ứng dụng nếu struct có trường crc

  for (int i=0; i<sizeof(TDMABeacon); i++) {
      Serial.print(buf[i], HEX); Serial.print(" ");
  }
  Serial.println(" ");
  return true;

}
// Nhận beacon với timeout; return true nếu parse & CRC OK
bool lora_receive_beacon(TDMABeacon& out) {
  xSemaphoreTake(gLoraMutex, portMAX_DELAY);
  uint8_t buf[BEACON_TOTAL_LEN];
  int state = radio.readData(buf, BEACON_TOTAL_LEN);

  if (state != RADIOLIB_ERR_NONE) return false;

  int len = radio.getPacketLength();
  if ((len != BEACON_TOTAL_LEN) || (buf[0] != 0xAA)) return false;

  if (!tdma_beacon_deserialize(out, buf, BEACON_TOTAL_LEN)) 
  {
    Serial.println("Deserialize error");
    return false;
  }
  
  vTaskDelay(pdMS_TO_TICKS(2));
  xSemaphoreGive(gLoraMutex);
  return true;
}


uint8_t tdma_choose_slot(uint8_t my_id, const TDMABeacon& b) {
  // Broadcast: mỗi node tự ánh xạ theo ID (tránh 0)
  if (b.node_id == 0) {
    if (b.total_slots == 0) return 0;
    uint8_t idx = (uint8_t)((my_id > 0 ? (my_id - 1) : 0) % b.total_slots);
    return idx;
  }
  // Targeted: slot = node_id - 1 theo mô tả trong header
  // (chỉ node có ID phù hợp mới nên TX ở frame này)
  if (b.node_id > 0) {
    return (uint8_t)(b.node_id - 1);
  }
  return 0;
}







