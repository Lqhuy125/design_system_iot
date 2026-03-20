#include "main.h"
#define LOG_SIZE 1000

// ==================== PACKET CONFIG ====================
#define PACKET_TOTAL_LEN (SECURE_DATA_TOTAL_LEN + sizeof(uint16_t))

// ==================== PDR TRACKER ====================
#define MAX_NODES 8

typedef struct {
    uint16_t last_seq_id;
    uint32_t packets_received;
    uint32_t packets_lost;
    bool     initialized;
} NodePDR;

typedef struct {
    NodePDR nodes[MAX_NODES];
    SemaphoreHandle_t mutex;
} PDRTracker;

PDRTracker g_pdr_tracker;

void pdr_tracker_init(PDRTracker* tracker) {
    tracker->mutex = xSemaphoreCreateMutex();
    for (int i = 0; i < MAX_NODES; i++) {
        tracker->nodes[i].last_seq_id = 0;
        tracker->nodes[i].packets_received = 0;
        tracker->nodes[i].packets_lost = 0;
        tracker->nodes[i].initialized = false;
    }
}

void pdr_tracker_update(PDRTracker* tracker, uint8_t node_id, uint16_t seq_id) {
    if (node_id >= MAX_NODES) return;

    xSemaphoreTake(tracker->mutex, portMAX_DELAY);

    NodePDR* node = &tracker->nodes[node_id];

    if (!node->initialized) {
        node->initialized = true;
        node->last_seq_id = seq_id;
        node->packets_received = 1;
        node->packets_lost = 0;
    } else {
        uint16_t expected = node->last_seq_id + 1;
        uint16_t lost = 0;

        if (seq_id == expected) {
            lost = 0;
        } else if (seq_id > expected) {
            lost = seq_id - expected;
        } else {
            // Wrap-around case
            lost = (0xFFFF - expected) + seq_id + 1;
            if (lost > 1000) {
                // Duplicate/old packet, ignore
                xSemaphoreGive(tracker->mutex);
                return;
            }
        }

        node->packets_lost += lost;
        node->packets_received++;
        node->last_seq_id = seq_id;
    }

    xSemaphoreGive(tracker->mutex);
}

float pdr_tracker_get(PDRTracker* tracker, uint8_t node_id) {
    if (node_id >= MAX_NODES) return 0.0f;

    xSemaphoreTake(tracker->mutex, portMAX_DELAY);

    NodePDR* node = &tracker->nodes[node_id];
    float pdr = 0.0f;

    if (node->initialized) {
        uint32_t total = node->packets_received + node->packets_lost;
        if (total > 0) {
            pdr = (float)node->packets_received / (float)total * 100.0f;
        }
    }

    xSemaphoreGive(tracker->mutex);
    return pdr;
}

void pdr_tracker_print_all(PDRTracker* tracker) {
    xSemaphoreTake(tracker->mutex, portMAX_DELAY);

    Serial.println("========== PDR REPORT ==========");
    for (int i = 0; i < MAX_NODES; i++) {
        NodePDR* node = &tracker->nodes[i];
        if (node->initialized) {
            uint32_t total = node->packets_received + node->packets_lost;
            float pdr = (total > 0) ? ((float)node->packets_received / total * 100.0f) : 0.0f;

            Serial.printf("Node %d: RX=%lu, Lost=%lu, Total=%lu, PDR=%.2f%%\n",
                          i, node->packets_received, node->packets_lost, total, pdr);
        }
    }
    Serial.println("================================");

    xSemaphoreGive(tracker->mutex);
}

// ==================== TASK LOGGER ====================
void log_task(uint8_t task_id, uint16_t value);
void dump_logs();

typedef struct {
    uint16_t tdma[LOG_SIZE];
    uint16_t lora_rx[LOG_SIZE];
    uint16_t mqtt[LOG_SIZE];

    uint16_t idx_tdma;
    uint16_t idx_lora;
    uint16_t idx_mqtt;

    bool printed;
} TaskLogger;

TaskLogger logger = {0};

// ==================== QUEUES & TASKS ====================
QueueHandle_t gRxQueue, gMqttQueue;
TaskHandle_t hTDMAScheduler, hLoRaRx, hMqttPush, hPDRReport;

SemaphoreHandle_t gI2CMutex;
SemaphoreHandle_t gLoraMutex;

// ==================== LORA STATE ====================
enum RadioMode : uint8_t {
    RADIO_IDLE = 0,
    RADIO_TX,
    RADIO_RX
};
volatile RadioMode radioMode = RADIO_IDLE;

volatile bool txDoneFlag = false;
volatile bool rxDoneFlag = false;

static inline void setModeTX() { radioMode = RADIO_TX; }
static inline void setModeRX() { radioMode = RADIO_RX; radio.startReceive(); }

void IRAM_ATTR dio0_isr() {
    if (radioMode == RADIO_TX) {
        txDoneFlag = true;
    } else if (radioMode == RADIO_RX) {
        rxDoneFlag = true;
    }
}

// ==================== VAR LORA ====================
int transmissionState = RADIOLIB_ERR_NONE;
SX1278 radio = new Module(ss, dio0, rst);
IMUSample nodeData[MAX_NODES];

// ==================== VAR BEACON ====================
TDMA_BeaconConfig cfg;

// ==================== VAR MQTT ====================
WiFiClient espClient;
PubSubClient client(espClient);

// ==================== CIPHER PACKET (updated) ====================
struct CipherPacket {
    uint8_t  data[SECURE_DATA_TOTAL_LEN];
    uint16_t seq_id;
    uint8_t  node_id;
    uint32_t timestamp;
};

// ==================== SETUP ====================
void setup() {
    Serial.begin(9600);

    pdr_tracker_init(&g_pdr_tracker);

    gLoraMutex = xSemaphoreCreateMutex();

    BeaconConfiguration();
    InitLora();

    radio.setPacketReceivedAction(dio0_isr);
    radio.setPacketSentAction(dio0_isr);

    Init_Connection();

    gRxQueue   = xQueueCreate(128, sizeof(IMUSample));
    gMqttQueue = xQueueCreate(128, sizeof(CipherPacket));

    xTaskCreatePinnedToCore(lora_process_task, "LoRaRxTask", 4096, nullptr, 3, &hLoRaRx, 1);
    xTaskCreatePinnedToCore(tdma_scheduler_task, "TDMAScheduler", 4096, nullptr, 2, &hTDMAScheduler, 1);
    xTaskCreatePinnedToCore(mqtt_push_task, "MqttPushTask", 4096, nullptr, 1, &hMqttPush, 0);
    xTaskCreatePinnedToCore(pdr_report_task, "PDRReport", 2048, nullptr, 1, &hPDRReport, 0);

    Serial.println("[MAIN] RTOS pipeline started: TDMA -> TX Beacon -> RX Data -> Push Data");
}

void loop() {
    // RTOS tasks running
}

// ==================== TDMA SCHEDULER ====================
volatile uint64_t end_tdma_scheduler_task;

void tdma_scheduler_task(void* pv) {
    (void)pv;
    #if DEBUG_APP == 1
    Serial.println("[MAIN] TDMA scheduler task");
    #endif

    const uint32_t FRAME_MS = (uint32_t)cfg.slot_len_ms * cfg.total_slots;
    TickType_t lastWake = xTaskGetTickCount();
    const TickType_t period = pdMS_TO_TICKS(FRAME_MS);

    uint16_t frame_id = cfg.start_frame_id;

    // Initial beacon
    {
        radioMode = RADIO_TX;
        #if DEBUG_APP == 1
        Serial.println("[MAIN] TDMA send beacon broadcast");
        #endif
        transmissionState = tdma_send_beacon_broadcast(frame_id, cfg.slot_len_ms, cfg.total_slots, 0);
    }

    while (!txDoneFlag) {
        vTaskDelay(pdMS_TO_TICKS(1));
    }
    txDoneFlag = false;

    #if DEBUG_APP == 1
    if (transmissionState == RADIOLIB_ERR_NONE) {
        Serial.println("[MAIN] Transmission beacon finished!");
    } else {
        Serial.print("failed, code: "); Serial.println(transmissionState);
    }
    #endif

    setModeRX();

    for (;;) {
        vTaskDelayUntil(&lastWake, period);

        uint64_t start = esp_timer_get_time();

        frame_id++;
        radioMode = RADIO_TX;
        transmissionState = tdma_send_beacon_broadcast(frame_id, cfg.slot_len_ms, cfg.total_slots, 0);

        #if DEBUG_APP == 1
        Serial.print("[TDMA] TX beacon frame_id="); Serial.println(frame_id);
        #endif

        end_tdma_scheduler_task = esp_timer_get_time();
        log_task(0, end_tdma_scheduler_task - start);

        while (!txDoneFlag) {
            vTaskDelay(pdMS_TO_TICKS(1));
        }
        txDoneFlag = false;

        #if DEBUG_APP == 1
        if (transmissionState == RADIOLIB_ERR_NONE) {
            Serial.println("[MAIN] transmission finished!");
        } else {
            Serial.print("failed, code: "); Serial.println(transmissionState);
        }
        #endif

        setModeRX();
    }
}

// ==================== LORA PROCESS TASK ====================
void lora_process_task(void* pv) {
    (void)pv;
    #if DEBUG_APP == 1
    Serial.println("[MAIN] Lora process task");
    #endif

    uint8_t rx_buffer[PACKET_TOTAL_LEN];
    CipherPacket cpk;
    IMUSample sample;

    for (;;) {
        if (rxDoneFlag) {
            rxDoneFlag = false;
            uint64_t start = esp_timer_get_time();

            // Read full packet: cipher + seq_id
            int state = radio.readData(rx_buffer, PACKET_TOTAL_LEN);
            cpk.timestamp = millis();

            if (state == RADIOLIB_ERR_NONE) {
                // 1) Extract cipher data
                memcpy(cpk.data, rx_buffer, SECURE_DATA_TOTAL_LEN);

                // 2) Extract seq_id (last 2 bytes)
                memcpy(&cpk.seq_id, &rx_buffer[SECURE_DATA_TOTAL_LEN], sizeof(uint16_t));

                // 3) Decrypt to get node_id for PDR tracking
                if (secure_data_decrypt(cpk.data, &sample)) {
                    cpk.node_id = sample.id;

                    // 4) Update PDR tracker
                    pdr_tracker_update(&g_pdr_tracker, cpk.node_id, cpk.seq_id);

                    #if DEBUG_APP == 1
                    Serial.printf("[RX] Node=%d, seq=%u, PDR=%.1f%%\n",
                                  cpk.node_id, cpk.seq_id,
                                  pdr_tracker_get(&g_pdr_tracker, cpk.node_id));
                    #endif

                    // 5) Queue for MQTT
                    if (xQueueSend(gMqttQueue, &cpk, 0) != pdTRUE) {
                        Serial.println("[MAIN] gMqttQueue full, drop packet");
                    }
                } else {
                    Serial.println("[MAIN] Decrypt failed, drop packet");
                }
            } else {
                #if DEBUG_APP == 1
                Serial.print("[MAIN] RX readData error: "); Serial.println(state);
                #endif
            }

            uint64_t end = esp_timer_get_time();
            log_task(1, end - start);
        }

        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

// ==================== MQTT PUSH TASK ====================
void mqtt_push_task(void* pv) {
    (void)pv;
    #if DEBUG_APP == 1
    Serial.println("[MAIN] Pushing MQTT task");
    #endif

    CipherPacket cpk;
    uint32_t lastLoop = 0;

    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xPeriod = pdMS_TO_TICKS(50);

    for (;;) {
        if (!client.connected()) {
            reconnect();
        }

        if (xQueueReceive(gMqttQueue, &cpk, 0) == pdTRUE) {
            uint32_t t0 = micros();

            publishCipherData(cpk);

            uint32_t t1 = micros();
            log_task(2, t1 - t0);
        }

        if (millis() - lastLoop >= 100) {
            lastLoop = millis();
            client.loop();
        }

        vTaskDelayUntil(&xLastWakeTime, xPeriod);
    }
}

// ==================== PDR REPORT TASK ====================
void pdr_report_task(void* pv) {
    (void)pv;

    const TickType_t report_interval = pdMS_TO_TICKS(10000);  // 10 seconds

    for (;;) {
        vTaskDelay(report_interval);
        pdr_tracker_print_all(&g_pdr_tracker);
    }
}

// ==================== TASK LOGGER ====================
void log_task(uint8_t task_id, uint16_t value) {
    if (logger.printed) return;

    switch (task_id) {
        case 0:
            if (logger.idx_tdma < LOG_SIZE)
                logger.tdma[logger.idx_tdma++] = value;
            break;
        case 1:
            if (logger.idx_lora < LOG_SIZE)
                logger.lora_rx[logger.idx_lora++] = value;
            break;
        case 2:
            if (logger.idx_mqtt < LOG_SIZE)
                logger.mqtt[logger.idx_mqtt++] = value;
            break;
    }

    if (logger.idx_tdma == LOG_SIZE &&
        logger.idx_lora == LOG_SIZE &&
        logger.idx_mqtt == LOG_SIZE) {
        dump_logs();
        logger.printed = true;
    }
}

void dump_logs() {
    Serial.println("TDMA_LOG_START");
    for (int i = 0; i < LOG_SIZE; i++)
        Serial.println(logger.tdma[i]);
    Serial.println("TDMA_LOG_END");

    Serial.println("LORA_RX_LOG_START");
    for (int i = 0; i < LOG_SIZE; i++)
        Serial.println(logger.lora_rx[i]);
    Serial.println("LORA_RX_LOG_END");

    Serial.println("MQTT_LOG_START");
    for (int i = 0; i < LOG_SIZE; i++)
        Serial.println(logger.mqtt[i]);
    Serial.println("MQTT_LOG_END");
}
