#include "main.h"

QueueHandle_t gRxQueue, gMqttQueue;
TaskHandle_t  hLoRaRx, hHandleNodes, hMqttPush;

SemaphoreHandle_t gI2CMutex;
SemaphoreHandle_t gLoraMutex;

/*======================== VAR LORA ========================*/
// flag to indicate transmission or reception state
bool transmitFlag = false;
// ====== ISR flag ======
volatile bool operationDone = false;
void IRAM_ATTR setFlag() {
  // Tuyệt đối KHÔNG print/khóa mutex trong ISR
  operationDone = true;
}
// save transmission states between loops
int transmissionState = RADIOLIB_ERR_NONE;
/*  Declare variable */
SX1278 radio = new Module(ss, dio0, rst);
IMUSample nodeData[MAX_NODES];
/*===========================================================*/

/*======================== VAR BEACON ========================*/

TDMA_BeaconConfig cfg;

/*===========================================================*/

void setup() {
  Serial.begin(9600);

  gLoraMutex = xSemaphoreCreateMutex();

  BeaconConfiguration();

  InitLora();

  /* Call one time beacon send when power on */
  /* If is Master node, it should be set before run */
  
  transmissionState = tdma_send_beacon_broadcast(1 /* Frame ID Init = 1 */, 
                                                 cfg.slot_len_ms, 
                                                 cfg.total_slots, 
                                                 0 /*  */
                                                 );
  transmitFlag = true;

  Init_Connection();

  /* Create queues Rx->Agg->MQTT */
  gRxQueue   = xQueueCreate(/*len=*/128, sizeof(IMUSample));
  // gMqttQueue = xQueueCreate(/*len=*/128, sizeof(IMUSample));

  gMqttQueue = xQueueCreate(/*len=*/128, sizeof(IMUSample));

  /* reate tasks, pin to cores (ESP32: core 0 & 1) */
  
  xTaskCreatePinnedToCore(lora_process_task,     
                          "LoRaRxTask",     
                          4096, 
                          nullptr, 
                          3, 
                          &hLoRaRx,     
                          1);
  /* xTaskCreatePinnedToCore(aggregator_task,  
                          "AggregatorTask", 
                          4096, 
                          nullptr, 
                          2, 
                          &hHandleNodes,
                          1); */
  xTaskCreatePinnedToCore(mqtt_push_task,   
                          "MqttPushTask",   
                          4096, 
                          nullptr, 
                          2, 
                          &hMqttPush,  
                          0);


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
void lora_process_task(void* pv) {
  (void)pv;
  uint8_t rxBuf[IMU_TOTAL_LEN];

  uint32_t lastPrint = 0;

  
  const uint32_t FRAME_MS = (uint32_t)cfg.slot_len_ms * cfg.total_slots;
  TickType_t lastWake = xTaskGetTickCount();
  TickType_t period   = pdMS_TO_TICKS(FRAME_MS);
  uint16_t frame_id = cfg.start_frame_id;

  while(1) {
    if (operationDone)
    {
      // reset flag
      operationDone = false;
      if(transmitFlag)
      { 
        /*Check The status of Transmit beacon and listen from slave node */
        if (transmissionState == RADIOLIB_ERR_NONE) {
          // packet was successfully sent
          Serial.println(F("transmission finished!"));

        } else {
          Serial.print(F("failed, code "));
          Serial.println(transmissionState);
        }
        /* Start switch to receive data mode */
        radio.startReceive();
        transmitFlag = false;
      }
      else
      {
        /* Recieve data for each slot */
        IMUSample s;
        int state = radio.readData(rxBuf, IMU_TOTAL_LEN);
        if (state == RADIOLIB_ERR_NONE) {

          bool check = deserializeIMUSample(s, rxBuf, IMU_TOTAL_LEN);
          if (check) {      // <-- GÁN GIÁ TRỊ CHO S
            if (xQueueSend(gMqttQueue, &s, 0) != pdTRUE) {    // <-- GIỜ MỚI SEND
              Serial.println("⚠️ gRxQueue full, drop sample");
            }
          }
        }
        
        // 2) Chờ đến đầu frame kế tiếp
        if (millis() - lastPrint >= FRAME_MS) {
          lastPrint = millis();

          frame_id++;
          /* Send another one beacon for the next cycle */
          transmissionState = tdma_send_beacon_broadcast(frame_id, cfg.slot_len_ms, cfg.total_slots, 0);
          transmitFlag = true;
          Serial.println("Send beacon for the next cycle");
        }
      }
    }
    vTaskDelay(pdMS_TO_TICKS(1));
  }
}
/* ================================ */