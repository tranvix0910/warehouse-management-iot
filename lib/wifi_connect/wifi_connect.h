#ifndef WIFI_CONNECT_H
#define WIFI_CONNECT_H

#include <WiFi.h>

extern const char* ssid;
extern const char* password;
extern const unsigned long timeout;

bool wifiConnect();
/** Gọi lại sau khi STA đã kết nối (ví dụ trước Firebase) để ép DNS 8.8.8.8 / 8.8.4.4 */
void wifiApplyPublicDns();

#endif // WIFI_CONNECT_H
