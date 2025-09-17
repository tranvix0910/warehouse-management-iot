#ifndef TEMP_HUMI_H
#define TEMP_HUMI_H

#include <Arduino.h>
#include <DHT.h>

// DHT11 sensor configuration
#define DHT_PIN 19
#define DHT_TYPE DHT11

// Function declarations
void tempHumiInit();
float readTemperature();
float readHumidity();
bool isSensorReady();

#endif // TEMP_HUMI_H
