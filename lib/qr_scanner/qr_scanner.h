#ifndef QR_SCANNER_H
#define QR_SCANNER_H

#include <Arduino.h>
#include "esp_camera.h"
#include <quirc.h>

// Camera pins for ESP32-S3-EYE
#define PWDN_GPIO_NUM     -1
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM     15
#define SIOD_GPIO_NUM     4
#define SIOC_GPIO_NUM     5
#define Y9_GPIO_NUM       16
#define Y8_GPIO_NUM       17
#define Y7_GPIO_NUM       18
#define Y6_GPIO_NUM       12
#define Y5_GPIO_NUM       10
#define Y4_GPIO_NUM       8
#define Y3_GPIO_NUM       9
#define Y2_GPIO_NUM       11
#define VSYNC_GPIO_NUM    6
#define HREF_GPIO_NUM     7
#define PCLK_GPIO_NUM     13

// QR code detection variables
extern struct quirc *q;
extern struct quirc_code code;
extern struct quirc_data data;
extern quirc_decode_error_t err;

// Statistics for QR detection
extern int totalQRDetected;
extern int totalQRDecoded;

// Function declarations
void qrScannerInit();
bool scanQRCode(String &qrData);
void qrScannerCleanup();

#endif // QR_SCANNER_H
