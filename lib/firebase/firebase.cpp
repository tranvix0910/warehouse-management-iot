#include "firebase.h"

// Define the global Firebase objects
UserAuth user_auth(WEB_API_KEY, USER_EMAIL, USER_PASS);
FirebaseApp app;
WiFiClientSecure ssl_client;
using AsyncClient = AsyncClientClass;
AsyncClient aClient(ssl_client);
RealtimeDatabase Database;

// Timer variables for sending data every 30 seconds
unsigned long lastSendTime = 0;
const unsigned long sendInterval = 30000; // 30 seconds in milliseconds

// Variables to send to the database
float temperature = 0.0;
float humidity = 0.0;

// Function to generate random temperature and humidity values between 20-70
void generateRandomSensorData() {
    temperature = random(200, 701) / 10.0; // Random value between 20.0-70.0
    humidity = random(200, 701) / 10.0;    // Random value between 20.0-70.0
}

void processData(AsyncResult &aResult) {
    if (!aResult.isResult())
        return;
  
    if (aResult.isEvent())
        Firebase.printf("Event task: %s, msg: %s, code: %d\n", aResult.uid().c_str(), aResult.eventLog().message().c_str(), aResult.eventLog().code());
  
    if (aResult.isDebug())
        Firebase.printf("Debug task: %s, msg: %s\n", aResult.uid().c_str(), aResult.debug().c_str());
  
    if (aResult.isError())
        Firebase.printf("Error task: %s, msg: %s, code: %d\n", aResult.uid().c_str(), aResult.error().message().c_str(), aResult.error().code());
  
    if (aResult.available())
        Firebase.printf("task: %s, payload: %s\n", aResult.uid().c_str(), aResult.c_str());
}

void firebaseInit() {
    Serial.println("Initializing Firebase...");

    // Configure SSL client first
    configureSSLClient();
    Serial.println("SSL client configured");

    // Initialize Firebase app
    initializeApp(aClient, app, getAuth(user_auth), processData, "🔐 authTask");
    app.getApp<RealtimeDatabase>(Database);
    Database.url(DATABASE_URL);
    
    Serial.println("Firebase initialized successfully");
}

void appLoop() {
    app.loop();

    if (app.ready()){ 
        // Periodic data sending every 30 seconds
        unsigned long currentTime = millis();
        if (currentTime - lastSendTime >= sendInterval){
          // Update the last send time
          lastSendTime = currentTime;
          
          // Generate random sensor data
          generateRandomSensorData();
          
          // Send temperature data to Firebase
          Database.set<float>(aClient, "/sensors/temperature", temperature, processData, "RTDB_Send_Temperature");
          
          // Send humidity data to Firebase
          Database.set<float>(aClient, "/sensors/humidity", humidity, processData, "RTDB_Send_Humidity");
          
          // Print to serial for debugging
          Serial.printf("Sending data - Temperature: %.1f°C, Humidity: %.1f%%\n", temperature, humidity);
        }
      }
}

void configureSSLClient(){
    // Configure SSL client
    ssl_client.setInsecure();
    ssl_client.setHandshakeTimeout(30); // Increase timeout to 30 seconds
    ssl_client.setTimeout(30); // Set overall timeout to 30 seconds
}