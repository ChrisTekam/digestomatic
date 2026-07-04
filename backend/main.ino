#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <SPIFFS.h>
#include <PubSubClient.h>

#include "customTimer.ino"
#include "dht11.ino"
#include "ds18b20.ino"
#include "dc_motor.ino"
#include "mq4.ino"
#include "mq135.ino"

// ── WiFi Config ───────────────────────────────────────────────────────────────
const char* WIFI_SSID     = "WIFI_SSID";
const char* WIFI_PASSWORD = "WIFI_PASSWORD";

// ── MQTT Config ───────────────────────────────────────────────────────────────
const char* MQTT_SERVER = "broker.hivemq.com";
const int   MQTT_PORT   = 1883;

const char* TOPIC_TEMP_OUT  = "digester/sensors/temp_outside";
const char* TOPIC_TEMP_IN   = "digester/sensors/temp_internal";
const char* TOPIC_HUM_OUT   = "digester/sensors/humidity_outside";
const char* TOPIC_METHANE   = "digester/sensors/methane";
const char* TOPIC_CO2       = "digester/sensors/co2";
const char* TOPIC_COMMANDS  = "digester/control/commands"; // e.g. "/stir" from dashboard or Foria

WiFiClient   espClient;
PubSubClient mqttClient(espClient);
AsyncWebServer server(80);

CustomTimer sensorTimer(5000); // 5-second interval

// ── MQTT ─────────────────────────────────────────────────────────────────────
void onMqttMessage(char* topic, byte* payload, unsigned int length) {
    String msg;
    for (unsigned int i = 0; i < length; i++) msg += (char)payload[i];
    msg.trim();

    Serial.print("[MQTT] ");
    Serial.print(topic);
    Serial.print(" -> ");
    Serial.println(msg);

    if (String(topic) == TOPIC_COMMANDS) {
        // Manual stir requested by the user, via the dashboard button or Foria.
        if (msg == "/stir" || msg == "/stir_now" || msg == "stir") {
            stirrer.manualStir();
        }
        // Future commands: /vent_gas, /heat_on, etc.
    }
}

void reconnectMQTT() {
    while (!mqttClient.connected()) {
        Serial.print("Connecting to MQTT... ");
        String id = "ESP32Digester-" + String(random(0xffff), HEX);
        if (mqttClient.connect(id.c_str())) {
            Serial.println("connected.");
            mqttClient.subscribe(TOPIC_COMMANDS);
        } else {
            Serial.print("failed (rc=");
            Serial.print(mqttClient.state());
            Serial.println("). Retry in 5s.");
            delay(5000);
        }
    }
}

// ── Setup ─────────────────────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    analogReadResolution(12);
    analogSetAttenuation(ADC_11db);

    initDHT();
    initDS18B20();
    initMotor();
    initMQ4();
    initMQ135();

    // Connect to WiFi
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    Serial.print("Connecting to WiFi");
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println(" connected.");
    Serial.print("Dashboard: http://");
    Serial.println(WiFi.localIP());

    // SPIFFS for serving dashboard.html
    if (!SPIFFS.begin(true)) {
        Serial.println("SPIFFS mount failed.");
        return;
    }

    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(SPIFFS, "/dashboard.html", "text/html");
    });

    server.begin();
    Serial.println("Web server started.");

    mqttClient.setServer(MQTT_SERVER, MQTT_PORT);
    mqttClient.setCallback(onMqttMessage);
}

// ── Loop ──────────────────────────────────────────────────────────────────────
void loop() {
    if (!mqttClient.connected()) reconnectMQTT();
    mqttClient.loop();

    stirrer.update(); // non-blocking — handles the 1-minute stir timing

    if (sensorTimer.isReady()) {
        float tempOutside  = readDHT();
        float humOutside   = readDHTHumidity();
        float tempInternal = readDS18B20();
        float methanePPM   = readMQ4PPM();
        float co2PPM       = readMQ135PPM();

        Serial.print("Outside: ");  Serial.print(tempOutside,  1); Serial.print(" °C  |  ");
        Serial.print("Humidity: "); Serial.print(humOutside,   1); Serial.print(" %  |  ");
        Serial.print("Internal: "); Serial.print(tempInternal, 1); Serial.print(" °C  |  ");
        Serial.print("CH4: ");      Serial.print(methanePPM,   1); Serial.print(" ppm  |  ");
        Serial.print("CO2: ");      Serial.print(co2PPM,       1); Serial.println(" ppm");

        char buf[10];

        dtostrf(tempOutside, 1, 2, buf);
        mqttClient.publish(TOPIC_TEMP_OUT, buf);

        dtostrf(humOutside, 1, 2, buf);
        mqttClient.publish(TOPIC_HUM_OUT, buf);

        dtostrf(tempInternal, 1, 2, buf);
        mqttClient.publish(TOPIC_TEMP_IN, buf);

        if (methanePPM >= 0.0f) {
            dtostrf(methanePPM, 1, 2, buf);
            mqttClient.publish(TOPIC_METHANE, buf);
            stirrer.checkMethaneDip(methanePPM); // auto-stir on methane dip
        }

        if (co2PPM >= 0.0f) {
            dtostrf(co2PPM, 1, 2, buf);
            mqttClient.publish(TOPIC_CO2, buf);
        }
    }
}
