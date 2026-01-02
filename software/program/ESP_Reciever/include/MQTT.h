#ifndef _MQTT_H
#define _MQTT_H

#include <main.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include "SensorData.h"

/* const char *mqttUser = "Lqhuy125";
const char *mqttPassword = "1252206"; */

void Init_Connection(void);

void callback(char* topic, byte* payload, unsigned int length);

void publishNodeData(const IMUSample &d);

void reconnect();

void mqtt_push_task(void* pv);

static inline void makeNodeIdStr(char* out, size_t len, int id);
#endif