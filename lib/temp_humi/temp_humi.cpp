#include "temp_humi.h"

// Create DHT object
DHT dht(DHT_PIN, DHT_TYPE);

void tempHumiInit() {
    Serial.println("Initializing DHT11 sensor...");
    dht.begin();
    delay(2000); // Wait for sensor to stabilize
    
    // Test reading to verify sensor is working
    float testTemp = dht.readTemperature();
    if (isnan(testTemp)) {
        Serial.println("Warning: DHT11 sensor may not be connected properly");
    } else {
        Serial.print("DHT11 test reading - Temperature: ");
        Serial.print(testTemp);
        Serial.println("°C");
    }
    
    Serial.println("DHT11 sensor initialized");
}

float readTemperature() {
    // DHT11 requires at least 2 seconds between readings
    static unsigned long lastReadTime = 0;
    unsigned long currentTime = millis();
    
    // Wait if reading too soon
    if (currentTime - lastReadTime < 2000) {
        delay(2000 - (currentTime - lastReadTime));
    }
    
    // Try reading up to 3 times
    for (int attempt = 0; attempt < 3; attempt++) {
        float temperature = dht.readTemperature();
        
        if (!isnan(temperature)) {
            lastReadTime = millis();
            return temperature;
        }
        
        // Wait before retry
        if (attempt < 2) {
            delay(500);
        }
    }
    
    Serial.println("Failed to read temperature from DHT11 after 3 attempts");
    return -999.0; // Return error value
}

float readHumidity() {
    // DHT11 requires at least 2 seconds between readings
    static unsigned long lastReadTime = 0;
    unsigned long currentTime = millis();
    
    // Wait if reading too soon
    if (currentTime - lastReadTime < 2000) {
        delay(2000 - (currentTime - lastReadTime));
    }
    
    // Try reading up to 3 times
    for (int attempt = 0; attempt < 3; attempt++) {
        float humidity = dht.readHumidity();
        
        if (!isnan(humidity)) {
            lastReadTime = millis();
            return humidity;
        }
        
        // Wait before retry
        if (attempt < 2) {
            delay(500);
        }
    }
    
    Serial.println("Failed to read humidity from DHT11 after 3 attempts");
    return -999.0; // Return error value
}

bool isSensorReady() {
    // Check if sensor is ready by trying to read temperature
    float temp = dht.readTemperature();
    return !isnan(temp);
}
