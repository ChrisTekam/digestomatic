#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <PubSubClient.h>

// Include your custom files
#include "customTimer.ino"
#include "DHT11.ino"
#include "DS18B20.ino"

// --- Wi-Fi Configuration ---
const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";

// --- MQTT Configuration ---
const char* mqtt_server = "broker.hivemq.com";
const int mqtt_port = 1883;

const char* topic_temp = "digester/sensors/temperature";

WiFiClient espClient;
PubSubClient mqttClient(espClient);
AsyncWebServer server(80);

CustomTimer sensorTimer(3000); // 3-second interval

// --- Embedded HTML Dashboard ---
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Digestomatic - AI MQTT Command Center</title>

    <script src="https://cdn.jsdelivr.net/npm/chart.js"></script>
    <script src="https://cdnjs.cloudflare.com/ajax/libs/paho-mqtt/1.0.1/mqttws31.min.js" type="text/javascript"></script>

    <style>
        :root {
            --bg-color: #0f172a;
            --card-bg: #1e293b;
            --text-main: #f8fafc;
            --text-muted: #94a3b8;
            --border-color: #334155;
            --accent-temp: #ef4444;
            --accent-methane: #10b981;
            --accent-ai: #a855f7;
        }

        [data-theme="light"] {
            --bg-color: #f1f5f9;
            --card-bg: #ffffff;
            --text-main: #0f172a;
            --text-muted: #64748b;
            --border-color: #cbd5e1;
        }

        body {
            font-family: system-ui, -apple-system, sans-serif;
            background-color: var(--bg-color);
            color: var(--text-main);
            margin: 0;
            padding: 20px;
            display: flex;
            flex-direction: column;
            align-items: center;
            transition: background-color 0.3s ease, color 0.3s ease;
        }

        .dashboard-container {
            width: 100%;
            max-width: 1300px;
            display: flex;
            flex-direction: column;
        }

        header {
            width: 100%;
            display: flex;
            justify-content: space-between;
            align-items: center;
            margin-bottom: 24px;
            border-bottom: 1px solid var(--border-color);
            padding-bottom: 12px;
        }

        h1 { margin: 0; font-size: 1.8rem; font-weight: 800; letter-spacing: -0.025em; }

        .header-controls {
            display: flex;
            align-items: center;
            gap: 20px;
        }

        #time-display { font-size: 1.1rem; color: var(--text-muted); font-variant-numeric: tabular-nums; }

        .theme-switch-wrapper {
            display: flex;
            align-items: center;
            gap: 8px;
            font-size: 0.875rem;
            color: var(--text-muted);
        }

        .theme-switch {
            display: inline-block;
            height: 24px;
            position: relative;
            width: 48px;
        }

        .theme-switch input { display: none; }

        .slider {
            background-color: #cbd5e1;
            bottom: 0;
            cursor: pointer;
            left: 0;
            position: absolute;
            right: 0;
            top: 0;
            transition: .4s;
            border-radius: 34px;
        }

        .slider:before {
            background-color: #fff;
            bottom: 3px;
            content: "";
            height: 18px;
            left: 3px;
            position: absolute;
            transition: .4s;
            width: 18px;
            border-radius: 50%;
            box-shadow: 0 1px 3px rgba(0,0,0,0.4);
        }

        input:checked + .slider { background-color: #3b82f6; }
        input:checked + .slider:before { transform: translateX(24px); }

        .main-layout {
            display: grid;
            grid-template-columns: 3fr 1fr;
            gap: 20px;
            width: 100%;
        }

        @media (max-width: 1024px) {
            .main-layout { grid-template-columns: 1fr; }
        }

        .telemetry-column {
            display: flex;
            flex-direction: column;
            gap: 20px;
        }

        .metrics-grid {
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(200px, 1fr));
            gap: 20px;
        }

        .sidebar-column {
            display: flex;
            flex-direction: column;
            gap: 20px;
            height: 100%;
        }

        .card {
            background-color: var(--card-bg);
            border-radius: 12px;
            padding: 20px;
            box-shadow: 0 4px 6px -1px rgba(0, 0, 0, 0.05);
            border: 1px solid var(--border-color);
            transition: background-color 0.3s ease, border-color 0.3s ease;
        }

        .card-title {
            font-size: 0.85rem;
            text-transform: uppercase;
            letter-spacing: 0.05em;
            color: var(--text-muted);
            margin-bottom: 12px;
        }

        .card-value { font-size: 2.3rem; font-weight: 700; font-variant-numeric: tabular-nums; }
        .temp-card { border-top: 4px solid var(--accent-temp); }
        .methane-card { border-top: 4px solid var(--accent-methane); }

        .ai-status-card {
            border-top: 4px solid var(--accent-ai);
            flex-grow: 1;
            display: flex;
            flex-direction: column;
        }

        .ai-log-display {
            background-color: rgba(0, 0, 0, 0.15);
            border: 1px solid var(--border-color);
            border-radius: 8px;
            padding: 12px;
            flex-grow: 1;
            overflow-y: auto;
            max-height: 480px;
            font-size: 0.9rem;
            line-height: 1.4;
        }

        .ai-status-entry {
            margin-bottom: 12px;
            padding-bottom: 12px;
            border-bottom: 1px dashed var(--border-color);
        }

        .ai-status-entry:last-child {
            margin-bottom: 0;
            padding-bottom: 0;
            border-bottom: none;
        }

        .ai-time {
            font-size: 0.75rem;
            color: var(--accent-ai);
            font-weight: bold;
            display: block;
            margin-bottom: 2px;
        }

        .command-card { border-top: 4px solid var(--text-muted); }

        .command-input-group { display: flex; gap: 8px; }

        .command-input-group input {
            flex-grow: 1;
            background-color: rgba(0, 0, 0, 0.2);
            border: 1px solid var(--border-color);
            border-radius: 6px;
            padding: 10px;
            color: var(--text-main);
            font-family: inherit;
        }

        .command-input-group input:focus { outline: 1px solid var(--accent-ai); }

        .command-input-group button {
            background-color: var(--accent-ai);
            color: white;
            border: none;
            border-radius: 6px;
            padding: 0 16px;
            font-weight: bold;
            cursor: pointer;
            transition: opacity 0.2s;
        }

        .command-input-group button:hover { opacity: 0.9; }

        .chart-container {
            width: 100%;
            background-color: var(--card-bg);
            border-radius: 12px;
            padding: 20px;
            box-shadow: 0 4px 6px -1px rgba(0, 0, 0, 0.05);
            border: 1px solid var(--border-color);
            box-sizing: border-box;
            height: 400px;
        }

        #debug-panel {
            width: 100%;
            background: #000;
            color: #0f0;
            font-family: monospace;
            font-size: 11px;
            padding: 10px;
            border-radius: 6px;
            box-sizing: border-box;
            height: 80px;
            overflow-y: auto;
        }
    </style>
</head>
<body>

    <div class="dashboard-container">
        <header>
            <h1>Digestomatic</h1>
            <div class="header-controls">
                <div id="time-display">Loading...</div>
                <div class="theme-switch-wrapper">
                    <span>Light</span>
                    <label class="theme-switch" for="checkbox">
                        <input type="checkbox" id="checkbox" checked />
                        <div class="slider"></div>
                    </label>
                    <span>Dark</span>
                </div>
            </div>
        </header>

        <main class="main-layout">

            <section class="telemetry-column">
                <div class="metrics-grid">
                    <div class="card temp-card">
                        <div class="card-title">Temperature</div>
                        <div class="card-value"><span id="temp">--</span>°C</div>
                    </div>
                    <div class="card methane-card">
                        <div class="card-title">Methane (CH₄)</div>
                        <div class="card-value"><span id="methane">--</span> ppm</div>
                    </div>
                </div>

                <div class="chart-container">
                    <canvas id="liveChart"></canvas>
                </div>

                <div id="debug-panel">System Initializing...<br></div>
            </section>

            <section class="sidebar-column">
                <div class="card ai-status-card">
                    <div class="card-title">✨ AI Insights &amp; Status</div>
                    <div class="ai-log-display" id="ai-logs"></div>
                </div>

                <div class="card command-card">
                    <div class="card-title">Send Command to Digester</div>
                    <div class="command-input-group">
                        <input type="text" id="cmd-field" placeholder="e.g., /vent_gas, /heat_on..." onkeydown="if(event.key === 'Enter') sendCommand()"/>
                        <button onclick="sendCommand()">Send</button>
                    </div>
                </div>
            </section>

        </main>
    </div>

    <script>
        let sensorChart = null;
        let mqttClient = null;
        const MAX_DATA_POINTS = 20;

        let currentTemp = 0.0;
        let currentMethane = 0;

        const MQTT_BROKER = "broker.hivemq.com";
        const MQTT_PORT = 8000;
        const MQTT_CLIENT_ID = "digestomatic_ui_" + Math.random().toString(16).substr(2, 8);

        const TOPIC_TEMP = "digester/sensors/temperature";
        const TOPIC_METHANE = "digester/sensors/methane";
        const TOPIC_COMMANDS = "digester/control/commands";

        function logDebug(msg) {
            const panel = document.getElementById('debug-panel');
            panel.innerHTML += `[${new Date().toLocaleTimeString()}] ${msg}<br>`;
            panel.scrollTop = panel.scrollHeight;
        }

        window.onload = function() {
            logDebug("Digestomatic UI Loaded.");

            if (typeof Chart === 'undefined' || typeof Paho === 'undefined') {
                logDebug("ERROR: External scripts failed to load correctly.");
                return;
            }

            const toggleSwitch = document.querySelector('.theme-switch input[type="checkbox"]');
            toggleSwitch.addEventListener('change', switchTheme);

            initChart();
            setInterval(updateTime, 1000);
            initMQTT();

            generateMockAIInsight("System online. Waiting for live MQTT broker stream data to map stability metrics...");
        };

        function updateTime() {
            const now = new Date();
            document.getElementById('time-display').innerText = now.toLocaleTimeString();
        }

        function initMQTT() {
            logDebug(`Connecting to MQTT Broker: ws://${MQTT_BROKER}:${MQTT_PORT}...`);
            mqttClient = new Paho.MQTT.Client(MQTT_BROKER, Number(MQTT_PORT), MQTT_CLIENT_ID);

            mqttClient.onConnectionLost = onConnectionLost;
            mqttClient.onMessageArrived = onMessageArrived;

            const options = {
                onSuccess: onConnectSuccess,
                onFailure: onConnectFailure,
                useSSL: false
            };

            mqttClient.connect(options);
        }

        function onConnectSuccess() {
            logDebug("Connected to MQTT Broker successfully!");
            mqttClient.subscribe(TOPIC_TEMP);
            mqttClient.subscribe(TOPIC_METHANE);
            logDebug("Subscribed to telemetry data stream channels.");
        }

        function onConnectFailure(err) {
            logDebug(`MQTT Connection Failed: ${err.errorMessage}. Retrying in 5s...`);
            setTimeout(initMQTT, 5000);
        }

        function onConnectionLost(responseObject) {
            if (responseObject.errorCode !== 0) {
                logDebug(`MQTT Connection Lost: ${responseObject.errorMessage}. Reconnecting...`);
                setTimeout(initMQTT, 5000);
            }
        }

        function onMessageArrived(message) {
            const topic = message.destinationName;
            const payload = message.payloadString;
            const floatVal = parseFloat(payload);

            const timestamp = new Date().toLocaleTimeString([], { hour: '2-digit', minute: '2-digit', second: '2-digit' });
            let dataUpdated = false;

            if (topic === TOPIC_TEMP) {
                currentTemp = floatVal;
                document.getElementById('temp').innerText = currentTemp.toFixed(1);
                dataUpdated = true;
            } else if (topic === TOPIC_METHANE) {
                currentMethane = parseInt(payload);
                document.getElementById('methane').innerText = currentMethane;
                dataUpdated = true;
            }

            if (dataUpdated && sensorChart) {
                if (sensorChart.data.labels.length === 0 || sensorChart.data.labels[sensorChart.data.labels.length - 1] !== timestamp) {
                    if (sensorChart.data.labels.length >= MAX_DATA_POINTS) {
                        sensorChart.data.labels.shift();
                        sensorChart.data.datasets[0].data.shift();
                        sensorChart.data.datasets[1].data.shift();
                    }
                    sensorChart.data.labels.push(timestamp);
                    sensorChart.data.datasets[0].data.push(currentTemp);
                    sensorChart.data.datasets[1].data.push(currentMethane);
                } else {
                    sensorChart.data.datasets[0].data[sensorChart.data.datasets[0].data.length - 1] = currentTemp;
                    sensorChart.data.datasets[1].data[sensorChart.data.datasets[1].data.length - 1] = currentMethane;
                }
                sensorChart.update();
            }
        }

        function sendCommand() {
            const field = document.getElementById('cmd-field');
            const cmd = field.value.trim();
            if (!cmd) return;

            if (!mqttClient || !mqttClient.isConnected()) {
                logDebug("ERROR: Cannot dispatch command. MQTT Client is offline.");
                return;
            }

            const message = new Paho.MQTT.Message(cmd);
            message.destinationName = TOPIC_COMMANDS;
            mqttClient.send(message);

            logDebug(`[MQTT PUBLISH] Sent: "${cmd}" to topic "${TOPIC_COMMANDS}"`);
            generateMockAIInsight(`💻 Command dispatched out to system nodes: "${cmd}".`);
            field.value = '';
        }

        function generateMockAIInsight(customText = null) {
            const container = document.getElementById('ai-logs');
            const timestamp = new Date().toLocaleTimeString([], { hour: '2-digit', minute: '2-digit', second: '2-digit' });

            let entry = document.createElement('div');
            entry.className = 'ai-status-entry';
            entry.innerHTML = `<span class="ai-time">${timestamp}</span> <span>${customText || "Awaiting LLM data interpretation cycle."}</span>`;

            container.insertBefore(entry, container.firstChild);
        }

        function switchTheme(e) {
            const theme = e.target.checked ? 'dark' : 'light';
            document.documentElement.setAttribute('data-theme', theme);
            logDebug(`Theme context reassigned: ${theme}`);
        }

        function initChart() {
            try {
                const ctx = document.getElementById('liveChart').getContext('2d');
                sensorChart = new Chart(ctx, {
                    type: 'line',
                    data: {
                        labels: [],
                        datasets: [
                            {
                                label: 'Temperature (°C)',
                                data: [],
                                borderColor: '#ef4444',
                                backgroundColor: 'rgba(239, 68, 68, 0.1)',
                                yAxisID: 'yTemp',
                                tension: 0.3,
                                borderWidth: 2
                            },
                            {
                                label: 'Methane (ppm)',
                                data: [],
                                borderColor: '#10b981',
                                backgroundColor: 'rgba(16, 185, 129, 0.1)',
                                yAxisID: 'yMethane',
                                tension: 0.3,
                                borderWidth: 2
                            }
                        ]
                    },
                    options: {
                        responsive: true,
                        maintainAspectRatio: false,
                        interaction: { mode: 'index', intersect: false },
                        scales: {
                            x: {
                                grid: { color: () => getComputedStyle(document.documentElement).getPropertyValue('--border-color').trim() },
                                ticks: { color: () => getComputedStyle(document.documentElement).getPropertyValue('--text-muted').trim() }
                            },
                            yTemp: {
                                type: 'linear',
                                display: true,
                                position: 'left',
                                title: { display: true, text: 'Temperature (°C)', color: '#ef4444' },
                                grid: { color: () => getComputedStyle(document.documentElement).getPropertyValue('--border-color').trim() },
                                ticks: { color: () => getComputedStyle(document.documentElement).getPropertyValue('--text-muted').trim() }
                            },
                            yMethane: {
                                type: 'linear',
                                display: true,
                                position: 'right',
                                title: { display: true, text: 'Methane (ppm)', color: '#10b981' },
                                grid: { drawOnChartArea: false },
                                ticks: { color: () => getComputedStyle(document.documentElement).getPropertyValue('--text-muted').trim() }
                            }
                        },
                        plugins: {
                            legend: { labels: { color: () => getComputedStyle(document.documentElement).getPropertyValue('--text-main').trim() } }
                        }
                    }
                });
            } catch (err) {
                console.error(err);
            }
        }
    </script>
</body>
</html>
)rawliteral";

void setupWifi() {
    delay(10);
    Serial.println();
    Serial.print("Connecting to Wi-Fi network: ");
    Serial.println(ssid);

    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("");
    Serial.println("Wi-Fi connected successfully!");
    Serial.print("Local Dashboard IP Address: http://");
    Serial.println(WiFi.localIP());
}

void reconnectMQTT() {
    while (!mqttClient.connected()) {
        Serial.print("Attempting MQTT broker connection...");
        String clientId = "ESP32DigesterClient-";
        clientId += String(random(0xffff), HEX);

        if (mqttClient.connect(clientId.c_str())) {
            Serial.println("connected to MQTT broker!");
        } else {
            Serial.print("failed, rc=");
            Serial.print(mqttClient.state());
            Serial.println(" trying again in 5 seconds");
            delay(5000);
        }
    }
}

void setup() {
    Serial.begin(115200);
    Serial.println("--- Initializing Multi-Sensor & UI System ---");

    initDHT();
    initDS18B20();

    setupWifi();

    mqttClient.setServer(mqtt_server, mqtt_port);

    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(200, "text/html", index_html);
    });

    server.begin();
    Serial.println("Web server active and operational.");
}

void loop() {
    if (!mqttClient.connected()) {
        reconnectMQTT();
    }
    mqttClient.loop();

    if (sensorTimer.isReady()) {
        // 1. Fetch physical DHT11 ambient temperature
        DHTData dhtData = readDHT();

        // 2. Fetch physical DS18B20 core probe temperature
        float dsTemp = readDS18B20();

        Serial.print("Ambient: "); Serial.print(dhtData.temperature);
        Serial.print("C | Core: "); Serial.print(dsTemp); Serial.println("C");

        char tempString[8];
        dtostrf(dsTemp, 1, 2, tempString);

        mqttClient.publish(topic_temp, tempString);
    }
}
