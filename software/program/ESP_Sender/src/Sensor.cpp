#include "Sensor.h"
#include "main.h"

// Add external declaration for log_task
extern void log_task(uint8_t task_id, uint16_t value);

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
  out->gx = g.gyro.x;
  out->gy = g.gyro.y;
  out->gz = g.gyro.z;

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

void sensor_task(void* pv) {
  QueueHandle_t q = (QueueHandle_t)pv;
  TickType_t lastWake = xTaskGetTickCount();
  const TickType_t periodTicks = pdMS_TO_TICKS(10);  // Example: 100Hz

  for (;;) {
    IMUSample s;
    s.id = SLAVE_NODE_ID;

    uint64_t start = esp_timer_get_time();

    if (sensor_read(&s) == 0) {
      xQueueOverwrite(q, &s);
    }

    uint64_t end = esp_timer_get_time();
    log_task(0, (uint16_t)(end - start));  // Log sensor read time

    vTaskDelayUntil(&lastWake, periodTicks);
  }
}