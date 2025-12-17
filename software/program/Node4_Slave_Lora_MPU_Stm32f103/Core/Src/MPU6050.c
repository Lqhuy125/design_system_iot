/*
 * MPU6050.c
 *
 *  Created on: Sep 27, 2025
 *      Author: Quang Huy
 */

#include "MPU6050.h"

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

void sample_data(SensorData* d){
	d->id = 4;
	d->accX = -1.1;
	d->accY = -0.6;
	d->accZ = 10.5;
	d->gyroX = 0.05;
	d->gyroY = -0.02;
	d->gyroZ = 0.03;
	d->temperature = 3;
	d->crc = 49.8;

	uint32_t crc = calcCRC32(d, sizeof(SensorData) - sizeof(d->crc));
	d->crc = crc;
}

