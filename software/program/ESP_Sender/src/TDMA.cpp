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
  radio_config_beacon();

  // Read encrypted beacon (16 bytes)
  uint8_t cipher[SECURE_BEACON_LEN];
  int state = radio.readData(cipher, SECURE_BEACON_LEN);

  radio_config_uplink();
  /* Serial.print("[RX] Cipher: ");
  for (int i = 0; i < SECURE_BEACON_LEN; i++) {
      Serial.print(cipher[i], HEX);
      Serial.print(" ");
  }
  Serial.println(); */

  if (state != RADIOLIB_ERR_NONE) {
      Serial.print("[RX] Read error: ");
      Serial.println(state);
      xSemaphoreGive(gLoraMutex);
      return false;
  }

  int len = radio.getPacketLength();
  if ((len != BEACON_TOTAL_LEN))
  {
    Serial.print("[RX] Invalid length: ");
    Serial.println(len);
    xSemaphoreGive(gLoraMutex);
    return false;
  }

  // Decrypt and verify MIC
  if (!secure_beacon_decrypt(cipher, &out)) {
      Serial.println("[RX] Decrypt/MIC failed");
      xSemaphoreGive(gLoraMutex);
      return false;
  }

  vTaskDelay(pdMS_TO_TICKS(2));
  xSemaphoreGive(gLoraMutex);

  return true;
}


uint8_t tdma_choose_slot(uint8_t my_id, const TDMABeacon& b) {
  // Broadcast mode: each node maps by ID
  if (b.node_id == 0) {
      if (b.total_slots == 0) return 0;
      uint8_t idx = (uint8_t)((my_id > 0 ? (my_id - 1) : 0) % b.total_slots);
      Serial.print("[TDMA] Broadcast mode, my slot: ");
      Serial.println(idx);
      return idx;
  }
  // Targeted: slot = node_id - 1 theo mô tả trong header
  // (chỉ node có ID phù hợp mới nên TX ở frame này)
  // Targeted mode: only specific node transmits
  if (b.node_id == my_id) {
      uint8_t idx = (uint8_t)(b.node_id - 1);
      Serial.print("[TDMA] Targeted mode, my slot: ");
      Serial.println(idx);
      return idx;
  }

  // Not my turn
  Serial.println("[TDMA] Not my slot this frame");
  return 0xFF;  // Invalid slot marker
}







