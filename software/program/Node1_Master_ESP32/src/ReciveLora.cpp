#include "ReciveLora.h"

uint32_t calcCRC32(const void *data, size_t length) {
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

void deserializeSensorData(SensorData &d, const uint8_t *buffer) {
  int idx = 0;

  memcpy(&d.id, &buffer[idx], sizeof(d.id)); idx += sizeof(d.id);
  memcpy(&d.accX, &buffer[idx], sizeof(d.accX)); idx += sizeof(d.accX);
  memcpy(&d.accY, &buffer[idx], sizeof(d.accY)); idx += sizeof(d.accY);
  memcpy(&d.accZ, &buffer[idx], sizeof(d.accZ)); idx += sizeof(d.accZ);
  memcpy(&d.gyroX, &buffer[idx], sizeof(d.gyroX)); idx += sizeof(d.gyroX);
  memcpy(&d.gyroY, &buffer[idx], sizeof(d.gyroY)); idx += sizeof(d.gyroY);
  memcpy(&d.gyroZ, &buffer[idx], sizeof(d.gyroZ)); idx += sizeof(d.gyroZ);
  memcpy(&d.temperature, &buffer[idx], sizeof(d.temperature)); idx += sizeof(d.temperature);
  memcpy(&d.crc, &buffer[idx], sizeof(d.crc)); idx += sizeof(d.crc);
}

void printSensorData_RECIEVE(const SensorData &d)
{
    Serial.println("==========RECIEVE DONE AND START WAIT FOR THE NEXT==========");
    Serial.print("ID: "); Serial.println(d.id);
    Serial.print("CRC: 0x"); Serial.println(d.crc, HEX);

    Serial.print("Acc: ");
    Serial.print(d.accX); Serial.print(", ");
    Serial.print(d.accY); Serial.print(", ");
    Serial.println(d.accZ);

    Serial.print("Gyro: ");
    Serial.print(d.gyroX); Serial.print(", ");
    Serial.print(d.gyroY); Serial.print(", ");
    Serial.println(d.gyroZ);

    Serial.print("Temp: "); Serial.println(d.temperature, 2);
}
void InitLora(void){
    Serial.println("LoRa Receiver");
    LoRa.setPins(ss, rst, dio0);
    if (!LoRa.begin(433E6)) {
        Serial.println("Starting LoRa failed!");
        Serial.println("Check connect before run the program!");
        while (1);
    }
}

void RecieveData(void)
{
    // try to parse packet
    int packetSize = LoRa.parsePacket();
    if (packetSize == sizeof(SensorData)) {
        uint8_t buffer[64];
        int len = 0;
        // Đọc dữ liệu vào buffer
        while (len < packetSize && LoRa.available()) {
            buffer[len++] = (uint8_t)LoRa.read();
        }

        SensorData received;
        deserializeSensorData(received, buffer);

        // in ra kết quả
        printSensorData_RECIEVE(received);

        // Bây giờ có thể check CRC
        uint32_t calc = calcCRC32(&received, sizeof(SensorData) - sizeof(received.crc));
        if (calc == received.crc) {
        Serial.println("✅ CRC OK");
        } else {
        Serial.println("❌ CRC ERROR");
        }

        for (int i=0; i<sizeof(SensorData); i++) {
            Serial.print(buffer[i], HEX); Serial.print(" ");
        }
        Serial.println();
    }
}