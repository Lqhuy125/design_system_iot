#include "main.h"

QueueHandle_t gRxQueue, gMqttQueue;
TaskHandle_t  hLoRaRx, hHandleNodes, hMqttPush;

SemaphoreHandle_t gI2CMutex;
SemaphoreHandle_t gLoraMutex;

void setup() {
  Serial.begin(9600);

  gLoraMutex = xSemaphoreCreateMutex();

  InitLora();

  Init_Connection();

  /* Create queues Rx->Agg->MQTT */
  gRxQueue   = xQueueCreate(/*len=*/128, sizeof(IMUSample));
  gMqttQueue = xQueueCreate(/*len=*/128, sizeof(IMUSample));

  /* reate tasks, pin to cores (ESP32: core 0 & 1) */
  
  xTaskCreatePinnedToCore(lora_rx_task,     
                          "LoRaRxTask",     
                          4096, 
                          nullptr, 
                          3, 
                          &hLoRaRx,     
                          0);
  xTaskCreatePinnedToCore(aggregator_task,  
                          "AggregatorTask", 
                          4096, 
                          nullptr, 
                          2, 
                          &hHandleNodes,
                          1);
  xTaskCreatePinnedToCore(mqtt_push_task,   
                          "MqttPushTask",   
                          4096, 
                          nullptr, 
                          2, 
                          &hMqttPush,  
                          1);


  Serial.println("RTOS pipeline started: RX lora -> Handle Nodes -> Push Data");
}

void loop() {
  // RTOS tasks chạy riêng; loop() có thể để trống
  // vTaskDelay(pdMS_TO_TICKS(1000));
  
  /* Run without RTOS*/

  /* RecieveData(); */
}
/* 
    =========== Task on freeRTOS ============
*/

/* ================================ */