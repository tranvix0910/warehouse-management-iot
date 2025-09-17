#include "wifi_connect.h"

// WiFi credentials
// const char* ssid = "PTIT.HCM_CanBo";
// const char* password = "";
// const char* ssid = "311HHN Lau 1";
// const char* password = "@@1234abcdlau1";
const char* ssid = "Huhu";
const char* password = "hahahahaa";

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
