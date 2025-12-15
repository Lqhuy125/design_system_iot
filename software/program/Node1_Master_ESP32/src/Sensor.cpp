#include "sensor.h"

Adafruit_MPU6050 mpu;

static uint32_t lastMicros = 0;

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
  sensors_event_t a, g, temp;
  mpu.getEvent(&a, &g, &temp);
  /* Print out the values */

  out->ax = a.acceleration.x;
  out->ay = a.acceleration.y;
  out->az = a.acceleration.z;
  out->gx = a.gyro.x;
  out->gy = a.gyro.y;
  out->gz = a.gyro.z;

  float norm = sqrtf(out->ax*out->ax + out->ay*out->ay + out->az*out->az);
  
  if (norm > 2) //m/s^2 ~ 9.8 and g ~ 1
  {
    out->ax = out->ax / 9.8;
    out->ay = out->ay / 9.8;
    out->az = out->az / 9.8;
  } 
  int32_t now = micros();
  float dt = (now - lastMicros) * 1e-6f;
  lastMicros = now;
  // Clamp dt range to avoid spikes
  if (dt < 0.0005f)  if (dt < 0.0005f) dt = 0.0005f;
  if (dt > 0.02f)   dt = 0.02f;
  out->dt = dt;

  return 0;

}