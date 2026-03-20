#include "main.h"

#define LOG_SIZE 1000

volatile uint8_t u8SlaveNodeID = 0xFU;

/*======================== TASK LOGGER ========================*/
void log_task(uint8_t task_id, uint16_t value);
void dump_logs();

typedef struct {
    uint16_t sensor[LOG_SIZE];
    uint16_t encrypt[LOG_SIZE];
    uint16_t lora_tx[LOG_SIZE];

    uint16_t idx_sensor;
    uint16_t idx_encrypt;
    uint16_t idx_lora;

    bool printed;
} TaskLogger;

TaskLogger logger = {0};
/*===========================================================*/

QueueHandle_t gTransmit;
QueueHandle_t gEncryptedQueue;  // New queue for encrypted data
TaskHandle_t  hSensor, hLoraProcess, hEncrypt;

SemaphoreHandle_t gI2CMutex;
SemaphoreHandle_t gLoraMutex;
SemaphoreHandle_t gLogMutex;
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
  
  // Get MAC Address
uint64_t chipid = ESP.getEfuseMac();

Serial.printf("ESP32 Chip ID: %llX\n", chipid);

if (chipid == 0x34B7C9EA6BE8ULL)
{
    u8SlaveNodeID = 1;
}
else if (chipid == 0x5C9D26077000ULL)   // thay bằng MAC thật
{
    u8SlaveNodeID = 3;
}
else if (chipid == 0x5C07B26062ECULL)   // thay bằng MAC thật
{
    u8SlaveNodeID = 2;
}
else
{
    u8SlaveNodeID = 0; // unknown node
}

Serial.printf("Slave Node ID: %d\n", u8SlaveNodeID);

  

  gI2CMutex = xSemaphoreCreateMutex();
  gLoraMutex = xSemaphoreCreateMutex();
  gLogMutex = xSemaphoreCreateMutex();
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
  

  IMUSample s;
  EncryptedPacket pkt;

  for (;;) {
    // Wait for raw IMU sample from sensor task
    if (xQueueReceive(gTransmit, &s, portMAX_DELAY) == pdTRUE) {
      uint64_t start = esp_timer_get_time();

      // Encrypt the IMU sample
      pkt.valid = secure_data_encrypt(s, pkt.cipher);

      uint64_t end = esp_timer_get_time();
      log_task(1, (uint16_t)(end - start));  // Log encrypt time

      if (pkt.valid) {
        // Send encrypted packet to LoRa task (overwrite if queue full)
        xQueueOverwrite(gEncryptedQueue, &pkt);
        #if DEBUG_APP
        
        #endif
      } else {
        
      }
    }

    vTaskDelay(pdMS_TO_TICKS(1));
  }
}

/*
    =========== LoRa TX Task ============
    Handles beacon reception and transmits encrypted data in TDMA slots
*/
volatile uint64_t end_lora;
void lora_tx_task(void* pv) {
  (void)pv;
  
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
        const uint8_t slot_idx = tdma_choose_slot(u8SlaveNodeID, b);

        // Skip if not our slot (targeted mode)
        if (slot_idx == 0xFF) {
          
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
          
          setModeRX();
          continue;
        }

        // 4) Get encrypted packet from queue (timeout = remaining time in slot)
        if (xQueueReceive(gEncryptedQueue, &pkt, pdMS_TO_TICKS(remain_ms)) == pdTRUE) {
          if (pkt.valid) {
            // 5) TX in slot
            setModeTX();
            uint64_t start = esp_timer_get_time();

            // Send encrypted data directly
            xSemaphoreTake(gLoraMutex, portMAX_DELAY);
            radio_config_uplink();
            int state = radio.transmit(pkt.cipher, SECURE_DATA_TOTAL_LEN);
            xSemaphoreGive(gLoraMutex);

            
            log_task(2, (uint16_t)(end_lora - start));  // Log LoRa TX time

            /* Move to recieve beacon moed */
            radio_config_beacon();
            /* Time delay for switch TX -> RX */
            radio.standby();
            delayMicroseconds(3000);   // 2–5 ms
            // 6) Return to RX mode
            setModeRX();
            #if DEBUG_APP
            
            #endif

            if (state != RADIOLIB_ERR_NONE) {
              
            }
          } else {
            
          }
        } else {
          
        }
      } else {
        
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

      
      
      
      
      
      
      
      
      
    }
  }
}

/*======================== TASK LOGGING FUNCTIONS ========================*/
void log_task(uint8_t task_id, uint16_t value)
{
    if (logger.printed) return;

    xSemaphoreTake(gLogMutex, portMAX_DELAY);

    switch (task_id)
    {
        case 0:
            if (logger.idx_sensor < LOG_SIZE)
                logger.sensor[logger.idx_sensor++] = value;
            break;

        case 1:
            if (logger.idx_encrypt < LOG_SIZE)
                logger.encrypt[logger.idx_encrypt++] = value;
            break;

        case 2:
            if (logger.idx_lora < LOG_SIZE)
                logger.lora_tx[logger.idx_lora++] = value;
            break;
    }

    bool ready =
        (logger.idx_sensor >= LOG_SIZE &&
         logger.idx_encrypt >= LOG_SIZE &&
         logger.idx_lora >= LOG_SIZE);

    xSemaphoreGive(gLogMutex);

    if (ready && !logger.printed)
    {
        logger.printed = true;   // ⚠️ set trước khi dump
        dump_logs();
    }
}

void dump_logs()
{
    Serial.println("===== TASK EXECUTION TIME LOGS (in microseconds) =====");

    Serial.println("SENSOR_LOG_START");
    for (int i = 0; i < LOG_SIZE; i++)
        Serial.println(logger.sensor[i]);
    Serial.println("SENSOR_LOG_END");

    Serial.println("ENCRYPT_LOG_START");
    for (int i = 0; i < LOG_SIZE; i++)
        Serial.println(logger.encrypt[i]);
    Serial.println("ENCRYPT_LOG_END");

    Serial.println("LORA_TX_LOG_START");
    for (int i = 0; i < LOG_SIZE; i++)
        Serial.println(logger.lora_tx[i]);
    Serial.println("LORA_TX_LOG_END");

    Serial.println("===== END OF LOGS =====");
}
