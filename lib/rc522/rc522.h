#ifndef RC522_H
#define RC522_H

#include <Arduino.h>
#include <SPI.h>
#include <MFRC522.h>

#define RC522_SDA 5
#define RC522_SCK 12
#define RC522_MOSI 11
#define RC522_MISO 13
#define RC522_RST 4

void rc522Init();
bool rc522ReadUID(String &uidOut);

#endif // RC522_H

