# QR Scanner Module

This module provides QR code scanning functionality for the ESP32-S3-EYE development board using the built-in camera.

## Features

- QR code detection and decoding using the quirc library
- Optimized camera settings for QR code scanning
- Adaptive thresholding for better detection of low-contrast QR codes
- Statistics tracking for detection success rates

## Hardware Requirements

- ESP32-S3-EYE development board with built-in camera
- Camera pins are pre-configured for the ESP32-S3-EYE

## Functions

- `qrScannerInit()`: Initialize the camera and QR detection system
- `scanQRCode(String &qrData)`: Scan for QR codes and return the decoded data
- `qrScannerCleanup()`: Clean up resources when done

## Usage

```cpp
#include "qr_scanner.h"

void setup() {
    qrScannerInit();
}

void loop() {
    String qrData;
    if (scanQRCode(qrData)) {
        Serial.println("QR Code: " + qrData);
    }
    delay(1000);
}
```

## Camera Configuration

The camera is configured with:
- Grayscale format for optimal QR detection
- QVGA resolution (320x240)
- Optimized brightness, contrast, and sharpness settings
- Auto gain and exposure control

## Dependencies

- espressif/esp32-camera@^2.0.4
- danielrb/quirc@^1.0.0
