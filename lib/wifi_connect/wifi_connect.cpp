#include "wifi_connect.h"
#include <cstring>
#include "esp_netif.h"
#include "lwip/dns.h"
#include "lwip/ip4_addr.h"

// Sửa trực tiếp SSID và mật khẩu tại đây
const char* ssid = "AEPTIT";
const char* password = "20242024";

const unsigned long timeout = 10000;

/** Đặt DNS qua esp_netif + lwIP — WiFi.config(DNS) trên ESP32-S3 thường không đủ / không áp dụng đúng. */
void wifiApplyPublicDns() {
    if (WiFi.status() != WL_CONNECTED) {
        return;
    }

    esp_netif_t *sta = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (sta) {
        esp_netif_dns_info_t dns;
        memset(&dns, 0, sizeof(dns));
        dns.ip.type = IPADDR_TYPE_V4;
        IP4_ADDR(&dns.ip.u_addr.ip4, 8, 8, 8, 8);
        esp_err_t e1 = esp_netif_set_dns_info(sta, ESP_NETIF_DNS_MAIN, &dns);
        IP4_ADDR(&dns.ip.u_addr.ip4, 8, 8, 4, 4);
        esp_err_t e2 = esp_netif_set_dns_info(sta, ESP_NETIF_DNS_BACKUP, &dns);
        Serial.printf("[WiFi] esp_netif DNS: MAIN/BACKUP -> %s / %s\n",
                      esp_err_to_name(e1), esp_err_to_name(e2));
    } else {
        Serial.println("[WiFi] esp_netif WIFI_STA_DEF = null");
    }

    ip_addr_t ns;
    IP_ADDR4(&ns, 8, 8, 8, 8);
    dns_setserver(0, &ns);
    IP_ADDR4(&ns, 8, 8, 4, 4);
    dns_setserver(1, &ns);
    Serial.println("[WiFi] lwip dns_setserver: 8.8.8.8, 8.8.4.4");
}

bool wifiConnect() {
    unsigned long startingTime = millis();
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
    Serial.println("Connecting to WiFi...");

    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
        if ((millis() - startingTime) > timeout) {
            Serial.println();
            Serial.println("WiFi connection timeout!");
            return false;
        }
    }
    Serial.println();
    Serial.println("WiFi connected!");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());

    wifiApplyPublicDns();
    delay(100);
    return true;
}
