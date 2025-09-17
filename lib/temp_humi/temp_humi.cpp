#include "temp_humi.h"

// Create DHT object
DHT dht(DHT_PIN, DHT_TYPE);

void tempHumiInit() {
    Serial.println("Initializing DHT11 sensor...");
    dht.begin();
    delay(2000); // Wait for sensor to stabilize
    Serial.println("DHT11 sensor initialized");
}

float readTemperature() {
    float temperature = dht.readTemperature();
    
    if (isnan(temperature)) {
        Serial.println("Failed to read temperature from DHT11");
        return -999.0; // Return error value
    }
    
    return temperature;
}

float readHumidity() {
    float humidity = dht.readHumidity();
    
    if (isnan(humidity)) {
        Serial.println("Failed to read humidity from DHT11");
        return -999.0; // Return error value
    }
    
    return humidity;
}

bool isSensorReady() {
    // Check if sensor is ready by trying to read temperature
    float temp = dht.readTemperature();
    return !isnan(temp);
}
