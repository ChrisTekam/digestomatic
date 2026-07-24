#ifndef DS18B20_H
#define DS18B20_H

#include <OneWire.h>
#include <DallasTemperature.h>

#define ONE_WIRE_BUS 15

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature ds18b20(&oneWire);

void initDS18B20() {
    ds18b20.begin();
}

float readDS18B20() {
    ds18b20.requestTemperatures();
    return ds18b20.getTempCByIndex(0); // Celsius
}

#endif
