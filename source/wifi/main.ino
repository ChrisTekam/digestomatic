#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <ESPAsyncWebServer.h>
#include <SPIFFS.h>
#include <PubSubClient.h>

#include "customTimer.ino"
#include "dht11.ino"
#include "ds18b20.ino"
#include "dc_motor.ino"
#include "mq4.ino"
#include "mq135.ino"
#include "data_logger.ino"

// ── WiFi Config ───────────────────────────────────────────────────────────────
const char* WIFI_SSID     = "Chris' A54";
const char* WIFI_PASSWORD = "hello123";

// ── MQTT Config (private HiveMQ Cloud cluster, over the internet via WiFi) ────
const char* MQTT_SERVER = "62e4f9c4dcc14e309386efc0e76fce89.s1.eu.hivemq.cloud";
const int   MQTT_PORT   = 8883;              // TLS port
const char* MQTT_USER   = "digestomatic";
const char* MQTT_PASS   = "digestocomms";

const char* TOPIC_PREFIX     = "digester/"; // private cluster now, so a simple prefix is fine
const String TOPIC_TEMP_OUT  = String(TOPIC_PREFIX) + "sensors/temp_outside";
const String TOPIC_TEMP_IN   = String(TOPIC_PREFIX) + "sensors/temp_internal";
const String TOPIC_HUM_OUT   = String(TOPIC_PREFIX) + "sensors/humidity_outside";
const String TOPIC_METHANE   = String(TOPIC_PREFIX) + "sensors/methane";
const String TOPIC_CO2       = String(TOPIC_PREFIX) + "sensors/co2";
const String TOPIC_COMMANDS  = String(TOPIC_PREFIX) + "control/commands"; // e.g. "/stir" from dashboard or Foria

WiFiClientSecure espClient;
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

    if (String(topic) == TOPIC_COMMANDS.c_str()) {
        // Manual stir requested by the user, via the dashboard button or Foria.
        if (msg.startsWith("/stir") || msg == "stir") {
            int colonIdx = msg.indexOf(':');
            if (colonIdx >= 0) {
                unsigned long durationSec = msg.substring(colonIdx + 1).toInt();
                if (durationSec > 0) {
                    stirrer.manualStir(durationSec);
                } else {
                    stirrer.manualStir();
                }
            } else {
                stirrer.manualStir();
            }
        }
    }
}

void reconnectMQTT() {
    while (!mqttClient.connected()) {
        Serial.print("Connecting to MQTT... ");
        String id = "ESP32Digester-" + String(random(0xffff), HEX);
        bool ok;
        if (strlen(MQTT_USER) > 0) {
            ok = mqttClient.connect(id.c_str(), MQTT_USER, MQTT_PASS);
        } else {
            ok = mqttClient.connect(id.c_str());
        }
        if (ok) {
            Serial.println("connected.");
            mqttClient.subscribe(TOPIC_COMMANDS.c_str());
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
    initDataLogger();

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

    espClient.setInsecure();

    // SPIFFS for serving dashboard.html
    if (!SPIFFS.begin(true)) {
        Serial.println("SPIFFS mount failed.");
        return;
    }

    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(SPIFFS, "/dashboard_incl_foria.html", "text/html");
    });

    server.on("/download-log", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (!SPIFFS.exists(LOG_FILE_PATH)) {
            request->send(404, "text/plain", "No log data yet.");
            return;
        }
        AsyncWebServerResponse *response = request->beginResponse(
            SPIFFS, LOG_FILE_PATH, "text/csv");
        response->addHeader("Content-Disposition", "attachment; filename=\"sensor_log.csv\"");
        request->send(response);
    });

    server.serveStatic("/", SPIFFS, "/");

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
        mqttClient.publish(TOPIC_TEMP_OUT.c_str(), buf);

        dtostrf(humOutside, 1, 2, buf);
        mqttClient.publish(TOPIC_HUM_OUT.c_str(), buf);

        dtostrf(tempInternal, 1, 2, buf);
        mqttClient.publish(TOPIC_TEMP_IN.c_str(), buf);

        if (methanePPM >= 0.0f) {
            dtostrf(methanePPM, 1, 2, buf);
            mqttClient.publish(TOPIC_METHANE.c_str(), buf);
            stirrer.checkMethaneDip(methanePPM); // auto-stir on methane dip
        }

        if (co2PPM >= 0.0f) {
            dtostrf(co2PPM, 1, 2, buf);
            mqttClient.publish(TOPIC_CO2.c_str(), buf);
        }

        dataLogger.addRow(tempOutside, tempInternal, methanePPM, co2PPM);
    }
}
