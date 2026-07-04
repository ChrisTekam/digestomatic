#ifndef DC_MOTOR_H
#define DC_MOTOR_H

#include <Arduino.h>

// ── Motor pins (L298N driver) ──────────────────────────────────────
#define MOTOR_ENA 25   // PWM speed control
#define MOTOR_IN1 26
#define MOTOR_IN2 27

// ── PWM config (ESP32 Arduino core v3.x API — ledcAttach/ledcWrite by pin) ─────
#define MOTOR_PWM_FREQ  1000
#define MOTOR_PWM_RES   8        // 0–255
#define MOTOR_SPEED     200      // default stir speed (0-255)

// ── Stirring behaviour ──────────────────────────────────────────────────────────
#define STIR_DURATION_MS      60000UL   // stir for 1 minute
#define METHANE_DIP_THRESHOLD 5.0f      // ppm drop vs. previous reading = "dip"
#define AUTO_STIR_COOLDOWN_MS 120000UL  // ignore further dips for 2 min after a stir

class DCMotor {
private:
    bool          stirring       = false;
    unsigned long stirStartTime  = 0;
    unsigned long lastAutoStirAt = 0;
    float         lastMethanePPM = -1.0f;
    bool          hasBaseline    = false;

    void driveForward(int speed) {
        digitalWrite(MOTOR_IN1, HIGH);
        digitalWrite(MOTOR_IN2, LOW);
        ledcWrite(MOTOR_ENA, speed);
    }

    void driveStop() {
        digitalWrite(MOTOR_IN1, LOW);
        digitalWrite(MOTOR_IN2, LOW);
        ledcWrite(MOTOR_ENA, 0);
    }

    void beginStir(const char* reason) {
        if (stirring) return; // already running, ignore re-trigger
        stirring = true;
        stirStartTime = millis();
        driveForward(MOTOR_SPEED);
        Serial.print("[MOTOR] Stir started (");
        Serial.print(reason);
        Serial.println(")");
    }

public:
    void begin() {
        pinMode(MOTOR_IN1, OUTPUT);
        pinMode(MOTOR_IN2, OUTPUT);
        ledcAttach(MOTOR_ENA, MOTOR_PWM_FREQ, MOTOR_PWM_RES);
        driveStop();
        Serial.println("[MOTOR] Stirrer initialized (ENA=25, IN1=26, IN2=27)");
    }

    // Call every loop() iteration — handles stir duration without blocking.
    void update() {
        if (stirring && millis() - stirStartTime >= STIR_DURATION_MS) {
            stirring = false;
            driveStop();
            lastAutoStirAt = millis();
            Serial.println("[MOTOR] Stir complete.");
        }
    }

    // Feed every new MQ-4 methane reading (ppm) here. Auto-triggers a 1-minute
    // stir when a slight dip vs. the previous reading is detected.
    void checkMethaneDip(float methanePPM) {
        if (!hasBaseline) {
            lastMethanePPM = methanePPM;
            hasBaseline = true;
            return;
        }

        float delta = lastMethanePPM - methanePPM;
        bool  cooldownOver = (millis() - lastAutoStirAt) >= AUTO_STIR_COOLDOWN_MS;

        if (delta >= METHANE_DIP_THRESHOLD && !stirring && cooldownOver) {
            Serial.printf("[MOTOR] Methane dip detected (%.1f -> %.1f ppm)\n", lastMethanePPM, methanePPM);
            beginStir("methane dip");
        }

        lastMethanePPM = methanePPM;
    }

    // Manual trigger — call this on user request (Foria command or dashboard
    // button), e.g. when an MQTT "/stir" command arrives.
    void manualStir() {
        Serial.println("[MOTOR] Manual stir requested.");
        beginStir("manual request");
    }

    bool isStirring() { return stirring; }
};

DCMotor stirrer;

void initMotor() {
    stirrer.begin();
}

#endif
