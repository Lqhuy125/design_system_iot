#include "MQTT.h"

// ==== Dêfine for each region ====
#define AREA_ID  1
// ==== RTOS objects (extern) ====
extern QueueHandle_t gMqttQueue;

// ==== WiFi / MQTT ====
WiFiClient espClient;
PubSubClient client(espClient);

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


void mqtt_push_task(void* pv) {
  (void)pv;
  IMUSample s;
  uint32_t lastLoop = 0;
  uint32_t lastPrint = 0;
  uint32_t cnt_loop = 0;
  for (;;) {
    // 1) chờ sample cần publish
    if (xQueueReceive(gMqttQueue, &s, portMAX_DELAY) == pdTRUE) {

      if (millis() - lastPrint >= 50) {
          lastPrint = millis();

          if (!client.connected()){
            reconnect();
          }
          if (client.connected())
          {
            publishNodeData(s);  
          }
      }
    }

    // 2) duy trì MQTT (keep-alive, callback...)
    uint32_t now = millis();
    if (now - lastLoop >= 100) {
      lastLoop = now;
      client.loop();
    }
    vTaskDelay(pdMS_TO_TICKS(5));
  } 
}

// ==== Helpers ====
static inline void makeNodeIdStr(char* out, size_t len, int id) {
  // id=1 -> "N01", id=12 -> "N12"
  snprintf(out, len, "N%02d", id);
}
