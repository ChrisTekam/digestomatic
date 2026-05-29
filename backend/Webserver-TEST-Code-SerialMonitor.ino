// ===============================
// ESP32 AI MQTT Dashboard
// WITH LIVE SERIAL MONITOR PANEL
// ===============================

#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <PubSubClient.h>

// Include your custom files
#include "customTimer.ino"
#include "DHT11.ino"
#include "DS18B20.ino"

// ===============================
// WIFI CONFIG
// ===============================

const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";

// ===============================
// MQTT CONFIG
// ===============================

const char* mqtt_server = "broker.hivemq.com";
const int mqtt_port = 1883;
const char* topic_temp = "digester/sensors/temperature";

WiFiClient espClient;
PubSubClient mqttClient(espClient);
AsyncWebServer server(80);

CustomTimer sensorTimer(3000);

// ===============================
// SERIAL LOG SYSTEM
// ===============================

String serialLog = "";

void addLog(String message) {
    Serial.println(message);

    serialLog += message + "\n";

    if (serialLog.length() > 5000) {
        serialLog.remove(0, 2000);
    }
}

// ===============================
// HTML DASHBOARD
// ===============================

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>ESP32 MQTT Dashboard</title>

<script src="https://cdn.jsdelivr.net/npm/chart.js"></script>

<style>
* {
    margin: 0;
    padding: 0;
    box-sizing: border-box;
}

body {
    font-family: Arial, sans-serif;
    background: #0f172a;
    color: white;
    padding: 20px;
}

h1 {
    margin-bottom: 20px;
}

.dashboard-container {
    display: grid;
    grid-template-columns: repeat(auto-fit, minmax(300px, 1fr));
    gap: 20px;
}

.card {
    background: rgba(255,255,255,0.05);
    border: 1px solid rgba(255,255,255,0.1);
    border-radius: 18px;
    padding: 20px;
}

.card h2 {
    margin-bottom: 15px;
}

.value {
    font-size: 2rem;
    font-weight: bold;
}

.console-card {
    grid-column: 1 / -1;
}

#serial-console {
    background: black;
    color: #00ff88;
    padding: 14px;
    border-radius: 12px;
    height: 260px;
    overflow-y: auto;
    font-family: monospace;
    font-size: 0.9rem;
    white-space: pre-wrap;
}

canvas {
    margin-top: 20px;
}
</style>
</head>
<body>

<h1>ESP32 MQTT Dashboard</h1>

<div class="dashboard-container">

    <div class="card">
        <h2>Ambient Temperature</h2>
        <div class="value" id="ambientTemp">-- °C</div>
    </div>

    <div class="card">
        <h2>Core Temperature</h2>
        <div class="value" id="coreTemp">-- °C</div>
    </div>

    <div class="card">
        <h2>WiFi Status</h2>
        <div class="value" id="wifiStatus">Connected</div>
    </div>

    <div class="card console-card">
        <h2>ESP32 Serial Monitor</h2>
        <pre id="serial-console"></pre>
    </div>

</div>

<canvas id="tempChart"></canvas>

<script>
const ctx = document.getElementById('tempChart').getContext('2d');

const tempChart = new Chart(ctx, {
    type: 'line',
    data: {
        labels: [],
        datasets: [
            {
                label: 'Ambient Temp',
                data: [],
                borderWidth: 2
            },
            {
                label: 'Core Temp',
                data: [],
                borderWidth: 2
            }
        ]
    },
    options: {
        responsive: true,
        scales: {
            y: {
                beginAtZero: false
            }
        }
    }
});

async function updateData() {
    try {
        const response = await fetch('/data');
        const data = await response.json();

        document.getElementById('ambientTemp').innerText = data.ambient + ' °C';
        document.getElementById('coreTemp').innerText = data.core + ' °C';

        const time = new Date().toLocaleTimeString();

        tempChart.data.labels.push(time);
        tempChart.data.datasets[0].data.push(data.ambient);
        tempChart.data.datasets[1].data.push(data.core);

        if (tempChart.data.labels.length > 20) {
            tempChart.data.labels.shift();
            tempChart.data.datasets[0].data.shift();
            tempChart.data.datasets[1].data.shift();
        }

        tempChart.update();

    } catch(err) {
        console.error(err);
    }
}

async function updateSerialConsole() {
    try {
        const response = await fetch('/logs');
        const text = await response.text();

        const consoleElement = document.getElementById('serial-console');

        consoleElement.textContent = text;
        consoleElement.scrollTop = consoleElement.scrollHeight;

    } catch(err) {
        console.error(err);
    }
}

setInterval(updateData, 3000);
setInterval(updateSerialConsole, 1000);

updateData();
updateSerialConsole();
</script>

</body>
</html>
)rawliteral";

// ===============================
// WIFI CONNECTION
// ===============================

void connectWiFi() {

    addLog("Connecting to WiFi...");

    WiFi.begin(ssid, password);

    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        addLog("Attempting WiFi connection...");
    }

    addLog("WiFi Connected");
    addLog("IP Address: " + WiFi.localIP().toString());
}

// ===============================
// MQTT CONNECTION
// ===============================

void reconnectMQTT() {

    while (!mqttClient.connected()) {

        addLog("Connecting to MQTT...");

        if (mqttClient.connect("ESP32Client")) {
            addLog("MQTT Connected");
        }
        else {
            addLog("MQTT Failed. Retrying in 5 seconds...");
            delay(5000);
        }
    }
}

// ===============================
// SETUP
// ===============================

void setup() {

    Serial.begin(115200);

    addLog("=====================");
    addLog("BOOTING ESP32");
    addLog("=====================");

    initDHT();
    initDS18B20();

    connectWiFi();

    mqttClient.setServer(mqtt_server, mqtt_port);

    // ===========================
    // MAIN PAGE
    // ===========================

    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send_P(200, "text/html", index_html);
    });

    // ===========================
    // SENSOR DATA ENDPOINT
    // ===========================

    server.on("/data", HTTP_GET, [](AsyncWebServerRequest *request){

        float ambientTemp = getTemperature();
        float coreTemp = getDSTemperature();

        String json = "{";
        json += "\"ambient\":" + String(ambientTemp) + ",";
        json += "\"core\":" + String(coreTemp);
        json += "}";

        request->send(200, "application/json", json);
    });

    // ===========================
    // SERIAL LOG ENDPOINT
    // ===========================

    server.on("/logs", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(200, "text/plain", serialLog);
    });

    // ===========================
    // START SERVER
    // ===========================

    server.begin();

    addLog("Web server started");
}

// ===============================
// LOOP
// ===============================

void loop() {

    if (!mqttClient.connected()) {
        reconnectMQTT();
    }

    mqttClient.loop();

    if (sensorTimer.isReady()) {

        float ambientTemp = getTemperature();
        float coreTemp = getDSTemperature();

        String payload = String(ambientTemp);

        mqttClient.publish(topic_temp, payload.c_str());

        addLog(
            "Ambient: " + String(ambientTemp) +
            " C | Core: " + String(coreTemp) + " C"
        );

        addLog("MQTT Publish -> " + payload);
    }
}
