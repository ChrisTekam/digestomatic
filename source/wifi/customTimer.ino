#ifndef CUSTOM_TIMER_H
#define CUSTOM_TIMER_H

#include <Arduino.h>

class CustomTimer {
private:
    unsigned long previousMillis;
    unsigned long interval;

public:
    CustomTimer(unsigned long defaultIntervalMs = 1000) {
        interval = defaultIntervalMs;
        previousMillis = 0;
    }

    void setInterval(unsigned long newIntervalMs) {
        interval = newIntervalMs;
    }

    bool isReady() {
        unsigned long currentMillis = millis();
        if (currentMillis - previousMillis >= interval) {
            previousMillis = currentMillis;
            return true;
        }
        return false;
    }
};

#endif
