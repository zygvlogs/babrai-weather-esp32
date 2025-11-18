#include <WiFi.h>
#include <WebServer.h>
#include <DHT.h>
#include <SPIFFS.h>
#include <esp_task_wdt.h>
#include <time.h>

#define DHTPIN 4
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

WebServer server(80);

// ---- WiFi ----
const char* ssid = "SSID";
const char* password = "PASSWORD";

// ---- Logging ----
const char* LOG_FILE = "/data.log";
const size_t MAX_FILE_SIZE = 550000; 

// ---- Statistika (nuo paleidimo) ----
float minTemp =  999.0;
float maxTemp = -999.0;
float minHum  =  999.0;
float maxHum  = -999.0;

float sumTemp = 0.0;
float sumHum  = 0.0;
unsigned long avgCount = 0;

// =============== LOG WRITE ===============
void writeLog(float t, float h) {
  File f = SPIFFS.open(LOG_FILE, "a");
  if (!f) return;

  unsigned long ts = time(nullptr); 

  String line = "{\"t\":";
  line += String(t, 2);
  line += ",\"h\":";
  line += String(h, 2);
  line += ",\"ts\":";
  line += String(ts);
  line += "}\n";

  f.print(line);
  f.close();
}

// =============== CLEAN OLD LOGS (slankus langas) ===============
void cleanOldLogs() {
  File f = SPIFFS.open(LOG_FILE, "r");
  if (!f) return;

  if (f.size() < MAX_FILE_SIZE) {
    f.close();
    return;
  }

  Serial.println("⚠ Log file big, trimming...");

  File temp = SPIFFS.open("/temp.log", "w");
  if (!temp) {
    f.close();
    return;
  }

  // paliekam naujausią ~50% failo
  f.seek(f.size() / 2);

  while (f.available()) {
    temp.write(f.read());
  }

  f.close();
  temp.close();

  SPIFFS.remove(LOG_FILE);
  SPIFFS.rename("/temp.log", LOG_FILE);

  Serial.println("✔ Trim done.");
}

// =============== SIMPLE JSON PARSER HELPERS ===============
bool extractValue(const String& line, const String& key, String& out) {
  int idx = line.indexOf(key);
  if (idx < 0) return false;
  int start = line.indexOf(':', idx);
  if (start < 0) return false;
  start++;
  int end = line.indexOf(',', start);
  if (end < 0) {
    end = line.indexOf('}', start);
    if (end < 0) return false;
  }
  out = line.substring(start, end);
  out.trim();
  return true;
}

// =============== CSV EXPORT (/csv) ===============
void handleCSV() {
  if (!SPIFFS.exists(LOG_FILE)) {
    server.send(404, "text/plain", "No log file.");
    return;
  }

  File f = SPIFFS.open(LOG_FILE, "r");
  if (!f) {
    server.send(500, "text/plain", "Cannot open file.");
    return;
  }

  String csv = "timestamp,temp,hum\n";

  while (f.available()) {
    String line = f.readStringUntil('\n');
    line.trim();
    if (line.length() == 0) continue;

    String tStr, hStr, tsStr;
    if (!extractValue(line, "\"t\"", tStr)) continue;
    if (!extractValue(line, "\"h\"", hStr)) continue;
    if (!extractValue(line, "\"ts\"", tsStr)) continue;

    csv += tsStr;
    csv += ",";
    csv += tStr;
    csv += ",";
    csv += hStr;
    csv += "\n";
  }

  f.close();
  server.send(200, "text/csv", csv);
}

// =============== LIVE DATA API (/data) ===============
void handleData() {
  float t = dht.readTemperature();
  float h = dht.readHumidity();

  if (isnan(t) || isnan(h)) {
    server.send(500, "text/plain", "Sensor error");
    return;
  }

  // stat update (nuo paleidimo)
  if (t < minTemp) minTemp = t;
  if (t > maxTemp) maxTemp = t;
  if (h < minHum)  minHum  = h;
  if (h > maxHum)  maxHum  = h;

  sumTemp += t;
  sumHum  += h;
  avgCount++;

  unsigned long cnt = (avgCount > 0 ? avgCount : 1);
  float avgT = sumTemp / cnt;
  float avgH = sumHum  / cnt;

  String json = "{";
  json += "\"temp\":"    + String(t, 2) + ",";
  json += "\"hum\":"     + String(h, 2) + ",";
  json += "\"minTemp\":" + String(minTemp, 2) + ",";
  json += "\"maxTemp\":" + String(maxTemp, 2) + ",";
  json += "\"minHum\":"  + String(minHum, 2)  + ",";
  json += "\"maxHum\":"  + String(maxHum, 2)  + ",";
  json += "\"avgTemp\":" + String(avgT, 2)    + ",";
  json += "\"avgHum\":"  + String(avgH, 2);
  json += "}";

  server.send(200, "application/json", json);
}

// =============== RAW LOG (/log) ===============
void handleLog() {
  if (!SPIFFS.exists(LOG_FILE)) {
    server.send(404, "text/plain", "No log file.");
    return;
  }
  File f = SPIFFS.open(LOG_FILE, "r");
  server.streamFile(f, "text/plain");
  f.close();
}

// =============== DASHBOARD UI (/) ===============
void handleRoot() {
  String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<title>Babrai Weather — 12h Dashboard</title>
<style>
body {
  font-family: Arial, sans-serif;
  background: #f2f2f7;
  margin: 0;
  padding: 20px;
  text-align: center;
}
.card {
  background: white;
  padding: 20px;
  margin: 20px auto;
  max-width: 640px;
  border-radius: 14px;
  box-shadow: 0 4px 12px rgba(0,0,0,0.08);
}
h1 {
  margin-top: 0;
  font-size: 28px;
}
canvas {
  width: 100%;
  max-height: 320px;
}
.btn {
  display: inline-block;
  font-size: 16px;
  padding: 10px 16px;
  background: #4CAF50;
  color: white;
  border-radius: 8px;
  text-decoration: none;
}
.stats p {
  margin: 4px 0;
}
</style>
</head>
<body>

<h1>Babrai Weather Dashboard</h1>

<div class="card">
  <h2>Live Data</h2>
  <p><b>Temperature:</b> <span id="temp">--</span> °C</p>
  <p><b>Humidity:</b> <span id="hum">--</span> %</p>
</div>

<div class="card stats">
  <h2>Statistics (since boot)</h2>
  <p><b>Avg Temp:</b> <span id="avgT">--</span> °C</p>
  <p><b>Min Temp:</b> <span id="minT">--</span> °C</p>
  <p><b>Max Temp:</b> <span id="maxT">--</span> °C</p>
  <p><b>Avg Hum:</b> <span id="avgH">--</span> %</p>
  <p><b>Min Hum:</b> <span id="minH">--</span> %</p>
  <p><b>Max Hum:</b> <span id="maxH">--</span> %</p>
  <br>
  <a class="btn" href="/csv">Download CSV</a>
</div>

<div class="card">
  <h2>12h Temperature</h2>
  <canvas id="tempChart"></canvas>
</div>

<div class="card">
  <h2>12h Humidity</h2>
  <canvas id="humChart"></canvas>
</div>

<script src="https://cdn.jsdelivr.net/npm/chart.js"></script>
<script>
let tempData = [];
let humData = [];
let labels = [];

// ---- Chart init ----
const tempCtx = document.getElementById('tempChart').getContext('2d');
const humCtx  = document.getElementById('humChart').getContext('2d');

const tempChart = new Chart(tempCtx, {
  type: 'line',
  data: {
    labels: labels,
    datasets: [{
      label: 'Temperature °C',
      data: tempData,
      borderWidth: 2,
      tension: 0.25
    }]
  }
});

const humChart = new Chart(humCtx, {
  type: 'line',
  data: {
    labels: labels,
    datasets: [{
      label: 'Humidity %',
      data: humData,
      borderWidth: 2,
      tension: 0.25
    }]
  }
});

// ---- Load 12h history from /log ----
async function loadHistory() {
  try {
    const res = await fetch("/log");
    if (!res.ok) return;
    const text = await res.text();
    const lines = text.trim().split("\n");

    lines.forEach(l => {
      try {
        const j = JSON.parse(l);
        const label = new Date(j.ts * 1000).toLocaleTimeString();
        labels.push(label);
        tempData.push(j.t);
        humData.push(j.h);
      } catch(e) {}
    });

    tempChart.update();
    humChart.update();
  } catch(e) {
    console.log("History load error", e);
  }
}

// ---- Live update every 3s ----
async function updateLive() {
  try {
    const res = await fetch("/data");
    if (!res.ok) return;
    const j = await res.json();

    document.getElementById("temp").textContent = j.temp;
    document.getElementById("hum").textContent  = j.hum;

    // stats
    document.getElementById("avgT").textContent = j.avgTemp;
    document.getElementById("minT").textContent = j.minTemp;
    document.getElementById("maxT").textContent = j.maxTemp;
    document.getElementById("avgH").textContent = j.avgHum;
    document.getElementById("minH").textContent = j.minHum;
    document.getElementById("maxH").textContent = j.maxHum;

    const label = new Date().toLocaleTimeString();
    labels.push(label);
    tempData.push(j.temp);
    humData.push(j.hum);

    if (labels.length > 400) {
      labels.shift();
      tempData.shift();
      humData.shift();
    }

    tempChart.update();
    humChart.update();
  } catch(e) {
    console.log("Live update error", e);
  }
}

loadHistory();
updateLive();
setInterval(updateLive, 3000);
</script>

</body>
</html>
)rawliteral";

  server.send(200, "text/html", html);
}

// =============== SETUP ===============
void setup() {
  Serial.begin(115200);
  esp_task_wdt_deinit();       

  dht.begin();
  SPIFFS.begin(true);

  Serial.println("Connecting WiFi...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(300);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());

  configTime(0, 0, "pool.ntp.org", "time.nist.gov");

  server.on("/",     handleRoot);
  server.on("/data", handleData);
  server.on("/log",  handleLog);
  server.on("/csv",  handleCSV);

  server.begin();
  Serial.println("HTTP server started");
}

unsigned long lastWrite = 0;

// =============== LOOP ===============
void loop() {
  server.handleClient();

  if (millis() - lastWrite >= 3000) {
    lastWrite = millis();

    float t = dht.readTemperature();
    float h = dht.readHumidity();

    if (!isnan(t) && !isnan(h)) {
      writeLog(t, h);
      cleanOldLogs();
    }
  }
}
