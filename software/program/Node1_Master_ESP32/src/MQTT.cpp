#include "MQTT.h"

WiFiClient espClient;
PubSubClient client(espClient);

const char *ssid = "QUANGHUY";
const char *password = "12121213";
/* Use test.mosquitto.org when can not connect for that 

ws://broker.hivemq.com:8000/mqtt

*/
const char *mqttServer = "test.mosquitto.org"; 
const int mqttPort = 1883; /* 1883 */

void Init_Connection()
{
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(1000);
        Serial.println("Connecting to WiFi...");
    }
    Serial.println("Connected to WiFi");
    Serial.println(WiFi.localIP());

    client.setServer(mqttServer, mqttPort);//connecting to mqtt server
    client.setCallback(callback);
    
    client.connect("ESP32_clientID");  // ESP will connect to mqtt broker with clientID
}

void publishNodeData(const SensorData &d) {
    char topic[64];
    char payload[32];

    // client.connect("ESP32_clientID");

    // Node name theo ID (node1, node2, ...)
    snprintf(topic, sizeof(topic), "bridge/node%d/acc/x", d.id);
    dtostrf(d.accX, 6, 2, payload);
    client.publish(topic, payload);

    snprintf(topic, sizeof(topic), "bridge/node%d/acc/y", d.id);
    dtostrf(d.accY, 6, 2, payload);
    client.publish(topic, payload);

    snprintf(topic, sizeof(topic), "bridge/node%d/acc/z", d.id);
    dtostrf(d.accZ, 6, 2, payload);
    client.publish(topic, payload);

    snprintf(topic, sizeof(topic), "bridge/node%d/gyro/x", d.id);
    dtostrf(d.gyroX, 6, 2, payload);
    client.publish(topic, payload);

    snprintf(topic, sizeof(topic), "bridge/node%d/gyro/y", d.id);
    dtostrf(d.gyroY, 6, 2, payload);
    client.publish(topic, payload);

    snprintf(topic, sizeof(topic), "bridge/node%d/gyro/z", d.id);
    dtostrf(d.gyroZ, 6, 2, payload);
    client.publish(topic, payload);

    snprintf(topic, sizeof(topic), "bridge/node%d/tmpture", d.id);
    dtostrf(d.temperature, 6, 2, payload);
    client.publish(topic, payload);

    Serial.print("📡 Published data for Node "); 
    Serial.println(d.id);

    if (!client.connected())
    {
        reconnect();
    }
}

void reconnect() {
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

void callback(char* topic, byte* payload, unsigned int length)
{

}

