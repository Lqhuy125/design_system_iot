
#include "Custom_Lora.h"
#include "main.h"

extern QueueHandle_t gRxQueue, gMqttQueue;

// ==== Tham số xử lý đa node ====
#define MAX_NODES_AGG         MAX_NODES   // dùng hằng của bạn
#define AGG_TICK_MS           20          // chu kỳ xử lý
#define PUBLISH_MIN_SPACING_MS 50         // tối thiểu 50 ms/node trước khi publish lần kế

typedef struct {
  bool       has_data;
  IMUSample  last;
  uint32_t   last_published_ms;
} NodeSlot;

void aggregator_task(void* pv) {
  (void)pv;
  static NodeSlot slots[MAX_NODES_AGG];
  uint32_t rr_idx = 0;
  uint32_t lastPrint = 0;
  for (;;) {
    // 1) Hút nhanh tất cả sample có sẵn trong RxQueue
    IMUSample s;
    while (xQueueReceive(gRxQueue, &s, 0) == pdTRUE) {
      int node = (int)s.id - 1;
      if (node >= 0 && node < MAX_NODES_AGG) {
        slots[node].last = s;
        slots[node].has_data = true;
      }
    }

    // 2) Round-robin các node, publish nếu đủ spacing
    for (int k = 0; k < MAX_NODES_AGG; ++k) {    uint32_t now = millis();
      int node = (rr_idx + k) % MAX_NODES_AGG;
      if (slots[node].has_data) {
        if (now - slots[node].last_published_ms >= PUBLISH_MIN_SPACING_MS) {
          if (xQueueSend(gMqttQueue, &slots[node].last, 0) == pdTRUE) {
            slots[node].last_published_ms = now;
            // Nếu chỉ muốn publish "mẫu mới nhất" rồi clear:
            // slots[node].has_data = false;
          }
        }
      }
    }
    rr_idx = (rr_idx + 1) % MAX_NODES_AGG;

    /* Print to debug */
    /* if (xQueueReceive(gRxQueue, &s, portMAX_DELAY) == pdTRUE) {

      if (millis() - lastPrint >= 50) {
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
    } */

    vTaskDelay(pdMS_TO_TICKS(AGG_TICK_MS));
  }
}
