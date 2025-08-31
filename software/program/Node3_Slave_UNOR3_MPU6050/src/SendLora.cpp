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
  uint8_t buffer[64]; // đủ lớn
  int len = serializeSensorData(data, buffer);

  LoRa.beginPacket();
  LoRa.write(buffer, len);
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