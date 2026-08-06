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
#define STIR_DURATION_MS      60000UL     // stir for 1 minute
#define AUTO_STIR_INTERVAL_MS 1800000UL   // automatic stir every 30 minutes

class DCMotor {
private:
    bool          stirring        = false;
    bool          isAutoStir      = false;         // true if the current stir was auto-triggered
    unsigned long stirStartTime   = 0;
    unsigned long stirDurationMs  = STIR_DURATION_MS; // set per-call by manualStir()/beginStir()
    unsigned long nextAutoStirAt  = 0;              // millis() timestamp of next scheduled auto-stir
    bool          autoTimerStarted = false;

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

    void beginStir(const char* reason, unsigned long durationMs, bool auto_) {
        if (stirring) return; // already running, ignore re-trigger
        stirring       = true;
        isAutoStir     = auto_;
        stirDurationMs = durationMs;
        stirStartTime  = millis();
        driveForward(MOTOR_SPEED);
        Serial.print("[MOTOR] Stir started (");
        Serial.print(reason);
        Serial.print(") for ");
        Serial.print(durationMs / 1000UL);
        Serial.println("s");
    }

public:
    void begin() {
        pinMode(MOTOR_IN1, OUTPUT);
        pinMode(MOTOR_IN2, OUTPUT);
        ledcAttach(MOTOR_ENA, MOTOR_PWM_FREQ, MOTOR_PWM_RES);
        driveStop();
        nextAutoStirAt = millis() + AUTO_STIR_INTERVAL_MS;
        autoTimerStarted = true;
        Serial.println("[MOTOR] Stirrer initialized (ENA=25, IN1=26, IN2=27)");
    }

    // Handles stir duration + the independent 30-min auto-stir schedule. Manual stirs don't affect it.
    void update() {
        if (stirring && millis() - stirStartTime >= stirDurationMs) {
            stirring = false;
            driveStop();
            Serial.println("[MOTOR] Stir complete.");
        }

        if (autoTimerStarted && millis() >= nextAutoStirAt) {
            // advance schedule regardless of whether this stir actually runs
            nextAutoStirAt += AUTO_STIR_INTERVAL_MS;
            if (!stirring) {
                Serial.println("[MOTOR] Scheduled 30-min auto-stir triggered.");
                beginStir("scheduled auto-stir", STIR_DURATION_MS, true);
            } else {
                Serial.println("[MOTOR] Scheduled auto-stir skipped (motor already running).");
            }
        }
    }

    // Manual trigger (dashboard/Foria). durationSec optional. Doesn't affect the auto-stir schedule.
    void manualStir(unsigned long durationSec = STIR_DURATION_MS / 1000UL) {
        Serial.println("[MOTOR] Manual stir requested.");
        beginStir("manual request", durationSec * 1000UL, false);
    }

    bool isStirring() { return stirring; }
    bool isAutoStirring() { return stirring && isAutoStir; }
};

DCMotor stirrer;

void initMotor() {
    stirrer.begin();
}

#endif
