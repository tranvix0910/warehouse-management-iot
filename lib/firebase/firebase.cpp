#include "firebase.h"

// Define the global Firebase objects
UserAuth user_auth(WEB_API_KEY, USER_EMAIL, USER_PASS);
FirebaseApp app;
WiFiClientSecure ssl_client;
using AsyncClient = AsyncClientClass;
AsyncClient aClient(ssl_client);
RealtimeDatabase Database;

// Timer variables for sending data every 10 seconds
unsigned long lastSendTime = 0;
const unsigned long sendInterval = 10000; // 10 seconds in milliseconds

// Variables to send to the database
int intValue = 0;
float floatValue = 0.01;
String stringValue = "";

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
    initializeApp(aClient, app, getAuth(user_auth), processData, "🔐 authTask");
    app.getApp<RealtimeDatabase>(Database);
    Database.url(DATABASE_URL);
}

void appLoop() {
    app.loop();

    if (app.ready()){ 
        // Periodic data sending every 10 seconds
        unsigned long currentTime = millis();
        if (currentTime - lastSendTime >= sendInterval){
          // Update the last send time
          lastSendTime = currentTime;
          
          // send a string
          stringValue = "value_" + String(currentTime);
          Database.set<String>(aClient, "/test/string", stringValue, processData, "RTDB_Send_String");
          // send an int
          Database.set<int>(aClient, "/test/int", intValue, processData, "RTDB_Send_Int");
          intValue++; //increment intValue in every loop
          // send a string
          floatValue = 0.01 + random (0,100);
          Database.set<float>(aClient, "/test/float", floatValue, processData, "RTDB_Send_Float");
        }
      }
}

void configureSSLClient(){
    // Configure SSL client
  ssl_client.setInsecure();
  ssl_client.setHandshakeTimeout(5);
}