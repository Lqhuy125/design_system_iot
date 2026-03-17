#include "main.h"
#define LOG_SIZE 1000

void log_task(uint8_t task_id, uint16_t value);
void dump_logs();
typedef struct
{
    uint16_t tdma[LOG_SIZE];
    uint16_t lora_rx[LOG_SIZE];
    uint16_t mqtt[LOG_SIZE];

    uint16_t idx_tdma;
    uint16_t idx_lora;
    uint16_t idx_mqtt;

    bool printed;

} TaskLogger;

TaskLogger logger = {0};
QueueHandle_t gRxQueue, gMqttQueue;
TaskHandle_t hTDMAScheduler, hLoRaRx, hMqttPush;

SemaphoreHandle_t gI2CMutex;
SemaphoreHandle_t gLoraMutex;


/*======================== LORA STATE ========================*/
enum RadioMode : uint8_t
{
  RADIO_IDLE = 0,
  RADIO_TX,
  RADIO_RX
};
volatile RadioMode radioMode = RADIO_IDLE;

volatile bool txDoneFlag = false;
volatile bool rxDoneFlag = false;

static inline void setModeTX() { radioMode = RADIO_TX; }
static inline void setModeRX() { radioMode = RADIO_RX; radio.startReceive();}

// DIO0 ISR: đánh dấu theo chế độ hiện tại
void IRAM_ATTR dio0_isr() {
  if (radioMode == RADIO_TX) {
    txDoneFlag = true;
  } else if (radioMode == RADIO_RX) {
    rxDoneFlag = true;
  }
}
/*======================== VAR LORA ========================*/

// save transmission states between loops
int transmissionState = RADIOLIB_ERR_NONE;
/*  Declare variable */
SX1278 radio = new Module(ss, dio0, rst);
IMUSample nodeData[MAX_NODES];
/*===========================================================*/

/*======================== VAR BEACON ========================*/
TDMA_BeaconConfig cfg;
/*===========================================================*/

/*======================== VAR MQTT ========================*/
WiFiClient espClient;
PubSubClient client(espClient);
/*===========================================================*/
void setup() {
  Serial.begin(9600);

  gLoraMutex = xSemaphoreCreateMutex();

  BeaconConfiguration();

  InitLora();

  radio.setPacketReceivedAction(dio0_isr);

  radio.setPacketSentAction(dio0_isr);

  Init_Connection();

  /* Create queues Rx->Agg->MQTT */
  gRxQueue   = xQueueCreate(/*len=*/128, sizeof(IMUSample));
  // gMqttQueue = xQueueCreate(/*len=*/128, sizeof(IMUSample));
  gMqttQueue = xQueueCreate(/*len=*/128, sizeof(CipherPacket));

  /* Create tasks, pin to cores (ESP32: core 0 & 1) */
  xTaskCreatePinnedToCore(lora_process_task,
                          "LoRaRxTask",
                          4096,
                          nullptr,
                          3,
                          &hLoRaRx,
                          1);
  xTaskCreatePinnedToCore(tdma_scheduler_task,
                          "TDMAScheduler",
                          4096,
                          nullptr,
                          2,
                          &hTDMAScheduler,
                          1);
  xTaskCreatePinnedToCore(mqtt_push_task,
                          "MqttPushTask",
                          4096,
                          nullptr,
                          1,
                          &hMqttPush,
                          0);

  Serial.println("[MAIN] RTOS pipeline started: TDMA -> TX Beacon -> RX Data -> Push Data");
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

volatile uint64_t end_tdma_scheduler_task;
/*======================== TDMA Scheduler ====================*/
void tdma_scheduler_task(void* pv) {
  #if DEBUG_APP == 1
  Serial.println("[MAIN] TDMA scheduler task");
  #endif
  (void)pv;
  const uint32_t FRAME_MS = (uint32_t)cfg.slot_len_ms * cfg.total_slots;
  TickType_t lastWake = xTaskGetTickCount();
  const TickType_t period = pdMS_TO_TICKS(FRAME_MS);

  uint16_t frame_id = cfg.start_frame_id;

  /* Gửi beacon khởi động */
  {
    radioMode = RADIO_TX;
    #if DEBUG_APP == 1
    Serial.println("[MAIN] TDMA send beacon broadcast");
    #endif
    transmissionState = tdma_send_beacon_broadcast( frame_id,
                                                    cfg.slot_len_ms,
                                                    cfg.total_slots,
                                                    0);
    #if DEBUG_APP == 1                                                
    Serial.println("[TDMA] TX beacon frame_id=");
    Serial.print(frame_id);
    #endif
  }

  // Serial.println("txDoneFlag"); Serial.print(txDoneFlag);
  // Chờ TX done -> chuyển sang RX
  while (!txDoneFlag)
  {
    vTaskDelay(pdMS_TO_TICKS(1));
  }
  txDoneFlag = false;
  #if DEBUG_APP == 1
  if (transmissionState == RADIOLIB_ERR_NONE) {
    Serial.println("[MAIN] Transmission beacon finished!");
  } else {
    Serial.println("failed, code :");  Serial.print(transmissionState);
  }
  #endif
  // Bắt đầu nhận cho phần còn lại của chu kỳ
  setModeRX();

  // --- Vòng lặp các chu kỳ TDMA ---
  for (;;) {

    // Đợi tới đúng mốc chu kỳ kế tiếp
    vTaskDelayUntil(&lastWake, period);

    uint64_t start = esp_timer_get_time();
    // Phát beacon cho chu kỳ mới
    frame_id++;
    radioMode = RADIO_TX;
    transmissionState = tdma_send_beacon_broadcast(frame_id, cfg.slot_len_ms, cfg.total_slots, 0);
    #if DEBUG_APP == 1
    Serial.print("[TDMA] TX beacon frame_id=");  Serial.println(frame_id);
    #endif

    // Serial.printf("TDMA scheduler exec: %llu us\n", end_tdma_scheduler_task - start);
    log_task(0, end_tdma_scheduler_task - start);
    // Chờ TX done
    while (!txDoneFlag) { vTaskDelay(pdMS_TO_TICKS(1)); }
    txDoneFlag = false;
    #if DEBUG_APP == 1
    if (transmissionState == RADIOLIB_ERR_NONE) {
      Serial.println("[MAIN] transmission finished!");
    } else {
      Serial.println("failed, code:");
      Serial.println(transmissionState);
    }
    #endif
    // Chuyển sang RX cho phần thời gian slots
    setModeRX();

    // (Không phát beacon trong nhánh RX nữa)
    // 
  }
}

void lora_process_task(void* pv) {
  (void)pv;
  #if DEBUG_APP == 1
  Serial.println("[MAIN] Lora process task");
  #endif
  uint8_t  rxBuf[SECURE_DATA_TOTAL_LEN];
  /* IMUSample s; */
  CipherPacket cpk;
  for (;;) {

    if (rxDoneFlag) {
      // Clear cờ trước khi xử lý
      rxDoneFlag = false;
      uint64_t start = esp_timer_get_time();
      // Đọc dữ liệu an toàn
      int state = radio.readData(cpk.data, SECURE_DATA_TOTAL_LEN);
      cpk.timestamp = millis();

#if DEBUG_APP == 1
      Serial.print("[LORA] CIPHER DATA RECIEVE: ");
      for(uint8_t i = 0; i< SECURE_DATA_TOTAL_LEN; i++)
      {
        Serial.print(cpk.data[i], HEX);
        Serial.print(" ");
      }
      Serial.println();
#endif
      if (state == RADIOLIB_ERR_NONE) {
        /* if (deserializeIMUSample(s, rxBuf, IMU_TOTAL_LEN)) {
          if (xQueueSend(gMqttQueue, &s, 0) != pdTRUE) {
            Serial.println("⚠️ gMqttQueue full, drop sample");
          }
        } */
        // Send cipher packet directly to MQTT queue (no decryption)
        if (xQueueSend(gMqttQueue, &cpk, 0) != pdTRUE) {
          Serial.println("[MAIN] gMqttQueue full, drop packet");
        } else {
          Serial.println("[MAIN] Cipher packet queued for MQTT");
        }
      }
      else {
        #if DEBUG_APP == 1
        Serial.println("[MAIN] RX readData error:");
        Serial.println(state);
        #endif
      }
      uint64_t end = esp_timer_get_time();
      
      log_task(1, end - start);
    }
    vTaskDelay(pdMS_TO_TICKS(1));
  }
}

void mqtt_push_task(void* pv) {
  (void)pv;
  #if DEBUG_APP == 1
  Serial.println("[MAIN] Pushing MQTT task");
  #endif
  /* IMUSample s; */
  CipherPacket cpk;
  uint32_t lastLoop = 0;
  uint32_t lastPrint = 0;
  uint32_t cnt_loop = 0;
  
  TickType_t xLastWakeTime = xTaskGetTickCount();
  const TickType_t xPeriod = pdMS_TO_TICKS(50);
  for (;;) {

    /* 1. check MQTT connection */
    if (!client.connected()) {
      reconnect();
    }

     /* 2. Take packet if have */
    if (xQueueReceive(gMqttQueue, &cpk, 0) == pdTRUE)
    {
        uint32_t t0 = micros();

        publishCipherData(cpk);

        uint32_t t1 = micros();
        
        log_task(2, t1 - t0);
    }

    /* 3. maintain MQTT */
    if(millis()-lastLoop>=100)
    {
      lastLoop=millis();
      client.loop();
    }
    
    /* 4. wait until next 50ms period */
    vTaskDelayUntil(&xLastWakeTime, xPeriod);
  }
}
/* ================================ */
void log_task(uint8_t task_id, uint16_t value)
{
    if(logger.printed) return;

    switch(task_id)
    {
        case 0: // TDMA
            if(logger.idx_tdma < LOG_SIZE)
                logger.tdma[logger.idx_tdma++] = value;
            break;

        case 1: // LORA RX
            if(logger.idx_lora < LOG_SIZE)
                logger.lora_rx[logger.idx_lora++] = value;
            break;

        case 2: // MQTT
            if(logger.idx_mqtt < LOG_SIZE)
                logger.mqtt[logger.idx_mqtt++] = value;
            break;
    }

    if(logger.idx_tdma == LOG_SIZE &&
       logger.idx_lora == LOG_SIZE &&
       logger.idx_mqtt == LOG_SIZE)
    {
        dump_logs();
        logger.printed = true;
    }
}
void dump_logs()
{
    Serial.println("TDMA_LOG_START");

    for(int i=0;i<LOG_SIZE;i++)
        Serial.println(logger.tdma[i]);

    Serial.println("TDMA_LOG_END");


    Serial.println("LORA_RX_LOG_START");

    for(int i=0;i<LOG_SIZE;i++)
        Serial.println(logger.lora_rx[i]);

    Serial.println("LORA_RX_LOG_END");


    Serial.println("MQTT_LOG_START");

    for(int i=0;i<LOG_SIZE;i++)
        Serial.println(logger.mqtt[i]);

    Serial.println("MQTT_LOG_END");
}