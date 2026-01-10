#ifndef _SENSOR_DATA_H
#define _SENSOR_DATA_H

#include <Arduino.h>

struct __attribute__((packed)) SensorData {
  uint32_t id; 
  float accX, accY, accZ;
  float gyroX, gyroY, gyroZ;
  float temperature;
  uint32_t crc;
};

typedef struct __attribute__((packed)) {
  uint8_t id;
  float ax, ay, az;  // in g
  float gx, gy, gz;  // in rad/s
  float dt;          // seconds between samples
  float t_s;  //timestamp
  uint32_t crc;
} IMUSample;
#endif