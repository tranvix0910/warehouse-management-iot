#include "rc522.h"

static MFRC522 *rfid = nullptr;

void rc522Init(){
    // Initialize custom SPI bus with specified pins
    SPI.begin(RC522_SCK, RC522_MISO, RC522_MOSI, RC522_SDA);
    if (!rfid){
        rfid = new MFRC522(RC522_SDA, RC522_RST);
    }
    rfid->PCD_Init();
    delay(50);
    Serial.println("RC522 initialized");
}

bool rc522ReadUID(String &uidOut){
    uidOut = "";
    if (!rfid){
        return false;
    }
    // Look for new cards
    if (!rfid->PICC_IsNewCardPresent()){
        return false;
    }
    if (!rfid->PICC_ReadCardSerial()){
        return false;
    }

    // Convert UID to formatted hex string: "XX XX XX XX" (4 groups of 2 chars)
    String uidStr = "";
    for (byte i = 0; i < 4; i++){
        uint8_t value = (i < rfid->uid.size) ? rfid->uid.uidByte[i] : 0x00;
        String byteHex = String(value, HEX);
        if (byteHex.length() < 2) byteHex = "0" + byteHex;
        byteHex.toUpperCase();
        uidStr += byteHex;
        if (i < 3) uidStr += " ";
    }
    uidOut = uidStr;

    // Halt PICC and stop encryption on PCD
    rfid->PICC_HaltA();
    rfid->PCD_StopCrypto1();
    return true;
}


