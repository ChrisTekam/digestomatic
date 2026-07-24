#ifndef DHT11_H
#define DHT11_H

#include <DHT.h>

#define DHTPIN 4
#define DHTTYPE DHT11

DHT dht(DHTPIN, DHTTYPE);

void initDHT() {
    dht.begin();
}

float readDHT() {
    return dht.readTemperature(); // Celsius
}

float readDHTHumidity() {
    return dht.readHumidity(); // %RH
}

#endif
