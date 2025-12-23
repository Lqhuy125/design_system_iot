#include "Sensor.h"

Adafruit_MPU6050 mpu;

static uint32_t lastMicros = 0;

extern SemaphoreHandle_t gI2CMutex;

void Init_MPU6050()
{
    // Try to initialize!
    if (!mpu.begin()) {
        Serial.println("Failed to find MPU6050 chip");
        while (1) {
        delay(10);
        }
    }
    Serial.println("MPU6050 Found!");
}

int sensor_read(IMUSample* out)
{
  // đọc dữ liệu từ mpu
  /* Get new sensor events with the readings */
  xSemaphoreTake(gI2CMutex, portMAX_DELAY);

  sensors_event_t a, g, temp;
  mpu.getEvent(&a, &g, &temp);
  xSemaphoreGive(gI2CMutex);
  /* Print out the values */
  const float MS2_TO_G = 1.0f / 9.80665f;
  
  out->ax = a.acceleration.x * MS2_TO_G;
  out->ay = a.acceleration.y * MS2_TO_G;
  out->az = (a.acceleration.z * MS2_TO_G); //Celebration each micro controller
  out->gx = a.gyro.x;
  out->gy = a.gyro.y;
  out->gz = a.gyro.z;

  int32_t now = micros();
  float dt = (now - lastMicros) * 1e-6f;
  lastMicros = now;
  // Clamp dt range to avoid spikes
  if (dt < 0.0005f) dt = 0.0005f; //~2kHz
  if (dt > 0.02f)   dt = 0.02f;   //50Hz
  out->dt = dt;

  out->t_s = millis() * 1e-3f;

  return 0;
}


// ------------------- FreeRTOS Sensor Task -------------------

void sensor_task(void* pv) 
{
  QueueHandle_t q = (QueueHandle_t)pv;
  const TickType_t periodTicks = pdMS_TO_TICKS(5); //  const TickType_t periodTicks = pdMS_TO_TICKS(5); // ~200 Hz
  TickType_t lastWake = xTaskGetTickCount();

  for (;;) {
    IMUSample s;
    s.id = 1;
    if (sensor_read(&s) == 0) {
      // gửi vào queue (không block quá lâu)
      xQueueSend(q, &s, 0);
    }
    vTaskDelayUntil(&lastWake, periodTicks);
  }
}