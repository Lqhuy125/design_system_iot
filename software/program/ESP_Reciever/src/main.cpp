#include "main.h"

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
  gMqttQueue = xQueueCreate(/*len=*/128, sizeof(IMUSample));

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

  Serial.println("RTOS pipeline started: TDMA -> TX Beacon -> RX -> Push Data");
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

/*======================== TDMA Scheduler ====================*/
void tdma_scheduler_task(void* pv) {
  Serial.println("tdma_scheduler_task");
  (void)pv;
  const uint32_t FRAME_MS = (uint32_t)cfg.slot_len_ms * cfg.total_slots;
  TickType_t lastWake = xTaskGetTickCount();
  const TickType_t period = pdMS_TO_TICKS(FRAME_MS);

  uint16_t frame_id = cfg.start_frame_id;

  /* Gửi beacon khởi động */
  {
    radioMode = RADIO_TX;
    Serial.println("tdma_send_beacon_broadcast");

    transmissionState = tdma_send_beacon_broadcast( frame_id,
                                                    cfg.slot_len_ms,
                                                    cfg.total_slots,
                                                    0);
    Serial.println("[TDMA] TX beacon frame_id=");
    Serial.print(frame_id);
  }

  // Serial.println("txDoneFlag"); Serial.print(txDoneFlag);
  // Chờ TX done -> chuyển sang RX
  while (!txDoneFlag)
  {
    vTaskDelay(pdMS_TO_TICKS(1));
  }
  txDoneFlag = false;
  if (transmissionState == RADIOLIB_ERR_NONE) {
    Serial.println("transmission finished!");
  } else {
    Serial.println("failed, code :");  Serial.print(transmissionState);
  }

  // Bắt đầu nhận cho phần còn lại của chu kỳ
  setModeRX();

  // --- Vòng lặp các chu kỳ TDMA ---
  for (;;) {
    uint32_t trace_start = micros();

    // Đợi tới đúng mốc chu kỳ kế tiếp
    vTaskDelayUntil(&lastWake, period);

    // Phát beacon cho chu kỳ mới
    frame_id++;
    radioMode = RADIO_TX;
    transmissionState = tdma_send_beacon_broadcast(frame_id, cfg.slot_len_ms, cfg.total_slots, 0);
    Serial.print("[TDMA] TX beacon frame_id=");  Serial.println(frame_id);

    // Chờ TX done
    while (!txDoneFlag) { vTaskDelay(pdMS_TO_TICKS(1)); }
    txDoneFlag = false;
    if (transmissionState == RADIOLIB_ERR_NONE) {
      Serial.println("transmission finished!");
    } else {
      Serial.println("failed, code:");
      Serial.println(transmissionState);
    }
    uint32_t trace_stop = micros();
    // Serial.println(" time to run: ");      Serial.print(trace_stop-trace_start);
    // Chuyển sang RX cho phần thời gian slots
    setModeRX();

    // (Không phát beacon trong nhánh RX nữa)
    // Serial.println("Send beacon for the next cycle (scheduled)");
  }
}

void lora_process_task(void* pv) {
  (void)pv;
  Serial.println("lora_process_task");
  uint8_t  rxBuf[SECURE_DATA_TOTAL_LEN];
  IMUSample s;

  for (;;) {

    if (rxDoneFlag) {
      // Clear cờ trước khi xử lý
      rxDoneFlag = false;
      // Đọc dữ liệu an toàn
      int state = radio.readData(rxBuf, SECURE_DATA_TOTAL_LEN);

      Serial.print("[LORA] CIPHER DATA RECIEVE: ");
      for(uint8_t i = 0; i< SECURE_DATA_TOTAL_LEN; i++)
      {
        Serial.print(rxBuf[i], HEX);
        Serial.print(" ");
      }
      Serial.println();

      if (state == RADIOLIB_ERR_NONE) {
        /* if (deserializeIMUSample(s, rxBuf, IMU_TOTAL_LEN)) {
          if (xQueueSend(gMqttQueue, &s, 0) != pdTRUE) {
            Serial.println("⚠️ gMqttQueue full, drop sample");
          }
        } */
        // Send cipher packet directly to MQTT queue (no decryption)
        if (xQueueSend(gMqttQueue, &rxBuf, 0) != pdTRUE) {
          Serial.println("⚠️ gMqttQueue full, drop packet");
        } else {
          Serial.println("✅ Cipher packet queued for MQTT");
        }
      }
      else {
        Serial.println("RX readData error:");
        Serial.println(state);
      }
    }
    vTaskDelay(pdMS_TO_TICKS(1));
  }
}

void mqtt_push_task(void* pv) {
  (void)pv;
  /* IMUSample s; */
  uint32_t lastLoop = 0;
  uint32_t lastPrint = 0;
  uint32_t cnt_loop = 0;
  // Serial.println("Checkpoint3");
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
            /* publishNodeData(s); */
            publishCipherData(rxBuf);
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
/* ================================ */