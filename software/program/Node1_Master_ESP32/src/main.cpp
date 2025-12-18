#include "main.h"

QueueHandle_t gImuQueue;
QueueHandle_t gPoseQueue;
TaskHandle_t  hSensor, hAHRS, hTel;

typedef struct {
  float roll, pitch, yaw;     // degrees
  float ax_n, ay_n, az_n;     // normalized accel (gravity -> -Z)
  float gx, gy, gz;     // normalized accel (gravity -> -Z)
  float stamp_ms;
} Orientation;

void setup() {
  Serial.begin(115200);

  InitLora();

  /* Init_Connection(); */
  
  Init_MPU6050();

  /* Create queues */
  gImuQueue  = xQueueCreate(/*length=*/128, sizeof(IMUSample));
  gPoseQueue = xQueueCreate(/*length=*/128, sizeof(Orientation));

  /* reate tasks, pin to cores (ESP32: core 0 & 1) */
  xTaskCreatePinnedToCore(sensor_task,        /* Name of task function  */
                          "SensorTask",       /* Name task */
                          4096,               /* usStackDepth */
                          (void*)gImuQueue, 
                          3,                  /* Priority */
                          &hSensor,           /* Task hander */
                          0);                 /* Core ID */
  xTaskCreatePinnedToCore(ahrs_task,    
                          "AHRSTask",     
                          4096, 
                          nullptr,          
                          2, 
                          &hAHRS,     
                          1);
  xTaskCreatePinnedToCore(telemetry_task,
                          "TelemetryTask",
                          4096, 
                          nullptr,          
                          1, 
                          &hTel,    
                          1);

  Serial.println("RTOS pipeline started: Sensor->AHRS->Telemetry");

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
void ahrs_task(void* pv) {
  (void)pv;
  IMUSample s;
  Orientation o;

  for (;;) {
    if (xQueueReceive(gImuQueue, &s, portMAX_DELAY) == pdTRUE) {
      MahonyAHRSGetIMU(&s);
      
      /* Roll Pitch can be caculated when perform draw in python */
      /* ahrs_get_euler(&o.roll, &o.pitch, &o.yaw); */

      // wrap yaw về (-180,180]
      if (o.yaw > 180.0f)  o.yaw -= 360.0f;
      if (o.yaw <= -180.0f) o.yaw += 360.0f;

      o.ax_n = s.ax;
      o.ay_n = s.ay;
      o.az_n = s.az;
      o.gx = s.gx;
      o.gy = s.gy;
      o.gz = s.gz;
    
      o.stamp_ms = s.dt;
      xQueueSend(gPoseQueue, &o, 0);
    }
  }
}

void telemetry_task(void* pv) {
  (void)pv;
  Orientation o;
  static float yaw_f = 0.0f;
  const float alphaYaw = 0.15f;

  uint32_t lastPrint = 0;
  for (;;) {
    if (xQueueReceive(gPoseQueue, &o, portMAX_DELAY) == pdTRUE) {
      // low-pass yaw hiển thị
      yaw_f = yaw_f + alphaYaw * (o.yaw - yaw_f);

      if (millis() - lastPrint >= 50) {
        lastPrint = millis();
        /* Serial.print("Roll: ");  Serial.print(o.roll, 2);
        Serial.print("  Pitch: "); Serial.print(o.pitch, 2);
        Serial.print("  Yaw: ");   Serial.print(yaw_f, 2); */
        Serial.print("  ax_n: ");  Serial.print(o.ax_n, 3);
        Serial.print("  ay_n: ");  Serial.print(o.ay_n, 3);
        Serial.print("  az_n: ");  Serial.print(o.az_n, 3);
        Serial.print("  gx: ");  Serial.print(o.gx, 3);
        Serial.print("  gy: ");  Serial.print(o.gy, 3);
        Serial.print("  gz: ");  Serial.print(o.gz, 3);
        Serial.print("  t(ms): "); Serial.println(o.stamp_ms * 1000.0f, 2);
      }
    }
  }
}
/* ================================ */

void read_without_rtos()
{
  IMUSample s;
  if (sensor_read(&s) == 0)
  {
    MahonyAHRSGetIMU(&s);

    // Lấy Euler để hiển thị
    float roll, pitch, yaw;
    ahrs_get_euler(&roll, &pitch, &yaw);

    static uint32_t lastPrint = 0;
    if (millis() - lastPrint >= 50) {
      lastPrint = millis();
      Serial.print("  Roll: ");  Serial.print(roll, 2);
      Serial.print("  Pitch: "); Serial.print(pitch, 2);
      Serial.print("  Yaw: ");   Serial.print(yaw, 2);  
      Serial.print("  ax: ");    Serial.print(s.ax, 3);
      Serial.print("  ay: ");    Serial.print(s.ay, 3);
      Serial.print("  az: ");    Serial.print(s.az, 3);

      Serial.print("  dt(ms): "); Serial.println(s.dt * 1000.0f, 2);
    } 

  }
}
