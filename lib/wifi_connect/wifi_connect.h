#ifndef WIFI_CONNECT_H
#define WIFI_CONNECT_H

#include <WiFi.h>

extern const char* ssid;
extern const char* password;

extern const unsigned long timeout;

bool wifiConnect();

#endif // WIFI_CONNECT_H
