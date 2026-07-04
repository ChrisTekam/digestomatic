#ifndef MQ4_H
#define MQ4_H

#include <Arduino.h>

// ── Pin & voltage divider ─────────────────────────────────────────────────────
#define MQ4_PIN       34
#define MQ4_RL        1.0f    // load resistor on breakout (kΩ)
#define MQ4_DIVIDER   0.353f  // Vadc/Vsensor ratio: 1.8k/(1.8k+3.3k)
#define MQ4_VCC       5.0f

// ── Calibration (update R0 after outdoor calibration) ─────────────────────────
#define MQ4_R0        0.347f  // measured R0 in clean air (kΩ)

// ── Curve constants for CH₄ (log-log fit from datasheet) ─────────────────────
// ppm = A * (Rs/R0)^B
#define MQ4_A         1012.7f
#define MQ4_B         -2.786f

float readMQ4Rs() {
    int   raw          = analogRead(MQ4_PIN);
    float vAdc         = (raw / 4095.0f) * 3.3f;
    float vSensor      = vAdc / MQ4_DIVIDER;
    if (vSensor <= 0.0f) return -1.0f;
    return MQ4_RL * (MQ4_VCC - vSensor) / vSensor;
}

float readMQ4PPM() {
    float rs = readMQ4Rs();
    if (rs < 0.0f) return -1.0f;
    return MQ4_A * pow(rs / MQ4_R0, MQ4_B);
}

void initMQ4() {
    analogReadResolution(12);
    Serial.println("[MQ4] Methane sensor initialized (GPIO34)");
}

#endif
