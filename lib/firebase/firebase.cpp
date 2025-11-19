#include "firebase.h"
#include "temp_humi.h"
#include "rc522.h"
#include "qr_scanner.h"
#include <WiFi.h>

// Define the global Firebase objects
UserAuth user_auth(WEB_API_KEY, USER_EMAIL, USER_PASS);
FirebaseApp app;
WiFiClientSecure ssl_client;
using AsyncClient = AsyncClientClass;
AsyncClient aClient(ssl_client);
RealtimeDatabase Database;

// Timer variables for sending data
unsigned long lastTempHumiSendTime = 0;
unsigned long lastCheckFlagsReadTime = 0;
const unsigned long tempHumiInterval = 2000; // 2 seconds for temperature and humidity
const unsigned long checkFlagsInterval = 1000; // 1 second for reading check flags from Firebase

// Variables to send to the database
float temperature = 0.0;
float humidity = 0.0;
String rfidUid = "";
String qrData = "";

// Check mode flags
bool check_qr = false;
bool check_rfid = false;
bool checkFlagsReceived = false;

// Function to read temperature and humidity from DHT11 sensor
void readSensorData() {
    // Read temperature first
    temperature = readTemperature();
    
    // Small delay before reading humidity (DHT11 needs time between readings)
    delay(100);
    
    // Read humidity
    humidity = readHumidity();
    
    // If sensor reading fails, use fallback values
    if (temperature == -999.0) {
        temperature = 25.0; // Default temperature
        Serial.println("Using fallback temperature value");
    }
    
    if (humidity == -999.0) {
        humidity = 50.0; // Default humidity
        Serial.println("Using fallback humidity value");
    }
}

void processData(AsyncResult &aResult) {
    if (!aResult.isResult())
        return;
  
    if (aResult.isEvent())
        Firebase.printf("Event task: %s, msg: %s, code: %d\n", aResult.uid().c_str(), aResult.eventLog().message().c_str(), aResult.eventLog().code());
  
    if (aResult.isDebug()) {
        // Only print important debug messages
        String msg = aResult.debug();
        if (msg.indexOf("Connecting") >= 0 || msg.indexOf("Terminating") >= 0) {
            Firebase.printf("Debug task: %s, msg: %s\n", aResult.uid().c_str(), aResult.debug().c_str());
        }
    }
  
    if (aResult.isError()) {
        // Only print errors that are not cancellation (code -118 is cancellation)
        if (aResult.error().code() != -118) {
            Firebase.printf("Error task: %s, msg: %s, code: %d\n", aResult.uid().c_str(), aResult.error().message().c_str(), aResult.error().code());
        }
    }
  
    if (aResult.available())
        Firebase.printf("task: %s, payload: %s\n", aResult.uid().c_str(), aResult.c_str());
}

// Read check flags (check_qr and check_rfid) from Firebase
void readCheckFlagsFromFirebase() {
    bool newCheckQr = Database.get<bool>(aClient, "/sensors/check_qr");
    bool newCheckRfid = Database.get<bool>(aClient, "/sensors/check_rfid");
    
    if (newCheckQr != check_qr) {
        check_qr = newCheckQr;
        Serial.printf("check_qr updated: %s\n", check_qr ? "true" : "false");
    }
    
    if (newCheckRfid != check_rfid) {
        check_rfid = newCheckRfid;
        Serial.printf("check_rfid updated: %s\n", check_rfid ? "true" : "false");
    }
    
    checkFlagsReceived = true;
}

void firebaseInit() {
    Serial.println("Initializing Firebase...");
    
    // Ensure WiFi is connected and stable
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("Error: WiFi not connected, cannot initialize Firebase");
        return;
    }
    
    // Wait a bit more to ensure WiFi stack is fully ready
    delay(500);

    // Configure SSL client first
    configureSSLClient();
    Serial.println("SSL client configured");
    
    // Additional delay to ensure everything is ready
    delay(200);

    // Initialize Firebase app
    initializeApp(aClient, app, getAuth(user_auth), processData, "🔐 authTask");
    app.getApp<RealtimeDatabase>(Database);
    Database.url(DATABASE_URL);
    
    Serial.println("Firebase initialized successfully");
}

void appLoop() {
    app.loop();

    // Check WiFi connection first
    if (WiFi.status() != WL_CONNECTED) {
        static unsigned long lastWiFiCheck = 0;
        if (millis() - lastWiFiCheck > 5000) { // Log every 5 seconds
            Serial.println("WiFi disconnected! Attempting to reconnect...");
            WiFi.reconnect();
            lastWiFiCheck = millis();
        }
        return;
    }

    if (app.ready()){ 
        unsigned long currentTime = millis();
        
        // Read check flags from Firebase every second
        if (currentTime - lastCheckFlagsReadTime >= checkFlagsInterval) {
            lastCheckFlagsReadTime = currentTime;
            readCheckFlagsFromFirebase();
        }
        
        if (check_rfid) {
            // RFID priority mode: pause other sensors and scan RFID continuously
            String uid;
            if (rc522ReadUID(uid)) {
                if (!uid.isEmpty()) {
                    rfidUid = uid;
                    rfidUid.toUpperCase();
                    Serial.printf("RFID detected (priority mode): %s\n", rfidUid.c_str());
                    
                    object_t jsonRfid;
                    object_t jsonCheck;
                    object_t payload;
                    payload.initObject();
                    JsonWriter writer;
                    writer.create(jsonRfid, "rfid_uid", rfidUid.c_str());
                    writer.create(jsonCheck, "check_rfid", false);
                    writer.join(payload, 2, jsonRfid, jsonCheck);
                    
                    Database.update(aClient, "/sensors", payload, processData, "RTDB_Send_RFID_CheckRFID");
                    
                    check_rfid = false;
                    Serial.println("RFID sent. check_rfid set to false. Resuming normal mode.");
                }
            }
            // Small delay for RFID scanning stability
            delay(50);
        } else if (check_qr) {
            // QR priority mode: pause other sensors and scan QR continuously
            String scannedQr;
            if (scanQRCode(scannedQr)) {
                if (!scannedQr.isEmpty()) {
                    qrData = scannedQr;
                    Serial.printf("QR Code detected (priority mode): %s\n", qrData.c_str());
                    
                    // Create payload with both qr_data and check_qr
                    object_t jsonQr;
                    object_t jsonCheckQr;
                    object_t payload;
                    payload.initObject();
                    JsonWriter writer;
                    writer.create(jsonQr, "qr_data", qrData.c_str());
                    writer.create(jsonCheckQr, "check_qr", false);
                    writer.join(payload, 2, jsonQr, jsonCheckQr);
                    
                    bool qrSent = Database.update(aClient, "/sensors", payload);
                    if (qrSent) {
                        Serial.printf("Sending QR to Firebase - QR: %s, check_qr: false\n", qrData.c_str());
                        check_qr = false;
                        Serial.println("QR scan completed. check_qr set to false. Resuming normal mode.");
                    } else {
                        Serial.printf("Failed to send QR data: %s (code: %d)\n",
                                      aClient.lastError().message().c_str(),
                                      aClient.lastError().code());
                    }
                }
            }
            delay(100);
        } else {
            // Normal mode: only DHT11 readings every 2 seconds
            if (currentTime - lastTempHumiSendTime >= tempHumiInterval) {
                lastTempHumiSendTime = currentTime;
                
                readSensorData();
                sendTempHumiToFirebase(temperature, humidity);
                
                Serial.printf("Sending temp/humi - Temperature: %.1f°C, Humidity: %.1f%%\n", 
                             temperature, humidity);
            }
        }
      } 
}

// Send temperature and humidity data to Firebase
void sendTempHumiToFirebase(float temperature, float humidity) {
    object_t jsonTemp;
    object_t jsonHum;
    object_t payload;
    payload.initObject();
    JsonWriter writer;
    writer.create(jsonTemp, "temperature", temperature);
    writer.create(jsonHum, "humidity", humidity);
    writer.join(payload, 2, jsonTemp, jsonHum);

    Database.update(aClient, "/sensors", payload, processData, "RTDB_Send_TempHumi");
}

// Send RFID and QR data to Firebase
void sendRfidQrToFirebase(String rfidUid, String qrData) {
    // Send RFID UID data to Firebase (send "null" string when empty)
    if (rfidUid.isEmpty()) {
        Database.set<String>(aClient, "/sensors/rfid_uid", "null", processData, "RTDB_Send_RFID_UID");
    } else {
        Database.set<String>(aClient, "/sensors/rfid_uid", rfidUid, processData, "RTDB_Send_RFID_UID");
    }
    
    // Send QR data to Firebase (send "null" string when empty)
    if (qrData.isEmpty()) {
        Database.set<String>(aClient, "/sensors/qr_data", "null", processData, "RTDB_Send_QR_Data");
    } else {
        Database.set<String>(aClient, "/sensors/qr_data", qrData, processData, "RTDB_Send_QR_Data");
    }
}

// Keep the original function for backward compatibility
void sendAllDataToFirebase(float temperature, float humidity, String rfidUid, String qrData) {
    sendTempHumiToFirebase(temperature, humidity);
    sendRfidQrToFirebase(rfidUid, qrData);
}

void configureSSLClient(){
    // Ensure WiFi is connected before configuring SSL
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("Warning: WiFi not connected, SSL configuration may fail");
        return;
    }
    
    // Wait a bit for WiFi stack to be ready
    delay(100);
    
    // Configure SSL client with better settings for stability
    // Note: setInsecure() and timeout settings don't require socket to be open
    ssl_client.setInsecure();
    ssl_client.setHandshakeTimeout(60); // Increase timeout to 60 seconds
    ssl_client.setTimeout(60); // Set overall timeout to 60 seconds
    
    // Don't set NoDelay here - it requires socket to be open
    // Socket will be opened automatically when Firebase connects
}