#include "main.h"
#include <math.h>

#define LOG_SIZE 1000
#define PACKET_TOTAL_LEN (SECURE_DATA_TOTAL_LEN + sizeof(uint16_t))

// ==================== LATENCY STATISTICS ====================
typedef struct {
    uint32_t count;
    float    min_us;
    float    max_us;
    float    sum;
    float    sum_sq;
    float    last_value;
    float    jitter_sum;
    uint32_t jitter_count;
} LatencyStats;

typedef struct {
    LatencyStats lora_rx;
    LatencyStats mqtt_publish;
    LatencyStats end_to_end;
    LatencyStats loop_time;
    LatencyStats queue_wait;
} SystemLatencyStats;

SystemLatencyStats g_latency_stats;

void latency_stats_init(LatencyStats* stats) {
    stats->count = 0;
    stats->min_us = 999999999.0f;
    stats->max_us = 0.0f;
    stats->sum = 0.0f;
    stats->sum_sq = 0.0f;
    stats->last_value = 0.0f;
    stats->jitter_sum = 0.0f;
    stats->jitter_count = 0;
}

void latency_stats_update(LatencyStats* stats, float value_us) {
    stats->count++;
    if (value_us < stats->min_us) stats->min_us = value_us;
    if (value_us > stats->max_us) stats->max_us = value_us;
    stats->sum += value_us;
    stats->sum_sq += (value_us * value_us);
    if (stats->count > 1) {
        stats->jitter_sum += fabs(value_us - stats->last_value);
        stats->jitter_count++;
    }
    stats->last_value = value_us;
}

float latency_stats_mean(LatencyStats* stats) {
    if (stats->count == 0) return 0.0f;
    return stats->sum / stats->count;
}

float latency_stats_variance(LatencyStats* stats) {
    if (stats->count < 2) return 0.0f;
    float mean = latency_stats_mean(stats);
    return (stats->sum_sq / stats->count) - (mean * mean);
}

float latency_stats_std(LatencyStats* stats) {
    float var = latency_stats_variance(stats);
    return (var > 0) ? sqrtf(var) : 0.0f;
}

float latency_stats_jitter(LatencyStats* stats) {
    if (stats->jitter_count == 0) return 0.0f;
    return stats->jitter_sum / stats->jitter_count;
}

void system_latency_init() {
    latency_stats_init(&g_latency_stats.lora_rx);
    latency_stats_init(&g_latency_stats.mqtt_publish);
    latency_stats_init(&g_latency_stats.end_to_end);
    latency_stats_init(&g_latency_stats.loop_time);
    latency_stats_init(&g_latency_stats.queue_wait);
}

// ==================== PDR TRACKER ====================
typedef struct {
    uint16_t last_seq_id;
    uint32_t packets_received;
    uint32_t packets_lost;
    bool     initialized;
} NodePDR;

NodePDR g_pdr_nodes[MAX_NODES];

void pdr_tracker_init() {
    for (int i = 0; i < MAX_NODES; i++) {
        g_pdr_nodes[i].last_seq_id = 0;
        g_pdr_nodes[i].packets_received = 0;
        g_pdr_nodes[i].packets_lost = 0;
        g_pdr_nodes[i].initialized = false;
    }
}

void pdr_tracker_update(uint8_t node_id, uint16_t seq_id) {
    if (node_id >= MAX_NODES) return;

    NodePDR* node = &g_pdr_nodes[node_id];

    if (!node->initialized) {
        node->initialized = true;
        node->last_seq_id = seq_id;
        node->packets_received = 1;
        node->packets_lost = 0;
        return;
    }

    uint16_t diff = seq_id - node->last_seq_id;
    if (diff == 0) return;
    if (diff > 30000) {
        node->last_seq_id = seq_id;
        return;
    }
    if (diff < 1000) {
        node->packets_lost += (diff - 1);
        node->packets_received++;
        node->last_seq_id = seq_id;
    }
}

float pdr_tracker_get(uint8_t node_id) {
    if (node_id >= MAX_NODES) return 0.0f;
    NodePDR* node = &g_pdr_nodes[node_id];
    if (!node->initialized) return 0.0f;
    uint32_t total = node->packets_received + node->packets_lost;
    if (total == 0) return 0.0f;
    return (float)node->packets_received / (float)total * 100.0f;
}

// ==================== THROUGHPUT TRACKER ====================
typedef struct {
    uint32_t total_packets_rx;
    uint32_t total_bytes_rx;
    uint32_t total_packets_mqtt;
    uint32_t start_time_ms;
} ThroughputTracker;

ThroughputTracker g_throughput;

void throughput_init() {
    g_throughput.total_packets_rx = 0;
    g_throughput.total_bytes_rx = 0;
    g_throughput.total_packets_mqtt = 0;
    g_throughput.start_time_ms = millis();
}

void throughput_rx_update(uint32_t bytes) {
    g_throughput.total_packets_rx++;
    g_throughput.total_bytes_rx += bytes;
}

void throughput_mqtt_update() {
    g_throughput.total_packets_mqtt++;
}

// ==================== QUEUE OVERFLOW TRACKER ====================
typedef struct {
    uint32_t total_enqueue_attempts;
    uint32_t overflow_count;
    uint32_t current_queue_size;
    uint32_t max_queue_size;
} QueueStats;

QueueStats g_queue_stats;

void queue_stats_init() {
    g_queue_stats.total_enqueue_attempts = 0;
    g_queue_stats.overflow_count = 0;
    g_queue_stats.current_queue_size = 0;
    g_queue_stats.max_queue_size = 0;
}

// ==================== GLOBAL VARIABLES ====================
SX1278 radio = new Module(ss, dio0, rst);
WiFiClient espClient;
PubSubClient client(espClient);

typedef struct {
    uint8_t  data[SECURE_DATA_TOTAL_LEN];
    uint16_t seq_id;
    uint8_t  node_id;
    uint32_t timestamp;
    uint32_t rx_done_time_us;
    uint32_t enqueue_time_us;
} CipherPacketExt;

#define QUEUE_SIZE 128
CipherPacketExt g_packet_queue[QUEUE_SIZE];
volatile uint8_t g_queue_head = 0;
volatile uint8_t g_queue_tail = 0;

volatile bool rxDoneFlag = false;

uint32_t lastPdrReport = 0;
uint32_t lastMqttLoop = 0;
uint32_t lastStatsReport = 0;

// ==================== ISR ====================
void IRAM_ATTR dio0_isr() {
    rxDoneFlag = true;
}

// ==================== QUEUE FUNCTIONS ====================
uint8_t queue_size() {
    return (g_queue_head - g_queue_tail + QUEUE_SIZE) % QUEUE_SIZE;
}

bool queue_push(CipherPacketExt* pkt) {
    g_queue_stats.total_enqueue_attempts++;

    uint8_t next_head = (g_queue_head + 1) % QUEUE_SIZE;
    if (next_head == g_queue_tail) {
        g_queue_stats.overflow_count++;
        return false;
    }

    pkt->enqueue_time_us = micros();
    memcpy(&g_packet_queue[g_queue_head], pkt, sizeof(CipherPacketExt));
    g_queue_head = next_head;

    // Update queue size stats
    g_queue_stats.current_queue_size = queue_size();
    if (g_queue_stats.current_queue_size > g_queue_stats.max_queue_size) {
        g_queue_stats.max_queue_size = g_queue_stats.current_queue_size;
    }

    return true;
}

bool queue_pop(CipherPacketExt* pkt) {
    if (g_queue_head == g_queue_tail) {
        return false;
    }
    memcpy(pkt, &g_packet_queue[g_queue_tail], sizeof(CipherPacketExt));
    g_queue_tail = (g_queue_tail + 1) % QUEUE_SIZE;
    g_queue_stats.current_queue_size = queue_size();
    return true;
}

// ==================== COMPREHENSIVE REPORT ====================
void print_full_statistics_report() {
    uint32_t elapsed_ms = millis() - g_throughput.start_time_ms;
    float elapsed_sec = elapsed_ms / 1000.0f;

    Serial.println("\n");
    Serial.println("╔══════════════════════════════════════════════════════════════╗");
    Serial.println("║            COMPREHENSIVE STATISTICS REPORT                   ║");
    Serial.println("╠══════════════════════════════════════════════════════════════╣");
    Serial.printf("║  Test Duration: %.2f seconds                                 \n", elapsed_sec);
    Serial.println("╠══════════════════════════════════════════════════════════════╣");

    // ===== 1. PDR (Packet Delivery Ratio) =====
    Serial.println("║                                                              ║");
    Serial.println("║  [1] PDR - PACKET DELIVERY RATIO                             ║");
    Serial.println("║  ─────────────────────────────────────────────────────────── ║");

    uint32_t total_rx = 0, total_lost = 0;
    for (int i = 0; i < MAX_NODES; i++) {
        NodePDR* node = &g_pdr_nodes[i];
        if (node->initialized) {
            uint32_t total = node->packets_received + node->packets_lost;
            float pdr = (total > 0) ? ((float)node->packets_received / total * 100.0f) : 0.0f;
            Serial.printf("║  Node %d: RX=%lu, Lost=%lu, PDR=%.2f%%\n",
                          i, node->packets_received, node->packets_lost, pdr);
            total_rx += node->packets_received;
            total_lost += node->packets_lost;
        }
    }

    uint32_t total_expected = total_rx + total_lost;
    float overall_pdr = (total_expected > 0) ? ((float)total_rx / total_expected * 100.0f) : 0.0f;
    Serial.println("║  ─────────────────────────────────────────────────────────── ║");
    Serial.printf("║  OVERALL: RX=%lu, Lost=%lu, PDR=%.2f%%\n", total_rx, total_lost, overall_pdr);

    // ===== 2. LATENCY =====
    Serial.println("║                                                              ║");
    Serial.println("║  [2] LATENCY STATISTICS (microseconds)                       ║");
    Serial.println("║  ─────────────────────────────────────────────────────────── ║");

    // LoRa RX Latency
    LatencyStats* lora = &g_latency_stats.lora_rx;
    Serial.println("║  >> LoRa RX + Decrypt:");
    Serial.printf("║     Count: %lu | Min: %.0f | Max: %.0f | Mean: %.0f\n",
                  lora->count, lora->min_us, lora->max_us, latency_stats_mean(lora));
    Serial.printf("║     Std: %.2f | Jitter: %.2f\n",
                  latency_stats_std(lora), latency_stats_jitter(lora));

    // MQTT Publish Latency
    LatencyStats* mqtt = &g_latency_stats.mqtt_publish;
    Serial.println("║  >> MQTT Publish:");
    Serial.printf("║     Count: %lu | Min: %.0f | Max: %.0f | Mean: %.0f\n",
                  mqtt->count, mqtt->min_us, mqtt->max_us, latency_stats_mean(mqtt));
    Serial.printf("║     Std: %.2f | Jitter: %.2f\n",
                  latency_stats_std(mqtt), latency_stats_jitter(mqtt));

    // End-to-End Latency
    LatencyStats* e2e = &g_latency_stats.end_to_end;
    Serial.println("║  >> End-to-End (RX to MQTT done):");
    Serial.printf("║     Count: %lu | Min: %.0f | Max: %.0f | Mean: %.0f\n",
                  e2e->count, e2e->min_us, e2e->max_us, latency_stats_mean(e2e));
    Serial.printf("║     Std: %.2f | Jitter: %.2f\n",
                  latency_stats_std(e2e), latency_stats_jitter(e2e));

    // Queue Wait Time
    LatencyStats* qwait = &g_latency_stats.queue_wait;
    Serial.println("║  >> Queue Wait Time:");
    Serial.printf("║     Count: %lu | Min: %.0f | Max: %.0f | Mean: %.0f\n",
                  qwait->count, qwait->min_us, qwait->max_us, latency_stats_mean(qwait));
    Serial.printf("║     Std: %.2f | Jitter: %.2f\n",
                  latency_stats_std(qwait), latency_stats_jitter(qwait));

    // ===== 3. THROUGHPUT =====
    Serial.println("║                                                              ║");
    Serial.println("║  [3] THROUGHPUT                                              ║");
    Serial.println("║  ─────────────────────────────────────────────────────────── ║");

    float rx_pps = (elapsed_sec > 0) ? (g_throughput.total_packets_rx / elapsed_sec) : 0;
    float mqtt_pps = (elapsed_sec > 0) ? (g_throughput.total_packets_mqtt / elapsed_sec) : 0;
    float rx_bps = (elapsed_sec > 0) ? ((g_throughput.total_bytes_rx * 8) / elapsed_sec) : 0;

    Serial.printf("║  Total Packets RX     : %lu\n", g_throughput.total_packets_rx);
    Serial.printf("║  Total Packets MQTT   : %lu\n", g_throughput.total_packets_mqtt);
    Serial.printf("║  Total Bytes RX       : %lu\n", g_throughput.total_bytes_rx);
    Serial.printf("║  RX Throughput        : %.2f packets/sec\n", rx_pps);
    Serial.printf("║  MQTT Throughput      : %.2f packets/sec\n", mqtt_pps);
    Serial.printf("║  RX Bit Rate          : %.2f bps (%.2f kbps)\n", rx_bps, rx_bps / 1000.0f);

    // ===== 4. LOOP TIME =====
    Serial.println("║                                                              ║");
    Serial.println("║  [4] LOOP TIME (microseconds)                                ║");
    Serial.println("║  ─────────────────────────────────────────────────────────── ║");

    LatencyStats* loop_t = &g_latency_stats.loop_time;
    Serial.printf("║  Total Loops  : %lu\n", loop_t->count);
    Serial.printf("║  Min          : %.0f us\n", loop_t->min_us);
    Serial.printf("║  Max          : %.0f us\n", loop_t->max_us);
    Serial.printf("║  Mean         : %.2f us\n", latency_stats_mean(loop_t));
    Serial.printf("║  Std Dev      : %.2f us\n", latency_stats_std(loop_t));
    Serial.printf("║  Jitter       : %.2f us\n", latency_stats_jitter(loop_t));

    float loop_freq = (latency_stats_mean(loop_t) > 0) ? (1000000.0f / latency_stats_mean(loop_t)) : 0;
    Serial.printf("║  Loop Freq    : %.2f Hz\n", loop_freq);

    // ===== 5. QUEUE OVERFLOW =====
    Serial.println("║                                                              ║");
    Serial.println("║  [5] QUEUE OVERFLOW STATISTICS                               ║");
    Serial.println("║  ─────────────────────────────────────────────────────────── ║");

    float overflow_rate = (g_queue_stats.total_enqueue_attempts > 0)
        ? ((float)g_queue_stats.overflow_count / g_queue_stats.total_enqueue_attempts * 100.0f)
        : 0.0f;

    Serial.printf("║  Queue Capacity       : %d\n", QUEUE_SIZE);
    Serial.printf("║  Total Enqueue Tries  : %lu\n", g_queue_stats.total_enqueue_attempts);
    Serial.printf("║  Overflow Count       : %lu\n", g_queue_stats.overflow_count);
    Serial.printf("║  Overflow Rate        : %.4f%%\n", overflow_rate);
    Serial.printf("║  Current Queue Size   : %lu\n", g_queue_stats.current_queue_size);
    Serial.printf("║  Max Queue Size       : %lu\n", g_queue_stats.max_queue_size);
    Serial.printf("║  Queue Utilization    : %.2f%%\n",
                  (float)g_queue_stats.max_queue_size / QUEUE_SIZE * 100.0f);

    Serial.println("║                                                              ║");
    Serial.println("╚══════════════════════════════════════════════════════════════╝");
    Serial.println("\n");
}

// ==================== TASK LOGGER (for raw data export) ====================
typedef struct {
    uint16_t lora_rx[LOG_SIZE];
    uint16_t mqtt[LOG_SIZE];
    uint16_t loop_time[LOG_SIZE];
    uint16_t end_to_end[LOG_SIZE];

    uint16_t idx_lora;
    uint16_t idx_mqtt;
    uint16_t idx_loop;
    uint16_t idx_e2e;

    bool printed;
} TaskLogger;

TaskLogger logger = {0};

void log_task(uint8_t task_id, uint16_t value) {
    if (logger.printed) return;

    switch (task_id) {
        case 0:
            if (logger.idx_lora < LOG_SIZE) logger.lora_rx[logger.idx_lora++] = value;
            break;
        case 1:
            if (logger.idx_mqtt < LOG_SIZE) logger.mqtt[logger.idx_mqtt++] = value;
            break;
        case 2:
            if (logger.idx_loop < LOG_SIZE) logger.loop_time[logger.idx_loop++] = value;
            break;
        case 3:
            if (logger.idx_e2e < LOG_SIZE) logger.end_to_end[logger.idx_e2e++] = value;
            break;
    }
}

void dump_logs() {
    Serial.println("\n===== RAW DATA LOGS FOR ANALYSIS =====\n");

    Serial.println("LORA_RX_LOG_START");
    for (int i = 0; i < logger.idx_lora; i++) Serial.println(logger.lora_rx[i]);
    Serial.println("LORA_RX_LOG_END");

    Serial.println("MQTT_LOG_START");
    for (int i = 0; i < logger.idx_mqtt; i++) Serial.println(logger.mqtt[i]);
    Serial.println("MQTT_LOG_END");

    Serial.println("LOOP_TIME_LOG_START");
    for (int i = 0; i < logger.idx_loop; i++) Serial.println(logger.loop_time[i]);
    Serial.println("LOOP_TIME_LOG_END");

    Serial.println("END_TO_END_LOG_START");
    for (int i = 0; i < logger.idx_e2e; i++) Serial.println(logger.end_to_end[i]);
    Serial.println("END_TO_END_LOG_END");

    logger.printed = true;
}

// ==================== SETUP ====================
void setup() {
    Serial.begin(115200);
    Serial.println("\n");
    Serial.println("╔══════════════════════════════════════════════════════════════╗");
    Serial.println("║     NON-RTOS, NON-TDMA RECEIVER - BENCHMARK MODE             ║");
    Serial.println("╚══════════════════════════════════════════════════════════════╝");
    Serial.println();

    // Init all trackers
    pdr_tracker_init();
    system_latency_init();
    throughput_init();
    queue_stats_init();

    // Init LoRa
    InitLora();
    radio.setPacketReceivedAction(dio0_isr);

    // Init WiFi & MQTT
    Init_Connection();

    // Start continuous receive mode
    int state = radio.startReceive();
    if (state == RADIOLIB_ERR_NONE) {
        Serial.println("[LORA] Continuous RX mode started");
    } else {
        Serial.printf("[LORA] startReceive failed, code: %d\n", state);
    }

    Serial.println("[MAIN] Setup complete - Polling mode active\n");
}

// ==================== MAIN LOOP ====================
void loop() {
    uint32_t loop_start = micros();

    // ========== 1. LORA RX HANDLING ==========
    if (rxDoneFlag) {
        rxDoneFlag = false;
        uint32_t rx_start = micros();

        uint8_t rx_buffer[PACKET_TOTAL_LEN];
        CipherPacketExt cpk;
        IMUSample sample;

        int state = radio.readData(rx_buffer, PACKET_TOTAL_LEN);
        cpk.timestamp = millis();

        if (state == RADIOLIB_ERR_NONE) {
            memcpy(cpk.data, rx_buffer, SECURE_DATA_TOTAL_LEN);
            memcpy(&cpk.seq_id, &rx_buffer[SECURE_DATA_TOTAL_LEN], sizeof(uint16_t));

            if (secure_data_decrypt(cpk.data, sample)) {
                if (sample.id < MAX_NODES) {
                    cpk.node_id = sample.id;
                    cpk.rx_done_time_us = micros();

                    pdr_tracker_update(cpk.node_id, cpk.seq_id);
                    throughput_rx_update(PACKET_TOTAL_LEN);

                    if (!queue_push(&cpk)) {
                        Serial.println("[WARN] Queue overflow!");
                    }

                    #if DEBUG_APP == 1
                    Serial.printf("[RX] Node=%d, seq=%u\n", cpk.node_id, cpk.seq_id);
                    #endif
                }
            }
        }

        radio.startReceive();

        uint32_t rx_end = micros();
        uint32_t rx_latency = rx_end - rx_start;
        latency_stats_update(&g_latency_stats.lora_rx, (float)rx_latency);
        log_task(0, rx_latency);
    }

    // ========== 2. MQTT HANDLING ==========
    if (!client.connected()) {
        reconnect();
    }

    CipherPacketExt cpk;
    if (queue_pop(&cpk)) {
        uint32_t mqtt_start = micros();

        // Queue wait time
        uint32_t queue_wait = mqtt_start - cpk.enqueue_time_us;
        latency_stats_update(&g_latency_stats.queue_wait, (float)queue_wait);

        // Publish
        CipherPacket cpk_mqtt;
        memcpy(cpk_mqtt.data, cpk.data, SECURE_DATA_TOTAL_LEN);
        cpk_mqtt.seq_id = cpk.seq_id;
        cpk_mqtt.node_id = cpk.node_id;
        cpk_mqtt.timestamp = cpk.timestamp;

        publishCipherData(cpk_mqtt);
        throughput_mqtt_update();

        uint32_t mqtt_end = micros();
        uint32_t mqtt_latency = mqtt_end - mqtt_start;
        latency_stats_update(&g_latency_stats.mqtt_publish, (float)mqtt_latency);
        log_task(1, mqtt_latency);

        // End-to-end
        uint32_t e2e_latency = mqtt_end - cpk.rx_done_time_us;
        latency_stats_update(&g_latency_stats.end_to_end, (float)e2e_latency);
        log_task(3, e2e_latency);
    }

    // MQTT keep-alive
    if (millis() - lastMqttLoop >= 100) {
        lastMqttLoop = millis();
        client.loop();
    }

    // ========== 3. PERIODIC REPORTS ==========

    // Full report every 30 seconds
    if (millis() - lastStatsReport >= 30000) {
        lastStatsReport = millis();
        print_full_statistics_report();
    }

    // ========== 4. DUMP RAW LOGS (once when buffers full) ==========
    if (!logger.printed &&
        logger.idx_lora >= LOG_SIZE &&
        logger.idx_mqtt >= LOG_SIZE) {
        dump_logs();
    }

    // ========== 5. LOG LOOP TIME ==========
    uint32_t loop_end = micros();
    uint32_t loop_latency = loop_end - loop_start;
    latency_stats_update(&g_latency_stats.loop_time, (float)loop_latency);
    log_task(2, loop_latency);

    delay(1);
}
