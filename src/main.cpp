#include <Arduino.h>
#include "warehouse_management.h"
#include "rc522.h"

void setup() {
  Serial.begin(115200);
  warehouseManagementInit();
  // rc522Init();
  
}

void loop() {
  warehouseManagementLoop();
  // String uid;
  // if (rc522ReadUID(uid)) {
  //   Serial.println(uid);
  // }
  // delay(1000);
}
