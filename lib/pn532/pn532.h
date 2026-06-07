#ifndef PN532_LIB_H
#define PN532_LIB_H

#include <Arduino.h>

// PN532 — CS slave 2 (SPI chung với RC522, xem rc522.h)
#define PN532_SS  39
#define PN532_RST 11

void pn532Init();
bool pn532ReadUID(String &uidOut);

#endif // PN532_LIB_H
