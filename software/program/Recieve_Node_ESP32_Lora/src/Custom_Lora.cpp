#include "Custom_Lora.h"

/*  Declare variable */
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
    Serial.println("LoRa Receiver");
    LoRa.setPins(ss, rst, dio0);
    
    if (!LoRa.begin(433E6)) {
        Serial.println("Starting LoRa failed!");
        Serial.println("Check connect before run the program!");
        while (1);
    }
    else
    {
        Serial.println("Connect successfully");
    }
    LoRa.setSyncWord(0xA5);
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


  // 4) Gửi qua LoRa
  xSemaphoreTake(gLoraMutex, portMAX_DELAY);
  LoRa.beginPacket();
  LoRa.write(buffer, total_len);
  LoRa.endPacket();
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
void lora_send_imusample(IMUSample &s)
{
    // try to parse packet
    int packetSize = LoRa.parsePacket();
    if (packetSize == sizeof(IMUSample)) {
        uint8_t buffer[128];
        int payload_len = 0;
        // Đọc dữ liệu vào buffer
        while (payload_len < packetSize && LoRa.available()) {
            buffer[payload_len++] = (uint8_t)LoRa.read();
        }

        IMUSample s_recieved;
        deserializeIMUSample(s_recieved, buffer);

        // in ra kết quả
        /* printSensorData_RECIEVE(received); */

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
        for (int i=0; i<sizeof(SensorData); i++) {
            Serial.print(buffer[i], HEX); Serial.print(" ");
        }
        Serial.println();
    }
    else
    {
        // Serial.println("========4_Read fail");
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
  memcpy(&s.crc, &buffer[idx], sizeof(s.crc));      idx += sizeof(s.crc);

  return idx;
}

/* ========================End Recieve Data======================== */