#include "ReadMPU.h"

Adafruit_MPU6050 mpu;

SensorData data;

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

// Print SensorData nicely
void printSensorData_READ(const SensorData &d) {
  Serial.println("==========READ DONE AND START SEND==========");
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

void InitMPU(void)
{
    Serial.println("Adafruit MPU6050 test!");

    // Try to initialize!
    if (!mpu.begin()) {
      Serial.println("Failed to find MPU6050 chip");
      while (1) {
        delay(10);
      }
    }
    Serial.println("MPU6050 Found!");

    //setupt motion detection
    mpu.setHighPassFilter(MPU6050_HIGHPASS_0_63_HZ);
    mpu.setMotionDetectionThreshold(1);
    mpu.setMotionDetectionDuration(20);
    mpu.setInterruptPinLatch(true);	// Keep it latched.  Will turn off when reinitialized.
    mpu.setInterruptPinPolarity(true);
    mpu.setMotionInterrupt(true);

    Serial.println("");
    delay(100);
}

void readMPU() {
  // đọc dữ liệu từ mpu
  /* Get new sensor events with the readings */
  sensors_event_t a, g, temp;
  mpu.getEvent(&a, &g, &temp);
  /* Print out the values */

  float accx = a.acceleration.x;
  float accy = a.acceleration.y;
  float accz = a.acceleration.z;
  float gyrox = g.gyro.x;
  float gyroy = g.gyro.y;
  float gyroz = g.gyro.z;
  float tmpture = temp.temperature;

  // Fill SensorData struct
  data = {0x0003, 0, accx, accy, accz, gyrox, gyroy, gyroz, tmpture};

  // CRC calculation (excluding crc field itself)
  uint32_t crc = calcCRC32(&data, sizeof(data) - sizeof(data.crc));

  data.crc = crc;

  printSensorData_READ(data);

  delay(500);
}