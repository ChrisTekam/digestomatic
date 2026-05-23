#ifndef DHT11_H
#define DHT11_H

#include <DHT.h>

#define DHTPIN 4
#define DHTTYPE DHT11

DHT dht(DHTPIN, DHTTYPE);

void initDHT() {
    dht.begin();
}

struct DHTData {
    float temperature;
};

DHTData readDHT() {
    DHTData data;
    data.temperature = dht.readTemperature(); // in degrees Celsius
    return data;
}

#endif