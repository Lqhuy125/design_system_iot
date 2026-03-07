#include "MQTT.h"

// ==== Dêfine for each region ====
#define AREA_ID  1
// ==== RTOS objects (extern) ====
extern QueueHandle_t gMqttQueue;

// ==== WiFi / MQTT ====
extern WiFiClient espClient;
extern PubSubClient client;

// ==== Config ====
static const char* WIFI_SSID = "QUANGHUY";
static const char* WIFI_PASS = "12121213";
static const char* MQTT_HOST = "broker.hivemq.com";
static const uint16_t MQTT_PORT = 1883;
/* Use test.mosquitto.org when can not connect for that

ws://broker.hivemq.com:8000/mqtt */

// ==== Public APIs ====
void Init_Connection(void)
{
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    while (WiFi.status() != WL_CONNECTED) {
        delay(1000);
        Serial.println("Connecting to WiFi...");
    }
    Serial.println("Connected to WiFi");
    Serial.println(WiFi.localIP());

    client.setServer(MQTT_HOST, MQTT_PORT);//connecting to mqtt server
    client.setCallback(callback);
    client.connect("ESP32_clientID");  // ESP will connect to mqtt broker with clientID
}

void callback(char* topic, byte* payload, unsigned int length)
{

}

void publishNodeData(const IMUSample &d)
{
    if (!client.connected())
    {
        reconnect();
        Serial.println("reconnect");
    }

    char nodeIdStr[8];
    makeNodeIdStr(nodeIdStr, sizeof(nodeIdStr), (int)d.id);

    char topic[64];
    snprintf(topic, sizeof(topic), "bridge/%d/%s/imu", AREA_ID, nodeIdStr);
    // client.connect("ESP32_clientID");

    // Node name theo ID (node1, node2, ...)
    char json[256];

      int n = snprintf(
        json, sizeof(json),
        "{\"ts\":%lu,\"areaId\":%d,\"nodeId\":\"%s\","
        "\"ax\":%.3f,\"ay\":%.3f,\"az\":%.3f,"
        "\"gx\":%.2f,\"gy\":%.2f,\"gz\":%.2f}",
        (unsigned long)d.t_s, AREA_ID, nodeIdStr,
        d.ax, d.ay, d.az, d.gx, d.gy, d.gz
      );
      if (n < 0 || n >= (int)sizeof(json)) {
        Serial.println("❌ JSON truncated/oversize");
        return;
      }

    // // PubSubClient publish (QoS 0). Nếu cần retain cho status thì thêm ở topic khác.
    bool ok = client.publish(topic, json /* retain = false */);
    if (!ok) {
      Serial.println("⚠️ MQTT publish failed (imu)");
    }
    // Serial.println("end push");
}

void publishCipherData(const CipherPacket &pkt)
{
    if (!client.connected())
    {
        reconnect();
        Serial.println("reconnect");
    }

    char topic[64];
    snprintf(topic, sizeof(topic), "bridge/%d/cipher", AREA_ID);

    // Convert cipher data to hex string
    char hexStr[SECURE_DATA_TOTAL_LEN * 2 + 1];
    for (size_t i = 0; i < SECURE_DATA_TOTAL_LEN; i++) {
        sprintf(&hexStr[i * 2], "%02X", pkt.data[i]);
    }
    hexStr[SECURE_DATA_TOTAL_LEN * 2] = '\0';

    // Build JSON payload
    char json[256];
    int n = snprintf(
        json, sizeof(json),
        "{\"ts\":%lu,\"areaId\":%d,\"len\":%d,\"cipher\":\"%s\"}",
        pkt.timestamp,
        AREA_ID,
        SECURE_DATA_TOTAL_LEN,
        hexStr
    );

    if (n < 0 || n >= (int)sizeof(json)) {
        Serial.println("❌ JSON truncated/oversize");
        return;
    }

    bool ok = client.publish(topic, json);
    if (ok) {
        Serial.println("✅ MQTT cipher published");
    } else {
        Serial.println("⚠️ MQTT publish failed (cipher)");
    }
}
void reconnect()
{
    client.connect("ESP32_clientID");

    while (!client.connected()) {
        Serial.println("Attempting MQTT connection...");
        if (client.connect("ESP32_clientID")) {
        Serial.println("connected");
        // Once connected, publish an announcement...
        client.publish("notify", "Nodemcu connected to MQTT");

        } else {
        Serial.print("failed, rc=");
        Serial.print(client.state());
        Serial.println(" try again in 5 seconds");
        // Wait 5 seconds before retrying
        delay(5000);
        }
    }
}

// ==== Helpers ====
static inline void makeNodeIdStr(char* out, size_t len, int id) {
  // id=1 -> "N01", id=12 -> "N12"
  snprintf(out, len, "N%02d", id);
}
