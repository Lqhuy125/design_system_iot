#include "SendLora.h"


void InitLora(void)
{
    if (!LoRa.begin(433E6)) { // or 915E6, the MHz speed of your module
      Serial.println("Starting LoRa failed!");
      Serial.println("Please check before run the program!");
      while (1);
    }
    else {
      Serial.println("Init lora module successful!");
    }
}

void SenData(const SensorData &data)
{
    // Copy sang mảng byte
    uint8_t buffer[sizeof(SensorData)];
    memcpy(buffer, &data, sizeof(SensorData));

    // Debug ra Serial
    printSensorData_SEND(data);

    // Gửi qua LoRa
    LoRa.beginPacket();
    LoRa.write(buffer, sizeof(SensorData));
    LoRa.endPacket();
}

void printSensorData_SEND(const SensorData &d) {
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
  Serial.println("==========SEND DONE==========");
}