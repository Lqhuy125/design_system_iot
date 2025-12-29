#ifndef _SENSOR_DATA_H
#define _SENSOR_DATA_H

#include <Arduino.h>

typedef struct __attribute__((packed)) {
  uint8_t id;
  float ax, ay, az;   /* in g */
  float gx, gy, gz;   /* in rad/s */
  float dt;           /* Seconds between samples*/
  float t_s;          /* Timestamp*/
  uint32_t crc;       /* Check sum field */
} IMUSample;
#endif