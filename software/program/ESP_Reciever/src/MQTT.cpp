#include "MQTT.h"

extern QueueHandle_t gMqttQueue;

WiFiClient espClient;
PubSubClient client(espClient);

const char *ssid = "QUANGHUY";
const char *password = "12121213";
/* Use test.mosquitto.org when can not connect for that 

ws://broker.hivemq.com:8000/mqtt

*/
const char *mqttServer = "broker.hivemq.com"; 
const int mqttPort = 1883; /* 1883 */

void Init_Connection(void)
{
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(1000);
        Serial.println("Connecting to WiFi...");
    }
    Serial.println("Connected to WiFi");
    Serial.println(WiFi.localIP());

    client.setServer(mqttServer, mqttPort);//connecting to mqtt server
    Serial.println("========1");
    client.setCallback(callback);
    Serial.println("========2");
    
    client.connect("ESP32_clientID");  // ESP will connect to mqtt broker with clientID
    Serial.println("========3");
}

void callback(char* topic, byte* payload, unsigned int length)
{
  
}

void publishNodeData(const IMUSample &d)
{
    char topic[64];
    char payload[32];

    // client.connect("ESP32_clientID");

    // Node name theo ID (node1, node2, ...)
    snprintf(topic, sizeof(topic), "bridge/node%d/acc/x", d.id);
    dtostrf(d.ax, 6, 2, payload);
    client.publish(topic, payload);

    snprintf(topic, sizeof(topic), "bridge/node%d/acc/y", d.id);
    dtostrf(d.ay, 6, 2, payload);
    client.publish(topic, payload);

    snprintf(topic, sizeof(topic), "bridge/node%d/acc/z", d.id);
    dtostrf(d.az, 6, 2, payload);
    client.publish(topic, payload);

    snprintf(topic, sizeof(topic), "bridge/node%d/gyro/x", d.id);
    dtostrf(d.gx, 6, 2, payload);
    client.publish(topic, payload);

    snprintf(topic, sizeof(topic), "bridge/node%d/gyro/y", d.id);
    dtostrf(d.gy, 6, 2, payload);
    client.publish(topic, payload);

    snprintf(topic, sizeof(topic), "bridge/node%d/gyro/z", d.id);
    dtostrf(d.gz, 6, 2, payload);
    client.publish(topic, payload);

    snprintf(topic, sizeof(topic), "bridge/node%d/dt", d.id);
    dtostrf(d.dt, 6, 2, payload);
    client.publish(topic, payload);

    snprintf(topic, sizeof(topic), "bridge/node%d/timestamp", d.id);
    dtostrf(d.t_s, 6, 2, payload);
    client.publish(topic, payload);

    Serial.print("📡 Published data for Node "); 
    Serial.println(d.id);

    if (!client.connected())
    {
        reconnect();
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


void mqtt_push_task(void* pv) {
  (void)pv;
  IMUSample s;
  uint32_t lastLoop = 0;

  for (;;) {
    // 1) chờ sample cần publish
    if (xQueueReceive(gMqttQueue, &s, portMAX_DELAY) == pdTRUE) {
      if (!client.connected()) {
        reconnect();
        if (!client.connected()) continue;
      }
      publishNodeData(s);  // giữ nguyên topic & format của bạn
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
