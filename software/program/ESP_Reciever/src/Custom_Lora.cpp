#include "Custom_Lora.h"

/*  Declare variable */
SX1278 radio = new Module(ss, dio0, rst);

IMUSample nodeData[MAX_NODES];

extern SemaphoreHandle_t gLoraMutex;

extern QueueHandle_t gRxQueue;
/* General Function */
static inline uint32_t calcCRC32(const void *data, size_t length) {
    const uint8_t *bytes = (const uint8_t*)data;
    uint32_t crc = 0xFFFFFFFF;   // initial value

    for (size_t i = 0; i < length; i++) {
        crc ^= bytes[i];
        for (uint8_t j = 0; j < 8; j++) {
            if (crc & 1)
                crc = (crc >> 1) ^ 0xEDB88320;  // polynomial
            else
                crc >>= 1;
        }
    }

    return ~crc;  // final XOR
}

void InitLora(void)
{
    // initialize SX1278 with default settings
    Serial.print(F("[SX1278] Initializing ... "));
    int state = radio.begin();
    if (state == RADIOLIB_ERR_NONE) {
      Serial.println(F("success!"));
    } else {
      Serial.print(F("failed, code "));
      Serial.println(state);
      while (true) { delay(10); }
    }
}

/* Send Data */
void lora_send_imusample(const IMUSample& s) {
  uint8_t buffer[128];  // đủ lớn cho IMUSample + CRC

  // 1) serialize IMUSample
  int payload_len = serializeIMUSample(s, buffer);

  // 2) calc CRC trên phần bytes của IMUSample
  uint32_t crc = calcCRC32(buffer, payload_len);

  // 3) append CRC32 (little-endian theo memcpy)
  memcpy(&buffer[payload_len], &crc, sizeof(crc));
  int total_len = payload_len + sizeof(crc);

  /* for (int i=0; i<sizeof(IMUSample); i++) {
      Serial.print(buffer[i], HEX); Serial.print(" ");
  } */
  // 4) Gửi qua LoRa
  xSemaphoreTake(gLoraMutex, portMAX_DELAY);
  Serial.println(F("[SX1278] Transmitting packet ... "));
  int state;
  state = radio.transmit((byte*)buffer, total_len);
  // byte byteArr[] = {0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF};
  //   int state = radio.transmit(byteArr, 8);

  if (state == RADIOLIB_ERR_NONE) {
    // the packet was successfully transmitted
    Serial.println(F(" success!"));

    // print measured data rate
    /* Serial.print(F("[SX1278] Datarate:\t"));
    Serial.print(radio.getDataRate());
    Serial.println(F(" bps")); */

  } else if (state == RADIOLIB_ERR_PACKET_TOO_LONG) {
    // the supplied packet was longer than 256 bytes
    Serial.println(F("too long!"));

  } else if (state == RADIOLIB_ERR_TX_TIMEOUT) {
    // timeout occurred while transmitting packet
    Serial.println(F("timeout!"));

  } else {
    // some other error occurred
    Serial.print(F("failed, code "));
    Serial.println(state);

  }
  xSemaphoreGive(gLoraMutex);
}

static int serializeIMUSample(const IMUSample& s, uint8_t* out) {
  int idx = 0;

  memcpy(&out[idx], &s.id, sizeof(s.id));      idx += sizeof(s.id);
  memcpy(&out[idx], &s.ax, sizeof(s.ax));      idx += sizeof(s.ax);
  memcpy(&out[idx], &s.ay, sizeof(s.ay));      idx += sizeof(s.ay);
  memcpy(&out[idx], &s.az, sizeof(s.az));      idx += sizeof(s.az);
  memcpy(&out[idx], &s.gx, sizeof(s.gx));      idx += sizeof(s.gx);
  memcpy(&out[idx], &s.gy, sizeof(s.gy));      idx += sizeof(s.gy);
  memcpy(&out[idx], &s.gz, sizeof(s.gz));      idx += sizeof(s.gz);
  memcpy(&out[idx], &s.dt, sizeof(s.dt));      idx += sizeof(s.dt);
  memcpy(&out[idx], &s.t_s, sizeof(s.t_s));    idx += sizeof(s.t_s);
  
  return idx; // độ dài phần dữ liệu IMUSample (chưa có CRC)
}


/* ========================Start Recieve Data======================== */

bool lora_receive_once(IMUSample &out) {
  uint8_t buf[IMU_TOTAL_LEN];
  // RadioLib blocking receive theo độ dài kỳ vọng:
  // Nếu firmware của bạn dùng API receive(buffer, len) – dùng dòng sau:
  int state = radio.receive(buf, IMU_TOTAL_LEN);

  if (state == RADIOLIB_ERR_NONE) {
    // Tách CRC nhận
    uint32_t recv_crc;
    memcpy(&recv_crc, &buf[IMU_PAYLOAD_LEN], sizeof(recv_crc));
    uint32_t calc_crc = calcCRC32(buf, IMU_PAYLOAD_LEN);
    if (calc_crc != recv_crc) {
      Serial.println(F("❌ CRC ERROR (IMU)"));
      for (int i=0; i<sizeof(IMUSample); i++) {
          Serial.print(buf[i], HEX); Serial.print(" ");
      }
      Serial.println(" ");
      return false;
    }

    // Giải IMUSample đúng thứ tự serializeIMUSample()
    int idx = 0;
    memcpy(&out.id, &buf[idx], sizeof(out.id)); idx += sizeof(out.id);
    memcpy(&out.ax, &buf[idx], sizeof(out.ax)); idx += sizeof(out.ax);
    memcpy(&out.ay, &buf[idx], sizeof(out.ay)); idx += sizeof(out.ay);
    memcpy(&out.az, &buf[idx], sizeof(out.az)); idx += sizeof(out.az);
    memcpy(&out.gx, &buf[idx], sizeof(out.gx)); idx += sizeof(out.gx);
    memcpy(&out.gy, &buf[idx], sizeof(out.gy)); idx += sizeof(out.gy);
    memcpy(&out.gz, &buf[idx], sizeof(out.gz)); idx += sizeof(out.gz);
    memcpy(&out.dt, &buf[idx], sizeof(out.dt)); idx += sizeof(out.dt);
    memcpy(&out.t_s, &buf[idx], sizeof(out.t_s)); idx += sizeof(out.t_s);
    out.crc = recv_crc;   // lưu lại CRC ứng dụng nếu struct có trường crc

    // (tuỳ chọn) in RSSI/SNR
    Serial.printf("[SX1278] RX OK size=%d RSSI=%d SNR=%.1f\n",
                  IMU_TOTAL_LEN, radio.getRSSI(), radio.getSNR());

    return true;
  } else if (state == RADIOLIB_ERR_RX_TIMEOUT) {
    // Không có gói trong window – normal
    return false;
  } else if (state == RADIOLIB_ERR_CRC_MISMATCH) {
    Serial.println(F("CRC error (PHY)"));
    return false;
  } else {
    Serial.print(F("RX failed, code ")); Serial.println(state);
    return false;
  }
}


void lora_rx_task(void* pv) {
  (void)pv;
  for (;;) {
    IMUSample s;
    if (lora_receive_once(s)) {
      if (xQueueSend(gRxQueue, &s, 0) != pdTRUE) {
        Serial.println("⚠️ gRxQueue full, drop sample");
      }
    }
    vTaskDelay(pdMS_TO_TICKS(5));
  }
}

/* ========================End Recieve Data======================== */
