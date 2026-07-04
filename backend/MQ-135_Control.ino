#include <Arduino.h>
#include <math.h>

// ====================== MQ-135 Configuration ======================
#define MQ135_PIN      35      // Analog pin
#define MQ135_RL       1.0f    // Load resistor (kΩ) - change to 10.0 if your module uses 10kΩ
#define MQ135_DIVIDER  0.353f  // Voltage divider ratio
#define MQ135_VCC      5.0f

// Calibration (replace after calibration)
#define MQ135_R0       0.964f

// CO2 curve
#define MQ135_A        116.602f
#define MQ135_B       -2.769f
// ================================================================

float readRs(int &raw, float &vAdc, float &vSensor)
{
    raw = analogRead(MQ135_PIN);

    vAdc = (raw / 4095.0f) * 3.3f;
    vSensor = vAdc / MQ135_DIVIDER;

    if (vSensor <= 0.0f)
        return -1.0f;

    return MQ135_RL * (MQ135_VCC - vSensor) / vSensor;
}

void setup()
{
    Serial.begin(115200);
    delay(2000);

    analogReadResolution(12);

    Serial.println();
    Serial.println("========== MQ-135 Test ==========");
    Serial.println("Warming up...");
    delay(10000);   // Allow sensor to stabilize
}

void loop()
{
    int raw;
    float vAdc;
    float vSensor;

    float Rs = readRs(raw, vAdc, vSensor);

    if (Rs < 0)
    {
        Serial.println("Sensor error");
        delay(2000);
        return;
    }

    float ratio = Rs / MQ135_R0;
    float ppm = MQ135_A * pow(ratio, MQ135_B);

    Serial.println("----------------------------------------");
    Serial.printf("ADC Raw      : %d\n", raw);
    Serial.printf("ADC Voltage  : %.3f V\n", vAdc);
    Serial.printf("Sensor Volt  : %.3f V\n", vSensor);
    Serial.printf("Rs           : %.3f kΩ\n", Rs);
    Serial.printf("R0           : %.3f kΩ\n", MQ135_R0);
    Serial.printf("Rs/R0        : %.3f\n", ratio);
    Serial.printf("CO2 Estimate : %.1f ppm\n", ppm);

    delay(2000);
}