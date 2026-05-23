#include "customTimer.ino"
#include "DHT11.ino"
#include "DS18B20.ino"

CustomTimer sensorTimer(3000); // 3-second interval

void setup() {
    Serial.begin(115200);
    Serial.println("--- Initializing Multi-Sensor System ---");

    // Initialize the temperature sensors (for now)
    initDHT();
    initDS18B20();

    Serial.println("System Ready!");
}

void loop() {
    if (sensorTimer.isReady()) {
        // Read DHT11
        DHTData dhtData = readDHT();
        Serial.print("DHT11 Temperature: ");
        Serial.print(dhtData.temperature);
        Serial.println(" °C");

        // Read DS18B20
        float dsTemp = readDS18B20();
        Serial.print("DS18B20 Temperature: ");
        Serial.print(dsTemp);
        Serial.println(" °C");
    }
}