#include "main.h"

QueueHandle_t gTransmit;
QueueHandle_t gEncryptedQueue;  // New queue for encrypted data
TaskHandle_t  hSensor, hLoraProcess, hEncrypt;

SemaphoreHandle_t gI2CMutex;
SemaphoreHandle_t gLoraMutex;

/*======================== VAR BEACON ========================*/

/*===========================================================*/

/*======================== LORA STATE ========================*/
/*  Declare variable */
SX1278 radio = new Module(ss, dio0, rst);
IMUSample nodeData[MAX_NODES];

// Structure to hold encrypted data for queue
struct EncryptedPacket {
  uint8_t cipher[SECURE_DATA_TOTAL_LEN];
  bool    valid;
};

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

/* Forward declarations for new tasks */
void encrypt_task(void* pv);
void lora_tx_task(void* pv);

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
  gEncryptedQueue = xQueueCreate(/*length=*/1, sizeof(EncryptedPacket));

  /* Create tasks, pin to cores (ESP32: core 0 & 1) */
  xTaskCreatePinnedToCore(sensor_task,        /* Name of task function  */
                          "SensorTask",       /* Name task */
                          4096,               /* usStackDepth */
                          (void*)gTransmit,   /* Queue handler, name Queue to refer */
                          2,                  /* Priority */
                          &hSensor,           /* Task hander */
                          0);                 /* Core ID */

  xTaskCreatePinnedToCore(encrypt_task,
    "EncryptTask", 4096, nullptr, 2, &hEncrypt, 0);  // Core 0 for encryption

  xTaskCreatePinnedToCore(lora_tx_task,
    "LoRaTxTask", 4096, nullptr, 2, &hLoraProcess, 1);  // Core 1 for LoRa

  setModeRX();
  Serial.println("RTOS pipeline started: Sensor->Encrypt->LoRaTx");
}

void loop() {
  // RTOS tasks chạy riêng; loop() có thể để trống
}

/*
    =========== Encrypt Task ============
    Receives raw IMU samples, encrypts them, and sends to encrypted queue
*/
void encrypt_task(void* pv) {
  (void)pv;
  Serial.println("encrypt_task started");

  IMUSample s;
  EncryptedPacket pkt;

  for (;;) {
    // Wait for raw IMU sample from sensor task
    if (xQueueReceive(gTransmit, &s, portMAX_DELAY) == pdTRUE) {
      // Encrypt the IMU sample
      pkt.valid = secure_data_encrypt(s, pkt.cipher);

      if (pkt.valid) {
        // Send encrypted packet to LoRa task (overwrite if queue full)
        xQueueOverwrite(gEncryptedQueue, &pkt);
        #if DEBUG_APP
        Serial.println("[ENCRYPT] Packet encrypted and queued");
        #endif
      } else {
        Serial.println("[ENCRYPT] Encryption failed!");
      }
    }

    vTaskDelay(pdMS_TO_TICKS(1));
  }
}

/*
    =========== LoRa TX Task ============
    Handles beacon reception and transmits encrypted data in TDMA slots
*/
void lora_tx_task(void* pv) {
  (void)pv;
  Serial.println("lora_tx_task started");
  setModeRX();  // Start in RX mode to catch beacon

  EncryptedPacket pkt;

  for (;;) {
    // 1) When RX interrupt (DIO0), try to read beacon
    if (rxDoneFlag) {
      rxDoneFlag = false;

      TDMABeacon b;
      if (lora_receive_beacon(b)) {  // parse + CRC OK
        uint32_t rx_millis = millis();

        // 2) Calculate slot according to TDMA
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

        // 3) Wait until start of slot
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

        // 4) Get encrypted packet from queue (timeout = remaining time in slot)
        if (xQueueReceive(gEncryptedQueue, &pkt, pdMS_TO_TICKS(remain_ms)) == pdTRUE) {
          if (pkt.valid) {
            // 5) TX in slot
            setModeTX();
            uint32_t t0 = micros();

            // Send encrypted data directly
            xSemaphoreTake(gLoraMutex, portMAX_DELAY);
            radio_config_uplink();
            int state = radio.transmit(pkt.cipher, SECURE_DATA_TOTAL_LEN);
            xSemaphoreGive(gLoraMutex);

            uint32_t t1 = micros();

            /* Move to recieve beacon moed */
            radio_config_beacon();
            /* Time delay for switch TX -> RX */
            radio.standby();
            delayMicroseconds(3000);   // 2–5 ms
            // 6) Return to RX mode
            setModeRX();
            Serial.print("TX time (us): "); Serial.println(t1 - t0);

            if (state != RADIOLIB_ERR_NONE) {
              Serial.print("TX failed, code: "); Serial.println(state);
            }
          } else {
            Serial.println("Invalid encrypted packet -> skip TX");
          }
        } else {
          Serial.println("No encrypted sample -> skip TX");
        }
      } else {
        Serial.println("Beacon parse/CRC failed");
        esp_restart();
      }
    }

    // Yield to other tasks
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
