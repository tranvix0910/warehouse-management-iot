#ifndef RC522_H
#define RC522_H

#include <Arduino.h>
#include <SPI.h>
#include <MFRC522.h>

// Bus SPI dùng chung RC522 + PN532 (ESP32-S3)
#define RFID_SPI_SCK  41
#define RFID_SPI_MISO 2
#define RFID_SPI_MOSI 1

// RC522 — CS slave 1; RST dùng GPIO 42 (trước đây là SS)
#define RC522_SDA 40   // SS / CS
#define RC522_RST 42   // RST
#define RC522_SCK RFID_SPI_SCK
#define RC522_MOSI RFID_SPI_MOSI
#define RC522_MISO RFID_SPI_MISO

void rc522Init();
bool rc522ReadUID(String &uidOut);
/** Tắt antenna + CS cao — gọi trước khi đọc PN532 để giảm nhiễu RF/SPI trên bus chung. */
void rc522ReleaseBus();
/** Bật lại antenna trước khi quét RC522 (được gọi trong rc522ReadUID). */
void rc522PrepareRead();

#endif // RC522_H

