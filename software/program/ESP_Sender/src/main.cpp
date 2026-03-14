#include "main.h"

QueueHandle_t gTransmit;
TaskHandle_t  hSensor, hLoraProcess;

SemaphoreHandle_t gI2CMutex;
SemaphoreHandle_t gLoraMutex;

/*======================== VAR BEACON ========================*/

/*===========================================================*/

/*======================== LORA STATE ========================*/
/*  Declare variable */
SX1278 radio = new Module(ss, dio0, rst);
IMUSample nodeData[MAX_NODES];

enum RadioMode : uint8_t
{
  RADIO_IDLE = 0,
  RADIO_TX,
  RADIO_RX
};
volatile RadioMode radioMode = RADIO_IDLE;

volatile bool txDoneFlag = false;
volatile bool rxDoneFlag = false;

void setModeTX() { radioMode = RADIO_TX; }
void setModeRX() { radioMode = RADIO_RX; radio.startReceive();}

// DIO0 ISR: đánh dấu theo chế độ hiện tại
void IRAM_ATTR dio0_isr() {
  if (radioMode == RADIO_TX) {
    txDoneFlag = true;
  } else if (radioMode == RADIO_RX) {
    rxDoneFlag = true;
  }
}

/*===========================================================*/

/*======================WDG Config============================*/
esp_err_t ESP32_ERROR;
int i = 0;
int last = millis();
/*===========================================================*/
void setup() {
  Serial.begin(9600);
  Serial.println("Node ID:"); Serial.print(SLAVE_NODE_ID);

  gI2CMutex = xSemaphoreCreateMutex();
  gLoraMutex = xSemaphoreCreateMutex();

  InitLora();
  radio.setPacketReceivedAction(dio0_isr);

  Init_MPU6050();

  /* Create queues */
  gTransmit = xQueueCreate(/*length=*/1, sizeof(IMUSample));

  /* reate tasks, pin to cores (ESP32: core 0 & 1) */
  xTaskCreatePinnedToCore(sensor_task,        /* Name of task function  */
                          "SensorTask",       /* Name task */
                          4096,               /* usStackDepth */
                          (void*)gTransmit,   /* Queue handler, name Queue to refer */
                          2,                  /* Priority */
                          &hSensor,           /* Task hander */
                          0);                 /* Core ID */

  xTaskCreatePinnedToCore(lora_process_task,
    "LoRaProcess", 4096, nullptr, 2, &hLoraProcess, 1);

  setModeRX();
  Serial.println("RTOS pipeline started: Sensor->Transmit");
}

void loop() {
  // RTOS tasks chạy riêng; loop() có thể để trống

  /* Run without RTOS*/
  // transmit_without_rtos();

  /* RecieveData(); */
}
/*
    =========== Task on freeRTOS ============
*/
void lora_process_task(void* pv) {
  (void)pv;
  Serial.println("lora_process_task");
  setModeRX();                 // vào RX để bắt beacon ngay
  IMUSample s;
  // Serial.println(rxDoneFlag);
  // esp_task_wdt_add(NULL);

// Mốc thời gian cuối cùng có tín hiệu sống
  uint32_t last_progress_ms = millis();

  for (;;) {
    // 1) Khi có ngắt RX (DIO0), thử đọc beacon
    if (rxDoneFlag) {
      rxDoneFlag = false;

      TDMABeacon b;
      if (lora_receive_beacon(b)) {            // parse + CRC OK
        uint32_t rx_millis = millis();

        // 2) Tính slot theo TDMA
        /* if (b.total_slots == 0) {              // không có slot thì bỏ qua
          setModeRX();
          vTaskDelay(pdMS_TO_TICKS(1));
          continue;
        } */
        const uint8_t slot_idx = tdma_choose_slot(SLAVE_NODE_ID, b);
        // Skip if not our slot (targeted mode)
        if (slot_idx == 0xFF) {
            Serial.println("Not my slot, skipping");
            setModeRX();
            vTaskDelay(pdMS_TO_TICKS(1));
            continue;
        }
        uint32_t t_start = rx_millis + TDMA_GUARD_MS
                         + (uint32_t)b.slot_len_ms * slot_idx;
        uint32_t t_end   = t_start + b.slot_len_ms - TDMA_MARGIN_MS;

        // 3) Chờ đến đầu slot
        uint32_t now = millis();
        if (t_start > now) {
          vTaskDelay(pdMS_TO_TICKS(t_start - now));
        }
        now = millis();
        uint32_t remain_ms = (t_end > now) ? (t_end - now) : 0;
        if (remain_ms == 0) {
          Serial.println("Missed slot -> skip");
          setModeRX();
          continue;
        }

        // 4) Lấy mẫu IMU (timeout = còn lại trong slot)
        if (xQueueReceive(gTransmit, &s, pdMS_TO_TICKS(remain_ms)) == pdTRUE) {
          // 5) TX trong slot
          setModeTX();                           // báo chế độ cho ISR
          uint32_t t0 = micros();
          // lora_send_imusample(s);                // hàm này đã standby + delay + startReceive
          lora_send_imusample_secure(s);
          uint32_t t1 = micros();
          // Serial.println(rxDoneFlag);
          // 6) Luôn quay lại RX
          setModeRX();
          Serial.print("TX time (us): "); Serial.println(t1 - t0);
          // Serial.println(rxDoneFlag);7
        }
        else {
          Serial.println("No sample -> skip TX");
        }
      }
      else
      {
        Serial.println("Beacon parse/CRC failed");
        // Serial.println(rxDoneFlag);
        esp_restart();
        // setModeRX();

      }
    }
    // Nhịp thở nhẹ để nhường CPU
    vTaskDelay(pdMS_TO_TICKS(1));
  }
}
/* ================================ */

void transmit_without_rtos()
{
  IMUSample s;

  if (sensor_read(&s) == 0)
  {
    static uint32_t lastPrint = 0;
    if (millis() - lastPrint >= 500) {
      lastPrint = millis();

      // lora_send_imusample(s);

      Serial.print("  id: ");  Serial.print(s.id);
      Serial.print("  ax_n: ");  Serial.print(s.ax, 3);
      Serial.print("  ay_n: ");  Serial.print(s.ay, 3);
      Serial.print("  az_n: ");  Serial.print(s.az, 3);
      Serial.print("  gx: ");  Serial.print(s.gx, 3);
      Serial.print("  gy: ");  Serial.print(s.gy, 3);
      Serial.print("  gz: ");  Serial.print(s.gz, 3);
      Serial.print("  t(ms): "); Serial.println(s.dt * 1000.0f, 2);
      Serial.print(" time(s): "); Serial.println(s.t_s, 3);
    }

  }
}
