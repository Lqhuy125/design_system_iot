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
    // LoRa.setFrequency(433E6);

    // Đọc lại FRF để verify
    uint32_t frf =
        ((uint32_t)LoRa.readRegister(0x06) << 16) |
        ((uint32_t)LoRa.readRegister(0x07) <<  8) |
        ((uint32_t)LoRa.readRegister(0x08) <<  0);

    double freqHz = frf * 61.03515625; // 32MHz / 2^19
    Serial.printf("[VERIFY] FRF=0x%06lX -> %.0f Hz\n", (unsigned long)frf, freqHz);
}

/* Send Data */
void lora_send_imusample(const IMUSample& s, const SensorData &data) {
  uint8_t buffer[128];  // đủ lớn cho IMUSample + CRC

  // 1) serialize IMUSample
  int payload_len = serializeIMUSample(s, buffer);

  // 2) calc CRC trên phần bytes của IMUSample
  uint32_t crc = calcCRC32(buffer, payload_len);

  // 3) append CRC32 (little-endian theo memcpy)
  memcpy(&buffer[payload_len], &crc, sizeof(crc));
  int total_len = payload_len + sizeof(crc);

    uint8_t buffer1[64]; // đủ lớn
  int len = serializeSensorData(data, buffer1);
  // 4) Gửi qua LoRa
//   xSemaphoreTake(gLoraMutex, portMAX_DELAY);

  LoRa.beginPacket();

  LoRa.write(buffer1, len);
  LoRa.endPacket();
  
    uint8_t op = LoRa.readRegister(0x01);
    Serial.printf("[STATE] OpMode=0x%02X\n", op);

  
  lora_dump_config();
//   xSemaphoreGive(gLoraMutex);
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

void SenData(const SensorData &data)
{
  uint8_t buffer[64]; // đủ lớn
  int len = serializeSensorData(data, buffer);

  LoRa.beginPacket();
  LoRa.write(buffer, len);
  LoRa.endPacket();
}

int serializeSensorData(const SensorData &d, uint8_t *buffer) {
  int idx = 0;

  memcpy(&buffer[idx], &d.id, sizeof(d.id)); idx += sizeof(d.id);
  memcpy(&buffer[idx], &d.accX, sizeof(d.accX)); idx += sizeof(d.accX);
  memcpy(&buffer[idx], &d.accY, sizeof(d.accY)); idx += sizeof(d.accY);
  memcpy(&buffer[idx], &d.accZ, sizeof(d.accZ)); idx += sizeof(d.accZ);
  memcpy(&buffer[idx], &d.gyroX, sizeof(d.gyroX)); idx += sizeof(d.gyroX);
  memcpy(&buffer[idx], &d.gyroY, sizeof(d.gyroY)); idx += sizeof(d.gyroY);
  memcpy(&buffer[idx], &d.gyroZ, sizeof(d.gyroZ)); idx += sizeof(d.gyroZ);
  memcpy(&buffer[idx], &d.temperature, sizeof(d.temperature)); idx += sizeof(d.temperature);
  memcpy(&buffer[idx], &d.crc, sizeof(d.crc)); idx += sizeof(d.crc);

  for (int i=0; i<sizeof(SensorData); i++) {
      Serial.print(buffer[i], HEX); Serial.print(" ");
  }
  Serial.println();
  return idx; // tổng số byte
}


static long mapBW(uint8_t bwBits) {
  switch (bwBits) {
    case 0x00: return 7800;   // 7.8 kHz
    case 0x01: return 10400;  // 10.4 kHz
    case 0x02: return 15600;  // 15.6 kHz
    case 0x03: return 20800;  // 20.8 kHz
    case 0x04: return 31250;  // 31.25 kHz
    case 0x05: return 41700;  // 41.7 kHz
    case 0x06: return 62500;  // 62.5 kHz
    case 0x07: return 125000; // 125 kHz
    case 0x08: return 250000; // 250 kHz
    case 0x09: return 500000; // 500 kHz
    default:   return -1;
  }
}

void lora_dump_config() {
  // Đọc tần số: RegFrfMsb/Mid/Lsb (0x06..0x08)
  uint32_t frf =
    ((uint32_t)LoRa.readRegister(0x06) << 16) |
    ((uint32_t)LoRa.readRegister(0x07) <<  8) |
    ((uint32_t)LoRa.readRegister(0x08) <<  0);
  // f = frf * (32e6 / 2^19) ≈ frf * 61.03515625 Hz
  double freqHz = frf * 61.03515625;

  uint8_t opmode   = LoRa.readRegister(0x01); // check LORA mode & mode RX/TX
  uint8_t modem1   = LoRa.readRegister(0x1D);
  uint8_t modem2   = LoRa.readRegister(0x1E);
  uint8_t modem3   = LoRa.readRegister(0x26);
  uint8_t syncWord = LoRa.readRegister(0x39);
  uint8_t preMsb   = LoRa.readRegister(0x20);
  uint8_t preLsb   = LoRa.readRegister(0x21);
  uint16_t preamble= ((uint16_t)preMsb << 8) | preLsb;

  // Giải mã
  uint8_t bwBits   = (modem1 >> 4) & 0x0F;
  long    bwHz     = mapBW(bwBits);
  uint8_t crBits   = (modem1 >> 1) & 0x07; // 001 -> 4/5, 010->4/6, 011->4/7, 100->4/8
  int crDen        = 4 + crBits;          // để in 4/5..4/8
  bool   implHdr   = modem1 & 0x01;       // 1 = implicit header

  uint8_t sf       = (modem2 >> 4) & 0x0F; // 6..12
  bool   rxCrcOn   = modem2 & 0x04;        // CRC on at PHY
  bool   lowDRopt  = modem3 & 0x08;        // LowDataRateOptimize

  Serial.println(F("---- SX127x CONFIG DUMP ----"));
  Serial.printf(" OpMode: 0x%02X (should have LoRa bit set)\n", opmode);
  Serial.printf(" Freq: %.0f Hz (FRF=0x%06lX)\n", freqHz, (unsigned long)frf);
  Serial.printf(" SyncWord: 0x%02X\n", syncWord);
  Serial.printf(" BW: %ld Hz, SF: %u, CR: 4/%d\n", bwHz, sf, crDen);
  Serial.printf(" Header: %s\n", implHdr ? "Implicit" : "Explicit");
  Serial.printf(" PHY CRC: %s\n", rxCrcOn ? "Enabled" : "Disabled");
  Serial.printf(" Preamble: %u symbols\n", preamble);
  Serial.printf(" LowDataRateOptimize: %s\n", lowDRopt ? "ON" : "OFF");
  Serial.println(F("---------------------------"));
}
