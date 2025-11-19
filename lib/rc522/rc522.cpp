#include "rc522.h"

static MFRC522 *rfid = nullptr;

void rc522Init(){
    Serial.println("Initializing RC522...");
    
    SPI.end();
    SPI.begin(RC522_SCK, RC522_MISO, RC522_MOSI, RC522_SDA);
    
    if (!rfid){
        rfid = new MFRC522(RC522_SDA, RC522_RST);
    }
    
    pinMode(RC522_RST, OUTPUT);
    digitalWrite(RC522_RST, HIGH);
    delay(10);
    rfid->PCD_Reset();
    delay(20);
    rfid->PCD_Init();
    rfid->PCD_SetAntennaGain(rfid->RxGain_max);
    delay(50);
    
    // Kiểm tra module RC522 đã nhận chưa kèm version cụ thể
    byte version = rfid->PCD_ReadRegister(MFRC522::VersionReg);
    Serial.printf("RC522 VersionReg: 0x%02X\n", version);
    if (version == 0x00 || version == 0xFF) {
        Serial.println("RC522 initialization failed! Please check wiring and power.");
    } else {
        byte major = (version & 0xF0) >> 4;
        byte minor = version & 0x0F;
        Serial.printf("RC522 initialized (version %d.%d)\n", major, minor);
    }
}

bool rc522ReadUID(String &uidOut){
    uidOut = "";
    if (!rfid){
        return false;
    }
    
    rfid->PCD_SetAntennaGain(rfid->RxGain_max);
    String uidString = "";  // Biến lưu UID dưới dạng chuỗi

    if (rfid->PICC_IsNewCardPresent()) {
        if (!rfid->PICC_ReadCardSerial()) {
            Serial.println("Card detected but read failed");
            rfid->PICC_HaltA();
            rfid->PCD_StopCrypto1();
            return false;
        }
    
        for (byte i = 0; i < rfid->uid.size; i++) {
            if (rfid->uid.uidByte[i] < 0x10) {
                uidString += "0"; // Thêm số 0 trước nếu số nhỏ hơn 0x10 để định dạng chuẩn
            }
            uidString += String(rfid->uid.uidByte[i], HEX);
            // Nếu không phải byte cuối cùng thì thêm dấu cách
            if (i != rfid->uid.size - 1) {
                uidString += " ";
            }
        }
        rfid->PICC_HaltA();
        rfid->PCD_StopCrypto1();
        
        // Gán UID vào uidOut và trả về true khi đọc được
        uidOut = uidString;
        return true;
    }
    
    return false;
}


