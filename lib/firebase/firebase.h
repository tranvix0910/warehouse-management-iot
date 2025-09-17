#ifndef FIREBASE_H
#define FIREBASE_H

#define ENABLE_USER_AUTH
#define ENABLE_DATABASE

#include <Arduino.h>
#include <WiFiClientSecure.h>
#include <FirebaseClient.h>

#define WEB_API_KEY "AIzaSyC7X5j-uDMLjnGHeYth2yA_2pgzfy8Eplc"
#define DATABASE_URL "https://mobile-app-development-1a585-default-rtdb.asia-southeast1.firebasedatabase.app/"
#define USER_EMAIL "khanhnlm2509@gmail.com"
#define USER_PASS "123456"

extern UserAuth user_auth;
extern FirebaseApp app;
extern WiFiClientSecure ssl_client;
extern AsyncClientClass aClient;
extern RealtimeDatabase Database;

void firebaseInit();
void processData(AsyncResult &aResult);
void appLoop();
void readSensorData();
void configureSSLClient();

#endif // FIREBASE_H