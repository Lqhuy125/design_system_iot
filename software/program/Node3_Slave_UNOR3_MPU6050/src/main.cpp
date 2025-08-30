#include "main.h"

void setup() {
    Serial.begin(115200);

    InitMPU();

    InitLora();
    
}

void loop() {
  // put your main code here, to run repeatedly:
  readMPU();
  SenData(data);

  delay(100); // gửi liên tục mỗi 100ms
}