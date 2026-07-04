#ifndef MQ135_H
#define MQ135_H

#include <Arduino.h>

// ── Pin & voltage divider ─────────────────────────────────────────────────────
#define MQ135_PIN       35
#define MQ135_RL        1.0f    // load resistor on breakout (kΩ)
#define MQ135_DIVIDER   0.353f  // Vadc/Vsensor ratio: 1.8k/(1.8k+3.3k)
#define MQ135_VCC       5.0f

// ── Calibration (update R0 after outdoor calibration) ─────────────────────────
#define MQ135_R0        0.964f  // measured R0 in clean air (kΩ)

// ── Curve constants for CO₂ (log-log fit from datasheet) ─────────────────────
// ppm = A * (Rs/R0)^B
#define MQ135_A         116.602f
#define MQ135_B         -2.769f

float readMQ135Rs() {
    int   raw          = analogRead(MQ135_PIN);
    float vAdc         = (raw / 4095.0f) * 3.3f;
    float vSensor      = vAdc / MQ135_DIVIDER;
    if (vSensor <= 0.0f) return -1.0f;
    return MQ135_RL * (MQ135_VCC - vSensor) / vSensor;
}

float readMQ135PPM() {
    float rs = readMQ135Rs();
    if (rs < 0.0f) return -1.0f;
    return MQ135_A * pow(rs / MQ135_R0, MQ135_B);
}

void initMQ135() {
    analogReadResolution(12);
    Serial.println("[MQ135] CO2 sensor initialized (GPIO35)");
}

#endif
