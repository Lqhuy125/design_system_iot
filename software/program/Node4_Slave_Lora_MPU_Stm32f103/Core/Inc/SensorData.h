/*
 * SensorData.h
 *
 *  Created on: Sep 27, 2025
 *      Author: Quang Huy
 */

#ifndef INC_SENSORDATA_H_
#define INC_SENSORDATA_H_

#include "main.h"

typedef struct __attribute__((packed)){
  uint32_t id;
  float accX, accY, accZ;
  float gyroX, gyroY, gyroZ;
  float temperature;
  uint32_t crc;
} SensorData;

#endif /* INC_SENSORDATA_H_ */
