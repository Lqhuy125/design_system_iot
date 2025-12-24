#include "main.h"

QueueHandle_t gTransmit;
TaskHandle_t  hSensor, hTransmit;

SemaphoreHandle_t gI2CMutex;
SemaphoreHandle_t gLoraMutex;

void setup() {
  Serial.begin(9600);

  gI2CMutex = xSemaphoreCreateMutex();
  gLoraMutex = xSemaphoreCreateMutex();

  InitLora();

  /* Init_Connection(); */
  
  Init_MPU6050();

  /* Create queues */
  gTransmit = xQueueCreate(/*length=*/2, sizeof(IMUSample));

  /* reate tasks, pin to cores (ESP32: core 0 & 1) */
  xTaskCreatePinnedToCore(sensor_task,        /* Name of task function  */
                          "SensorTask",       /* Name task */
                          4096,               /* usStackDepth */
                          (void*)gTransmit,  /* Queue handler, name Queue to refer */
                          3,                  /* Priority */
                          &hSensor,           /* Task hander */
                          0);                 /* Core ID */
  xTaskCreatePinnedToCore(transmit_task,
                          "TransmitTask",
                          4096, 
                          nullptr,          
                          1, 
                          &hTransmit,    
                          1);

  Serial.println("RTOS pipeline started: Sensor->Transmit");
}

void loop() {
  // RTOS tasks chạy riêng; loop() có thể để trống
  // vTaskDelay(pdMS_TO_TICKS(1000));
  
  /* Run without RTOS*/
  // transmit_without_rtos();

  /* RecieveData(); */
}
/* 
    =========== Task on freeRTOS ============
*/

void transmit_task(void* pv) {
  (void)pv;
  IMUSample s;
  uint32_t lastPrint = 0;
  for (;;) {
    if (xQueueReceive(gTransmit, &s, portMAX_DELAY) == pdTRUE) {

      if (millis() - lastPrint >= 50) {
        lastPrint = millis();

        lora_send_imusample(s);
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
