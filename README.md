## Overview

Digestomatic is a smart biodigester prototype that integrates **IoT, AI, and automation** to improve the usability and gas output of home-scale anaerobic digesters. Using an **ESP32**, dedicated gas and temperature sensors, and a locally hosted web dashboard, the system continuously monitors conditions inside the digester. A local LLM (**Foria**) interprets the data and answers user questions, while an automated DC motor stirrer keeps the substrate mixed to maximize methane production.

Biodigesters solve two problems at once: they divert organic waste from dumping/burning, and they produce renewable gas. Most existing biodigesters, however, are **passive** — no monitoring, no control, no visibility into what's happening inside the tank. Digestomatic changes that.

## Features

- Real-time CH₄ and CO₂ concentration + temperature monitoring
- Web dashboard with live sensor widgets and historical charts
- **Foria AI** — local LLM assistant for performance interpretation and Q&A
- Automatic stirrer mechanism (with manual override)
- Optimized methane production via timed stirring
- Downloadable CSV measurement logs
- Light/dark mode dashboard
- Modular, off-grid-capable design (local MQTT + local LLM)

## System Architecture


**Hardware:** ESP32, DHT11 (ambient temp), DS18B20 (digester temp), MQ-4 (methane), MQ-135 (CO₂), DC motor + L298N driver for stirring.

**Software stack:** Arduino/C++ firmware, HTTP/MQTT communication (HiveMQ Cloud), Python FastAPI bridge (Foria), SPIFFS-hosted HTML/JS/CSS dashboard, local LLM via Ollama (Llama 3.2).


## Getting Started

1. Flash `/source/"local comms"/` (all .ino files and the data folder) to an ESP32 using Arduino IDE (see required libraries below).
2. Configure WiFi + MQTT broker credentials in `main.ino`.
3. Run `foria_bridge.py` locally (requires [Ollama](https://ollama.ai) with `llama3.2` pulled).
4. Open the dashboard served by the ESP32, or access it directly from `/dashboard`.

### Required Libraries
- Adafruit Unified Sensor
- Async TCP
- DHT Sensor Library
- DallasTemperature
- ESP AsyncWebServer
- MQUnifiedsensor
- OneWire
- PubSubClient

See the libraries folder for the .zip files.


## Team Eudaimonia

*Eudaimonia (εὐδαιμονία)* — Greek for "human flourishing." Our team consists of 5 AdeKUS students, Mechanical & Electrical Engineering.

| Name | Role |
|---|---|
| Jozua Margaret | Artist / Designer & Mechanical Engineer |
| Riaasat Salarbux | Team Leader & All-Rounder |
| Vaughn Sprangers | Front-end Developer & Mechanical Engineer |
| Christopher Tekam | Electrical Design Lead & Creative Director |
| Elijah Wongsonadi | Hardware & Back-end Technician |

## Credits

Special thanks to Mr. Samuel Kuik (SlimGas N.V.), Mr. Stefanito Soerotomo, Mr. H. Sariman, Mr. A Dasoe, Mr. C. Kartopawiro, Ms. Julie Sundar, Mr. Theo Boomsma for their guidance during prototyping.

## License

<!-- Add license info here, e.g. MIT -->

---

<p align="center"><i>Built for Hackomation 2026 — turning organic waste into renewable energy, one digester at a time.</i></p>
