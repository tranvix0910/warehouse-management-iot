#include "rc522.h"

// Simple sketch to test RC522-only functionality.

void setup() {
  Serial.begin(115200);
  delay(500);
  rc522Init();
  Serial.println("RC522 test mode ready. Present a tag...");
}

void loop() {
  String uid;
  if (rc522ReadUID(uid)) {
    Serial.print("Tag UID: ");
    Serial.println(uid);
    delay(500); // Debounce to avoid repeated logs
  }
  delay(100);
}
