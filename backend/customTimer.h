#ifndef CUSTOM_TIMER_H
#define CUSTOM_TIMER_H

#include <Arduino.h>

class CustomTimer {
private:
    unsigned long previousMillis;
    unsigned long interval;

public:
    // Constructor: sets the default delay interval in milliseconds
    CustomTimer(unsigned long defaultIntervalMs = 1000) {
        interval = defaultIntervalMs;
        previousMillis = 0;
    }

    // Call this to dynamically change the delay time whenever you want
    void setInterval(unsigned long newIntervalMs) {
        interval = newIntervalMs;
    }

    // Checks if the time interval has passed. 
    // Returns true ONCE per interval, then automatically resets.
    bool isReady() {
        unsigned long currentMillis = millis();
        if (currentMillis - previousMillis >= interval) {
            previousMillis = currentMillis; // Reset the baseline
            return true;
        }
        return false;
    }
};

#endif