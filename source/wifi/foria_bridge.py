import time
import json
import threading
import requests
import paho.mqtt.client as mqtt
from datetime import datetime

from fastapi import FastAPI
from fastapi.middleware.cors import CORSMiddleware
from pydantic import BaseModel
import uvicorn

# --- Configuration ---
# Private HiveMQ Cloud cluster, over the internet via WiFi.
MQTT_BROKER = "62e4f9c4dcc14e309386efc0e76fce89.s1.eu.hivemq.cloud"
MQTT_PORT = 8883       # TLS port
MQTT_USER = "digestomatic"
MQTT_PASS = "digestocomms"

OLLAMA_URL = "http://localhost:11434/api/generate"
MODEL_NAME = "llama3.2"
HTTP_PORT = 8000  # dashboard talks to this over plain HTTP, no websockets needed

# MQTT Topics (must match the prefix used in main.ino)
TOPIC_PREFIX    = "digester/"
TOPIC_TEMP_OUT  = TOPIC_PREFIX + "sensors/temp_outside"
TOPIC_TEMP_IN   = TOPIC_PREFIX + "sensors/temp_internal"
TOPIC_METHANE   = TOPIC_PREFIX + "sensors/methane"
TOPIC_CO2       = TOPIC_PREFIX + "sensors/co2"
TOPIC_COMMANDS  = TOPIC_PREFIX + "control/commands"
TOPIC_AI_REQ    = TOPIC_PREFIX + "ai/request"
TOPIC_AI_RES    = TOPIC_PREFIX + "ai/response"

# Global state to hold latest sensor readings
sensor_data = {
    "digester_temp": "--",
    "ambient_temp": "--",
    "ch4_ppm": "--",
    "co2_ppm": "--",
    "timestamp": "--"
}

mqtt_client = None  # set in mqtt_thread()


def load_system_prompt():
    try:
        with open("foria_system_prompt.md", "r", encoding="utf-8") as f:
            prompt = f.read()
            # Inject live sensor data
            prompt = prompt.replace("{digester_temp}", str(sensor_data["digester_temp"]))
            prompt = prompt.replace("{ambient_temp}", str(sensor_data["ambient_temp"]))
            prompt = prompt.replace("{ch4_ppm}", str(sensor_data["ch4_ppm"]))
            prompt = prompt.replace("{co2_ppm}", str(sensor_data["co2_ppm"]))
            prompt = prompt.replace("{timestamp}", sensor_data["timestamp"])
            return prompt
    except Exception as e:
        print(f"Error loading prompt: {e}")
        return "You are Foria, a smart biodigester assistant."


def ask_ollama(user_prompt):
    system_prompt = load_system_prompt()
    payload = {
        "model": MODEL_NAME,
        "system": system_prompt,
        "prompt": user_prompt,
        "stream": False
    }

    try:
        response = requests.post(OLLAMA_URL, json=payload, timeout=60)
        response.raise_for_status()
        return response.json().get("response", "I'm having trouble thinking right now.")
    except requests.exceptions.RequestException as e:
        print(f"Ollama connection error: {e}")
        return "Error: Could not connect to the local Llama 3.2 model."


# ── MQTT callbacks (sensors + ESP32 stay exactly as before) ─────────────────
def on_connect(client, userdata, flags, rc):
    print("Connected to MQTT broker.")
    client.subscribe([(TOPIC_TEMP_OUT, 0), (TOPIC_TEMP_IN, 0),
                      (TOPIC_METHANE, 0), (TOPIC_CO2, 0),
                      (TOPIC_AI_REQ, 0)])


def on_message(client, userdata, msg):
    topic = msg.topic
    payload = msg.payload.decode('utf-8').strip()
    sensor_data["timestamp"] = datetime.now().strftime("%H:%M:%S")

    if topic == TOPIC_TEMP_OUT:
        sensor_data["ambient_temp"] = payload
    elif topic == TOPIC_TEMP_IN:
        sensor_data["digester_temp"] = payload
    elif topic == TOPIC_METHANE:
        sensor_data["ch4_ppm"] = payload
    elif topic == TOPIC_CO2:
        sensor_data["co2_ppm"] = payload
    elif topic == TOPIC_AI_REQ:
        # Still supported in case anything else publishes AI requests over MQTT
        print(f"Received AI Request via MQTT: {payload}")
        reply = ask_ollama(payload)
        print(f"Foria Reply: {reply}")
        client.publish(TOPIC_AI_RES, reply)


def mqtt_thread():
    global mqtt_client
    mqtt_client = mqtt.Client(client_id="foria_bridge_" + str(time.time()))
    mqtt_client.on_connect = on_connect
    mqtt_client.on_message = on_message

    mqtt_client.tls_set()  # enable TLS for the broker's secure port
    if MQTT_USER:
        mqtt_client.username_pw_set(MQTT_USER, MQTT_PASS)

    print(f"Connecting to {MQTT_BROKER}:{MQTT_PORT} over TLS...")
    mqtt_client.connect(MQTT_BROKER, MQTT_PORT, 60)
    mqtt_client.loop_forever()


# ── HTTP API for the dashboard — plain HTTP only, no websockets ─────────────
app = FastAPI()

app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_methods=["*"],
    allow_headers=["*"],
)


class ChatRequest(BaseModel):
    message: str


class StirRequest(BaseModel):
    duration_sec: int | None = None


@app.get("/sensors")
def get_sensors():
    return sensor_data


@app.post("/chat")
def chat(req: ChatRequest):
    reply = ask_ollama(req.message)
    if mqtt_client:
        mqtt_client.publish(TOPIC_AI_RES, reply)  # keep other MQTT subscribers in sync
    return {"reply": reply}


@app.post("/stir")
def stir(req: StirRequest = StirRequest()):
    if mqtt_client:
        command = f"/stir:{req.duration_sec}" if req.duration_sec else "/stir"
        mqtt_client.publish(TOPIC_COMMANDS, command)
        return {"status": "sent", "duration_sec": req.duration_sec}
    return {"status": "mqtt not connected"}


if __name__ == "__main__":
    t = threading.Thread(target=mqtt_thread, daemon=True)
    t.start()

    print(f"HTTP bridge running on http://0.0.0.0:{HTTP_PORT}")
    uvicorn.run(app, host="0.0.0.0", port=HTTP_PORT)
