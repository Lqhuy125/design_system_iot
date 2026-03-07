#ifndef _MQTT_H
#define _MQTT_H

#include <WiFi.h>
#include <PubSubClient.h>
#include "SensorData.h"

#define SECURE_DATA_TOTAL_LEN    48 

typedef struct
{
    uint8_t data[SECURE_DATA_TOTAL_LEN];
    uint32_t timestamp;
} CipherPacket;

/* const char *mqttUser = "Lqhuy125";
const char *mqttPassword = "1252206"; */

void Init_Connection(void);

void callback(char* topic, byte* payload, unsigned int length);

void publishNodeData(const IMUSample &d);

void reconnect();

void mqtt_push_task(void* pv);
void publishCipherData(const CipherPacket &pkt);
static inline void makeNodeIdStr(char* out, size_t len, int id);
#endif