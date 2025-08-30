/* This is the program to check DEVICE_ID of MPU 
   Step test MPU:
   1. Run program check i2c address 
   2. Run program check return MPU6050_DEVICE_ID: Maybe is 0x68. 0x69, 0x70, ...
   3. After print MPU6050_DEVICE_ID in terminal -> Change value MPU6050_DEVICE_ID in source library
*/
#include <Arduino.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <Wire.h>

Adafruit_MPU6050 mpu;

#define MPU6050_ADDRESS 0x68
#define WHO_AM_I 0x75

void setup() {
  Serial.begin(115200);
  Wire.begin();
  
  Wire.beginTransmission(MPU6050_ADDRESS);
  Wire.write(WHO_AM_I);
  Wire.endTransmission(false);
  Wire.requestFrom(MPU6050_ADDRESS, 1, true);
  
  if (Wire.available()) {
    byte c = Wire.read();
    Serial.print("WHO_AM_I = 0x");
    Serial.println(c, HEX);
  } else {
    Serial.println("Không đọc được WHO_AM_I");
  }
}
void loop() {
  // put your main code here, to run repeatedly:
}