#include "Custom_Lora.h"

/*  Declare variable */
SX1278 radio = new Module(ss, dio0, rst);

IMUSample nodeData[MAX_NODES];

extern SemaphoreHandle_t gLoraMutex;
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
void lora_recieve_imusample(IMUSample &s)
{
    uint8_t buffer[128];
    int state;
    // state = radio.receive(buffer);
    if (state == RADIOLIB_ERR_NONE) {
    // packet was successfully received
    Serial.println(F("success!"));

    // print the data of the packet
    Serial.print(F("[SX1278] Data:\t\t\t"));

    IMUSample s_recieved;
    deserializeIMUSample(s_recieved, buffer);

    // check CRC
    uint32_t calc_crc = calcCRC32(&buffer, sizeof(IMUSample) - sizeof(s_recieved.crc));
    if (calc_crc == s_recieved.crc) {
        Serial.println("✅ CRC OK");
        /* printSensorData_RECIEVE(received); */
        publishNodeData(s_recieved);

        // lưu dữ liệu theo node ID
        if (s_recieved.id > 0 && s_recieved.id <= MAX_NODES) {
            nodeData[s_recieved.id - 1] = s_recieved;
            Serial.print("===>>>Saved data for Node "); Serial.println(s_recieved.id);
        } else {
            Serial.print("⚠️ Unknown Node ID: "); Serial.println(s_recieved.id);
        }

    } else {
        Serial.println("❌ CRC ERROR");
    }

    // debug raw bytes
    /* for (int i=0; i<sizeof(IMUSample); i++) {
        Serial.print(buffer[i], HEX); Serial.print(" ");
    } 
    Serial.println(); */

    // print the RSSI (Received Signal Strength Indicator)
    // of the last received packet
    Serial.print(F("[SX1278] RSSI:\t\t\t"));
    Serial.print(radio.getRSSI());
    Serial.println(F(" dBm"));

    // print the SNR (Signal-to-Noise Ratio)
    // of the last received packet
    Serial.print(F("[SX1278] SNR:\t\t\t"));
    Serial.print(radio.getSNR());
    Serial.println(F(" dB"));

    // print frequency error
    // of the last received packet
    Serial.print(F("[SX1278] Frequency error:\t"));
    Serial.print(radio.getFrequencyError());
    Serial.println(F(" Hz"));

  } else if (state == RADIOLIB_ERR_RX_TIMEOUT) {
    // timeout occurred while waiting for a packet
    Serial.println(F("timeout!"));

  } else if (state == RADIOLIB_ERR_CRC_MISMATCH) {
    // packet was received, but is malformed
    Serial.println(F("CRC error!"));

  } else {
    // some other error occurred
    Serial.print(F("failed, code "));
    Serial.println(state);

  }
}

static int deserializeIMUSample(IMUSample& s, const uint8_t *buffer) {
  int idx = 0;

  memcpy(&s.id, &buffer[idx], sizeof(s.id));        idx += sizeof(s.id);
  memcpy(&s.ax, &buffer[idx], sizeof(s.ax));    idx += sizeof(s.ax);
  memcpy(&s.ay, &buffer[idx], sizeof(s.ay));    idx += sizeof(s.ay);
  memcpy(&s.az, &buffer[idx], sizeof(s.az));    idx += sizeof(s.az);
  memcpy(&s.gx, &buffer[idx], sizeof(s.gx));  idx += sizeof(s.gx);
  memcpy(&s.gy, &buffer[idx], sizeof(s.gy));  idx += sizeof(s.gy);
  memcpy(&s.gz, &buffer[idx], sizeof(s.gz));  idx += sizeof(s.gz);
  memcpy(&s.dt, &buffer[idx], sizeof(s.dt));      idx += sizeof(s.dt);
  memcpy(&s.t_s, &buffer[idx], sizeof(s.t_s));    idx += sizeof(s.t_s);
  memcpy(&s.crc, &buffer[idx], sizeof(s.crc));      idx += sizeof(s.crc);

  for (int i=0; i<sizeof(IMUSample); i++) {
      Serial.print(buffer[i], HEX); Serial.print(" ");
  }
  return idx;
}

/* ========================End Recieve Data======================== */
