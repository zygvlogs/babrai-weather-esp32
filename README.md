# Babrai Weather Station (ESP32 + DHT11 + Web Dashboard)

A compact ESP32-powered weather station featuring:

- **DHT11 temperature & humidity sensor**
- **Real-time live API (`/data`)**
- **12-hour graph dashboard (Chart.js)**
- **CSV export (`/csv`)**
- **Local JSON log storage in SPIFFS**
- **Automatic log trimming (sliding window)**
- **WiFi + NTP time synchronization**

This project runs a self-hosted HTTP dashboard directly on an ESP32 — no cloud required.

---

## ⚡ Features

### 📡 Live Data API

Endpoint `/data` returns:

```json
{
  "temp": 21.50,
  "hum": 45.20,
  "minTemp": 19.10,
  "maxTemp": 22.70,
  "minHum": 40.20,
  "maxHum": 51.80,
  "avgTemp": 20.75,
  "avgHum": 45.90
}
```

### 📈 Built-in Web Dashboard

- Live temperature & humidity  
- Statistics (min, max, average since boot)  
- 12h history graph loaded from SPIFFS logs  
- Two interactive Chart.js graphs  
- CSV export button  

### 📝 Logging

Every **3 seconds**, the ESP32 writes a JSON line to SPIFFS, e.g.:

```json
{"t":21.50,"h":45.20,"ts":1731841234}
```

Automatic trimming keeps the newest 50% when the log exceeds ~550 KB.

---

## 🛠 Hardware Required

| Component        | Notes                    |
|-----------------|--------------------------|
| ESP32 Dev Board | Any WROOM/WROVER module |
| DHT11 Sensor    | Data pin → GPIO 4        |
| 10k pull-up     | On DHT11 data pin        |
| WiFi network    | Required                 |
| USB Cable       | For flashing             |

---

## 🔧 Wiring

| DHT11 Pin | Connect to |
|----------|------------|
| VCC      | 3.3V       |
| GND      | GND        |
| DATA     | **GPIO 4** |

---

## 📦 Installation

### 1. Install Arduino Libraries

Make sure you have:

- **ESP32 board package**
- **DHT sensor library**
- **Adafruit Unified Sensor**
- **SPIFFS filesystem support**

### 2. Configure WiFi

Set your WiFi credentials in the code:

```cpp
const char* ssid = "SSID";
const char* password = "PASSWORD";
```

### 3. Upload the Sketch

Select your ESP32 board in Arduino IDE and upload the firmware.

### 4. Get the Dashboard URL

Open **Serial Monitor** at 115200 baud:

You should see:

```text
Connecting WiFi...
WiFi connected
IP: 192.168.x.x
HTTP server started
```

Open that IP in your browser.

---

## 🌐 API Endpoints

| Endpoint | Description            | Format |
|----------|------------------------|--------|
| `/`      | Dashboard UI           | HTML   |
| `/data`  | Live data + statistics | JSON   |
| `/log`   | Raw log file           | NDJSON |
| `/csv`   | Export logs            | CSV    |

---

## 🖥 Dashboard

The web dashboard shows:

- **Live readings** (temperature & humidity)
- **Min / Max / Average** since device boot
- **Rolling 12-hour history** based on stored log
- Two **Chart.js** line graphs (temperature & humidity)
- **Download CSV** button

Live values and charts are updated every **3 seconds** using `/data`.

---

## 📁 File Structure (SPIFFS)

The firmware uses the internal SPIFFS filesystem:

- ` /data.log` — main append-only log file (NDJSON)
- ` /temp.log` — temporary file used when trimming logs

Each log entry looks like:

```json
{"t":21.50,"h":45.20,"ts":1731841234}
```

---

## 🧠 Log Trimming Logic

To avoid running out of flash space, the firmware keeps the log size under **550 KB**:

1. When `data.log` exceeds `MAX_FILE_SIZE` (~550 KB), trimming is triggered.
2. The file pointer seeks to the middle of the file.
3. The **newest half** of the data is copied into `/temp.log`.
4. `data.log` is deleted and `/temp.log` is renamed to `data.log`.
5. Logging continues as normal.

This effectively maintains a sliding window of recent log data (roughly ~12 hours depending on interval and JSON length).

---

## 🧩 JSON → CSV Conversion

The `/csv` endpoint:

1. Reads each line from `data.log`.
2. Extracts `t` (temperature), `h` (humidity) and `ts` (timestamp).
3. Produces a CSV like:

```csv
timestamp,temp,hum
1731841000,21.40,44.00
1731841030,21.50,45.20
...
```

You can download it directly from the browser and import into Excel, Google Sheets, or any data analysis tool.

---

## ⏱ Time Synchronization

The device uses NTP for real timestamps:

- `pool.ntp.org`
- `time.nist.gov`

The Unix timestamp (`ts`) is stored with each log entry and converted to local time in the browser using JavaScript:

```js
new Date(j.ts * 1000).toLocaleTimeString();
```

---

## 🚀 Future Ideas

Some possible improvements:

- Support for **DHT22** or other sensors (BME280, etc.)
- **Dark mode** toggle for the dashboard
- **MQTT** integration (publish live data to broker)
- **WebSocket**-based real-time updates
- **SD card** for long-term data storage
- **OTA updates** (Over-The-Air firmware upgrade)
- Comparison with external APIs (OpenWeatherMap, etc.)

---

## 📄 License

Released under the **MIT License**.  
Feel free to use, modify, and share this project.
