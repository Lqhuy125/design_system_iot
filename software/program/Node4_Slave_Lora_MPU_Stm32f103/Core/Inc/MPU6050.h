/*
 * MPU6050.h
 *
 *  Created on: Sep 27, 2025
 *      Author: Quang Huy
 */

#ifndef INC_MPU6050_H_
#define INC_MPU6050_H_

#include "SensorData.h"

void sample_data(SensorData* d);

uint32_t calcCRC32(const void *data, size_t length);
#endif /* INC_MPU6050_H_ */
