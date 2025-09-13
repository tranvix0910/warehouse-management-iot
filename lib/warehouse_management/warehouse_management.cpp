#include "warehouse_management.h"
#include "wifi_connect.h"
#include "firebase.h"

void warehouseManagementInit() {
    Serial.begin(115200);
    wifiConnect();
    firebaseInit();
    Serial.println("Warehouse management initialized");
}

void warehouseManagementLoop() {
    appLoop();
}