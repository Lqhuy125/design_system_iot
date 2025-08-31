#include "main.h"



void setup() {
  Serial.begin(115200);

  InitLora();
}

void loop() {

  RecieveData();

}