#include "pn532.h"
#include "rc522.h"
#include <SPI.h>
#include <Adafruit_PN532.h>

static Adafruit_PN532 *nfc = nullptr;
static bool pn532Ready = false;

static void formatUidLikeRc522(const uint8_t *uid, uint8_t len, String &out) {
    out = "";
    for (uint8_t i = 0; i < len; i++) {
        if (uid[i] < 0x10) {
            out += "0";
        }
        out += String(uid[i], HEX);
        if (i + 1 < len) {
            out += " ";
        }
    }
    out.toUpperCase();
}

void pn532Init() {
    Serial.println(F("Initializing PN532 (shared SPI)..."));

    pinMode(PN532_SS, OUTPUT);
    digitalWrite(PN532_SS, HIGH);

    pinMode(PN532_RST, OUTPUT);
    digitalWrite(PN532_RST, LOW);
    delay(50);
    digitalWrite(PN532_RST, HIGH);
    delay(150);

    if (!nfc) {
        nfc = new Adafruit_PN532(PN532_SS, &SPI);
    }

    if (!nfc->begin()) {
        Serial.println(F("PN532: not found (check SPI wiring, SS=39, RST, 3.3V)."));
        pn532Ready = false;
        return;
    }

    // Adafruit begin() có thể gọi SPI.begin() không tham số trên ESP32 — khôi phục bus đã cấu hình
    SPI.begin(RFID_SPI_SCK, RFID_SPI_MISO, RFID_SPI_MOSI, -1);

    uint32_t versiondata = nfc->getFirmwareVersion();
    if (!versiondata) {
        Serial.println(F("PN532: firmware version read failed."));
        pn532Ready = false;
        return;
    }

    Serial.print(F("PN532 OK — IC: 0x"));
    Serial.print((versiondata >> 24) & 0xFF, HEX);
    Serial.print(F(", Vers: 0x"));
    Serial.println((versiondata >> 16) & 0xFF, HEX);

    nfc->SAMConfig();
    pn532Ready = true;
}

bool pn532ReadUID(String &uidOut) {
    uidOut = "";
    if (!pn532Ready || !nfc) {
        return false;
    }

    rc522ReleaseBus();
    SPI.begin(RFID_SPI_SCK, RFID_SPI_MISO, RFID_SPI_MOSI, -1);
    delay(3);

    uint8_t uid[7];
    uint8_t uidLength = 0;
    // Vài lần ngắn thay vì một lần 100ms — tăng cơ hội bắt thẻ khi appLoop thưa
    for (int attempt = 0; attempt < 4; attempt++) {
        if (nfc->readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength, 65)) {
            formatUidLikeRc522(uid, uidLength, uidOut);
            return true;
        }
        yield();
        delay(3);
    }
    return false;
}
