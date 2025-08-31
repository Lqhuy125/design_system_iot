#include "main.h"



void setup() {
  Serial.begin(115200);

  InitLora();

  Init_Connection();
  
}

void loop() {

  
  RecieveData();

}