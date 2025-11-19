#include "wifi_connect.h"


const char* ssid = "AEPTIT_1";
const char* password = "20242024";

// Timeout for WiFi connection (10 seconds)
const unsigned long timeout = 10000;

bool wifiConnect() {
    unsigned long startingTime = millis();
    WiFi.begin(ssid, password);
    Serial.println("Connecting to WiFi...");

    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
        if ((millis() - startingTime) > timeout) {
            Serial.println();
            Serial.println("WiFi connection timeout!");
            return false;
        }
    }
    Serial.println();
    Serial.println("WiFi connected!");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    return true;
}
