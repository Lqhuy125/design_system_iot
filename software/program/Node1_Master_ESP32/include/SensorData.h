#ifndef _SENSOR_DATA_H
#define _SENSOR_DATA_H

#include <Arduino.h>

struct __attribute__((packed)) SensorData {
  uint32_t id; 
  uint32_t crc;
  float accX, accY, accZ;
  float gyroX, gyroY, gyroZ;
  float temperature;
};

#endif