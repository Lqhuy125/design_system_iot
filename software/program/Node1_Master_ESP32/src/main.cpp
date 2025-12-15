#include "main.h"

void setup() {
  Serial.begin(115200);

  /* InitLora();

  Init_Connection(); */
  
  Init_MPU6050();
}

void loop() {
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
      Serial.print("Roll: ");  Serial.print(roll, 2);
      Serial.print("  Pitch: "); Serial.print(pitch, 2);
      Serial.print("  Yaw: ");   Serial.print(yaw, 2);  
      Serial.print("  ax: ");    Serial.print(s.ax, 3);
      Serial.print("  ay: ");    Serial.print(s.ay, 3);
      Serial.print("  az: ");    Serial.print(s.az, 3);

      Serial.print("  dt(ms): "); Serial.println(s.dt * 1000.0f, 2);
    }

  }
  /* RecieveData(); */

}