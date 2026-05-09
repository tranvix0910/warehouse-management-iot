#include "warehouse_management.h"
#include "wifi_connect.h"
#include "firebase.h"
#include "temp_humi.h"
#include "rc522.h"
#include "pn532.h"
#include "qr_scanner.h"

void warehouseManagementInit() {
    Serial.begin(115200);
    delay(1000); // Wait for serial to initialize
    
    if (wifiConnect()) {
        delay(2000); // Wait for WiFi to stabilize
        tempHumiInit(); // Initialize DHT11 sensor
        rc522Init(); // Shared SPI + RC522 (CS 40)
        pn532Init(); // PN532 on same SPI (CS 39)
        qrScannerInit(); // Initialize QR scanner
        firebaseInit();
        Serial.println("Warehouse management initialized");
    } else {
        Serial.println("Failed to connect to WiFi. Retrying in 5 seconds...");
        delay(5000);
        // Retry WiFi connection
        if (wifiConnect()) {
            delay(2000);
            tempHumiInit(); // Initialize DHT11 sensor
            rc522Init(); // Shared SPI + RC522 (CS 40)
            pn532Init(); // PN532 on same SPI (CS 39)
            qrScannerInit(); // Initialize QR scanner
            firebaseInit();
            Serial.println("Warehouse management initialized on retry");
        } else {
            Serial.println("WiFi connection failed. Please check credentials.");
        }
    }
}

void warehouseManagementLoop() {
    // Run the main app loop (handles Firebase, temperature, humidity, RFID, QR)
    appLoop();
    delay(50); // balance: FirebaseClient needs frequent app.loop(); camera task may need tuning if unstable
}
