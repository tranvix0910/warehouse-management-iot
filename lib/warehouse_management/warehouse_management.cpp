#include "warehouse_management.h"
#include "wifi_connect.h"
#include "firebase.h"
#include "temp_humi.h"
#include "rc522.h"

void warehouseManagementInit() {
    Serial.begin(115200);
    delay(1000); // Wait for serial to initialize
    
    if (wifiConnect()) {
        delay(2000); // Wait for WiFi to stabilize
        tempHumiInit(); // Initialize DHT11 sensor
        rc522Init(); // Initialize RC522 reader
        firebaseInit();
        Serial.println("Warehouse management initialized");
    } else {
        Serial.println("Failed to connect to WiFi. Retrying in 5 seconds...");
        delay(5000);
        // Retry WiFi connection
        if (wifiConnect()) {
            delay(2000);
            tempHumiInit(); // Initialize DHT11 sensor
            rc522Init(); // Initialize RC522 reader
            firebaseInit();
            Serial.println("Warehouse management initialized on retry");
        } else {
            Serial.println("WiFi connection failed. Please check credentials.");
        }
    }
}

void warehouseManagementLoop() {
    appLoop();
    delay(5);
}