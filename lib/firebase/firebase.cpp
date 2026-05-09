#include "firebase.h"
#include "wifi_connect.h"
#include "temp_humi.h"
#include "rc522.h"
#include "pn532.h"
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
const unsigned long checkFlagsInterval = 2000; // avoid overlapping blocking GETs with temp PATCH

// Variables to send to the database
float temperature = 0.0;
float humidity = 0.0;
String rfidUid = "";
String qrData = "";

// Check mode flags
bool check_qr = false;
bool check_rfid = false;
bool checkFlagsReceived = false;

// RFID: check_rfid=true — RC522 200ms -> gap 50ms -> PN532 200ms -> gap 50ms (lặp) cho đến hết RFID_CHECK_TIMEOUT_MS
static unsigned long rfidSessionStartMs = 0;
static unsigned long rfidPhaseStartMs = 0;
static uint8_t rfidPhase = 0; // 0=RC522, 1=gap, 2=PN532, 3=gap
static const unsigned long RFID_RC522_MS = 200;
static const unsigned long RFID_PN532_MS = 450; // PN532 cần cửa sổ dài hơn + nhiều lần thử trong pn532ReadUID
static const unsigned long RFID_GAP_MS = 50;
static const unsigned long RFID_CHECK_TIMEOUT_MS = 10000; // tổng thời gian chờ thẻ (10s) — chỉnh nếu cần

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
    // Use non-blocking get with timeout handling
    bool newCheckQr = Database.get<bool>(aClient, "/sensors/check_qr");
    bool newCheckRfid = Database.get<bool>(aClient, "/sensors/check_rfid");
    
    // Check for errors in the async client
    if (aClient.lastError().code() != 0) {
        Serial.printf("Error reading check flags: %s (code: %d)\n", 
                     aClient.lastError().message().c_str(), 
                     aClient.lastError().code());
        // Don't update flags if there's an error
        return;
    }
    
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
    
    delay(400);
    wifiApplyPublicDns();
    delay(200);

    // Thử resolve vài lần (stack DNS đôi khi chậm sau khi đặt esp_netif/lwIP)
    IPAddress resolved;
    const char *probe = "identitytoolkit.googleapis.com";
    bool dnsOk = false;
    for (int attempt = 0; attempt < 5; attempt++) {
        if (WiFi.hostByName(probe, resolved)) {
            dnsOk = true;
            Serial.printf("[Firebase] DNS OK: %s -> %s\n", probe, resolved.toString().c_str());
            break;
        }
        Serial.printf("[Firebase] DNS thử %d/5 fail cho %s — áp lại DNS\n", attempt + 1, probe);
        wifiApplyPublicDns();
        delay(300);
    }
    if (!dnsOk) {
        Serial.println("[Firebase] DNS vẫn fail — thử mạng khác / firewall / router chặn Google");
    }

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

        if (!check_rfid) {
            rfidSessionStartMs = 0;
            rfidPhaseStartMs = 0;
            rfidPhase = 0;
        }
        
        // Temp/humidity first: synchronous PATCH (blocking) so it completes before blocking GETs below.
        if (currentTime - lastTempHumiSendTime >= tempHumiInterval) {
            lastTempHumiSendTime = currentTime;
            readSensorData();
            sendTempHumiToFirebase(temperature, humidity);
            Serial.printf("Sending temp/humi - Temperature: %.1f°C, Humidity: %.1f%%\n",
                         temperature, humidity);
        }
        
        // Read check flags (blocking GET) after temp send to reduce client contention
        if (currentTime - lastCheckFlagsReadTime >= checkFlagsInterval) {
            lastCheckFlagsReadTime = currentTime;
            readCheckFlagsFromFirebase();
        }
        
        if (check_rfid) {
            unsigned long now = millis();
            if (rfidSessionStartMs == 0) {
                rfidSessionStartMs = now;
                rfidPhaseStartMs = now;
                rfidPhase = 0;
                Serial.printf("[RFID] Chu kỳ: RC522 %lums -> nghỉ %lums -> PN532 %lums -> nghỉ %lums (lặp), tối đa %lu s\n",
                              (unsigned long)RFID_RC522_MS, (unsigned long)RFID_GAP_MS,
                              (unsigned long)RFID_PN532_MS, (unsigned long)RFID_GAP_MS,
                              (unsigned long)(RFID_CHECK_TIMEOUT_MS / 1000));
            }

            unsigned long elapsed = now - rfidSessionStartMs;
            if (elapsed >= RFID_CHECK_TIMEOUT_MS) {
                object_t jsonCheckRfid;
                object_t payload;
                payload.initObject();
                JsonWriter writer;
                writer.create(jsonCheckRfid, "check_rfid", false);
                writer.join(payload, 1, jsonCheckRfid);
                if (Database.update(aClient, "/sensors", payload)) {
                    check_rfid = false;
                    rfidSessionStartMs = 0;
                    rfidPhaseStartMs = 0;
                    rfidPhase = 0;
                    Serial.println("[RFID] Hết thời gian — đã gửi check_rfid=false");
                } else {
                    Serial.printf("[RFID] Hết thời gian nhưng PATCH lỗi: %s (code: %d)\n",
                                  aClient.lastError().message().c_str(),
                                  aClient.lastError().code());
                }
                delay(50);
            } else {
                unsigned long inPhase = now - rfidPhaseStartMs;
                if (rfidPhase == 0 && inPhase >= RFID_RC522_MS) {
                    rfidPhase = 1;
                    rfidPhaseStartMs = now;
                } else if (rfidPhase == 1 && inPhase >= RFID_GAP_MS) {
                    rfidPhase = 2;
                    rfidPhaseStartMs = now;
                } else if (rfidPhase == 2 && inPhase >= RFID_PN532_MS) {
                    rfidPhase = 3;
                    rfidPhaseStartMs = now;
                } else if (rfidPhase == 3 && inPhase >= RFID_GAP_MS) {
                    rfidPhase = 0;
                    rfidPhaseStartMs = now;
                }

                String uid;
                bool got = false;
                if (rfidPhase == 0) {
                    got = rc522ReadUID(uid);
                } else if (rfidPhase == 2) {
                    got = pn532ReadUID(uid);
                }

                if (got && !uid.isEmpty()) {
                    rfidUid = uid;
                    rfidUid.toUpperCase();
                    const bool fromRc522 = (rfidPhase == 0);
                    Serial.printf("RFID detected (%s): %s\n", fromRc522 ? "RC522 -> uid_1" : "PN532 -> uid_2", rfidUid.c_str());

                    object_t jsonUid1;
                    object_t jsonUid2;
                    object_t jsonCheckRfid;
                    object_t payload;
                    payload.initObject();
                    JsonWriter writer;
                    writer.create(jsonUid1, "uid_1", fromRc522 ? rfidUid.c_str() : "");
                    writer.create(jsonUid2, "uid_2", fromRc522 ? "" : rfidUid.c_str());
                    writer.create(jsonCheckRfid, "check_rfid", false);
                    writer.join(payload, 3, jsonUid1, jsonUid2, jsonCheckRfid);

                    bool rfidSent = Database.update(aClient, "/sensors", payload);
                    if (rfidSent) {
                        Serial.printf("Sending to Firebase - %s: %s, check_rfid: false\n",
                                      fromRc522 ? "uid_1" : "uid_2", rfidUid.c_str());
                        check_rfid = false;
                        rfidSessionStartMs = 0;
                        rfidPhaseStartMs = 0;
                        rfidPhase = 0;
                        Serial.println("RFID scan completed. check_rfid set to false. Resuming normal mode.");
                    } else {
                        Serial.printf("Failed to send RFID data: %s (code: %d)\n",
                                      aClient.lastError().message().c_str(),
                                      aClient.lastError().code());
                    }
                }
            }
        } else if (check_qr) {
            // QR priority mode: scan QR until data is sent and check_qr cleared
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
        }
    } else {
        // Firebase app is not ready - log periodically
        static unsigned long lastNotReadyLog = 0;
        unsigned long currentTime = millis();
        if (currentTime - lastNotReadyLog > 5000) { // Log every 5 seconds
            Serial.println("Firebase app not ready - cannot read/write data");
            lastNotReadyLog = currentTime;
        }
    }

    // Extra pumps so auth/async work progresses (main loop also uses delay() elsewhere)
    for (int i = 0; i < 4; i++) {
        app.loop();
        yield();
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

    // Synchronous PATCH (bool overload) — waits until done; async callback version can stall if app.loop() is infrequent.
    bool ok = Database.update(aClient, "/sensors", payload);
    if (!ok) {
        Serial.printf("RTDB temp/humi PATCH failed: %s (code: %d)\n",
                      aClient.lastError().message().c_str(),
                      aClient.lastError().code());
    }
}

// Send RFID and QR data to Firebase (uid_1 = chuỗi RFID legacy; uid_2 để trống)
void sendRfidQrToFirebase(String rfidUid, String qrData) {
    object_t jsonUid1;
    object_t jsonUid2;
    object_t jsonQr;
    object_t payload;
    payload.initObject();
    JsonWriter writer;
    writer.create(jsonUid1, "uid_1", rfidUid.isEmpty() ? "null" : rfidUid.c_str());
    writer.create(jsonUid2, "uid_2", "null");
    if (qrData.isEmpty()) {
        writer.create(jsonQr, "qr_data", "null");
    } else {
        writer.create(jsonQr, "qr_data", qrData.c_str());
    }
    writer.join(payload, 3, jsonUid1, jsonUid2, jsonQr);
    if (!Database.update(aClient, "/sensors", payload)) {
        Serial.printf("sendRfidQrToFirebase PATCH failed: %s (code: %d)\n",
                      aClient.lastError().message().c_str(), aClient.lastError().code());
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