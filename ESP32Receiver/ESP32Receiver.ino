#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <ArduinoJson.h>

// Wi-Fi Access Point Credentials
const char* ssid = "K12 Satellite";
const char* password = "K12 Satellite";

// DNS Server for Captive Portal (Port 53)
const byte DNS_PORT = 53;
DNSServer dnsServer;

// Standard WebServer on port 80
WebServer server(80);

// RF Module connected to Serial2
#define RXD2 33
#define TXD2 32

// Global variable to store the latest JSON string from the Transmitter
String latestJson = "{\"temp\":0,\"hum\":0,\"press\":0,\"gas\":0,\"roll\":0,\"pitch\":0,\"magX\":0,\"magY\":0,\"magZ\":0}";

// Embedded HTML/CSS/JS (High-Tech Satellite Telemetry UI)
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>CanSat Telemetry System</title>
  <style>
    @import url('https://fonts.googleapis.com/css2?family=Share+Tech+Mono&display=swap');
    
    :root {
      --bg-color: #030a16;
      --panel-bg: rgba(16, 33, 58, 0.7);
      --accent-color: #00ffea;
      --text-main: #e2f1ff;
      --text-dim: #7393b3;
      --border-color: #005a8f;
    }
    
    body { 
      background-color: var(--bg-color); 
      background-image: 
        linear-gradient(rgba(0, 255, 234, 0.05) 1px, transparent 1px),
        linear-gradient(90deg, rgba(0, 255, 234, 0.05) 1px, transparent 1px);
      background-size: 20px 20px;
      color: var(--text-main); 
      font-family: 'Share Tech Mono', monospace; 
      margin: 0; 
      padding: 20px; 
    }
    
    .header {
      display: flex;
      justify-content: space-between;
      align-items: center;
      border-bottom: 2px solid var(--border-color);
      padding-bottom: 15px;
      margin-bottom: 30px;
    }
    
    .header h1 { 
      margin: 0; 
      font-size: 28px; 
      color: var(--accent-color);
      text-shadow: 0 0 10px rgba(0, 255, 234, 0.5);
      text-transform: uppercase;
      letter-spacing: 2px;
    }
    
    .status-indicator {
      display: flex;
      align-items: center;
      gap: 10px;
      font-size: 14px;
      color: var(--accent-color);
    }
    
    .dot {
      height: 10px;
      width: 10px;
      background-color: #00ff00;
      border-radius: 50%;
      box-shadow: 0 0 10px #00ff00;
      animation: blink 1s infinite;
    }
    
    @keyframes blink {
      0% { opacity: 1; }
      50% { opacity: 0.3; }
      100% { opacity: 1; }
    }
    
    .grid { 
      display: grid; 
      grid-template-columns: repeat(auto-fit, minmax(240px, 1fr)); 
      gap: 20px; 
    }
    
    .card { 
      background: var(--panel-bg); 
      padding: 20px; 
      border: 1px solid var(--border-color);
      border-left: 4px solid var(--accent-color);
      position: relative;
      box-shadow: inset 0 0 20px rgba(0, 0, 0, 0.5);
      display: flex;
      flex-direction: column;
    }
    
    .card::before, .card::after {
      content: '';
      position: absolute;
      width: 10px;
      height: 10px;
      border: 1px solid var(--accent-color);
    }
    .card::before { top: -1px; right: -1px; border-left: none; border-bottom: none; }
    .card::after { bottom: -1px; right: -1px; border-left: none; border-top: none; }
    
    .card h3 { 
      margin: 0 0 5px 0; 
      font-size: 12px; 
      color: var(--text-dim); 
      text-transform: uppercase; 
      letter-spacing: 1px;
    }
    
    .val-container {
      display: flex;
      align-items: baseline;
      margin-bottom: 10px;
    }
    
    .val { 
      font-size: 32px; 
      color: var(--accent-color); 
      text-shadow: 0 0 8px rgba(0, 255, 234, 0.3);
    }
    
    .unit {
      font-size: 14px;
      color: var(--text-dim);
      margin-left: 5px;
    }
    
    canvas {
      width: 100%;
      height: 60px;
      border-bottom: 1px solid rgba(0, 255, 234, 0.1);
      margin-top: auto;
    }
    
    .section-title {
      grid-column: 1 / -1;
      margin-top: 20px;
      margin-bottom: 5px;
      color: var(--text-dim);
      font-size: 14px;
      text-transform: uppercase;
      border-bottom: 1px dashed var(--border-color);
      padding-bottom: 5px;
    }
  </style>
</head>
<body>
  <div class="header">
    <h1>CanSat // Mission Control</h1>
    <div class="status-indicator">
      <div class="dot"></div>
      LINK ACTIVE
    </div>
  </div>
  
  <div class="grid">
    <div class="section-title">Attitude Control (MPU-6050)</div>
    <div class="card">
      <h3>Pitch</h3>
      <div class="val-container"><div class="val"><span id="pitch">--</span><span class="unit">&deg;</span></div></div>
      <canvas id="canvas-pitch" width="200" height="60"></canvas>
    </div>
    <div class="card">
      <h3>Roll</h3>
      <div class="val-container"><div class="val"><span id="roll">--</span><span class="unit">&deg;</span></div></div>
      <canvas id="canvas-roll" width="200" height="60"></canvas>
    </div>
    
    <div class="section-title">Environment (BME-688)</div>
    <div class="card">
      <h3>Temperature</h3>
      <div class="val-container"><div class="val"><span id="temp">--</span><span class="unit">&deg;C</span></div></div>
      <canvas id="canvas-temp" width="200" height="60"></canvas>
    </div>
    <div class="card">
      <h3>Humidity</h3>
      <div class="val-container"><div class="val"><span id="hum">--</span><span class="unit">%</span></div></div>
      <canvas id="canvas-hum" width="200" height="60"></canvas>
    </div>
    <div class="card">
      <h3>Pressure</h3>
      <div class="val-container"><div class="val"><span id="press">--</span><span class="unit">hPa</span></div></div>
      <canvas id="canvas-press" width="200" height="60"></canvas>
    </div>
    <div class="card">
      <h3>Gas Resistance</h3>
      <div class="val-container"><div class="val"><span id="gas">--</span><span class="unit">k&Omega;</span></div></div>
      <canvas id="canvas-gas" width="200" height="60"></canvas>
    </div>
    
    <div class="section-title">Geomagnetic (BMM-350)</div>
    <div class="card">
      <h3>Mag X</h3>
      <div class="val-container"><div class="val"><span id="magX">--</span><span class="unit">&micro;T</span></div></div>
      <canvas id="canvas-magX" width="200" height="60"></canvas>
    </div>
    <div class="card">
      <h3>Mag Y</h3>
      <div class="val-container"><div class="val"><span id="magY">--</span><span class="unit">&micro;T</span></div></div>
      <canvas id="canvas-magY" width="200" height="60"></canvas>
    </div>
    <div class="card">
      <h3>Mag Z</h3>
      <div class="val-container"><div class="val"><span id="magZ">--</span><span class="unit">&micro;T</span></div></div>
      <canvas id="canvas-magZ" width="200" height="60"></canvas>
    </div>
  </div>
  
  <script>
    const MAX_POINTS = 30;
    const historyData = {
      pitch: [], roll: [], temp: [], hum: [], press: [], gas: [], magX: [], magY: [], magZ: []
    };

    function drawGraph(key) {
      const canvas = document.getElementById('canvas-' + key);
      const ctx = canvas.getContext('2d');
      const width = canvas.width;
      const height = canvas.height;
      const data = historyData[key];
      
      ctx.clearRect(0, 0, width, height);
      if (data.length < 2) return;
      
      let min = Math.min(...data);
      let max = Math.max(...data);
      if (min === max) { min -= 1; max += 1; }
      let range = max - min;
      
      ctx.beginPath();
      ctx.strokeStyle = '#00ffea';
      ctx.lineWidth = 2;
      ctx.lineJoin = 'round';
      
      for (let i = 0; i < data.length; i++) {
        const x = (i / (MAX_POINTS - 1)) * width;
        const y = height - ((data[i] - min) / range) * height * 0.8 - (height * 0.1);
        if (i === 0) ctx.moveTo(x, y);
        else ctx.lineTo(x, y);
      }
      ctx.stroke();
      
      // Draw subtle gradient under the line
      ctx.lineTo(width, height);
      ctx.lineTo(0, height);
      ctx.closePath();
      let gradient = ctx.createLinearGradient(0, 0, 0, height);
      gradient.addColorStop(0, 'rgba(0, 255, 234, 0.2)');
      gradient.addColorStop(1, 'rgba(0, 255, 234, 0)');
      ctx.fillStyle = gradient;
      ctx.fill();
    }

    function processData(key, value) {
      document.getElementById(key).innerText = value.toFixed(1);
      historyData[key].push(value);
      if (historyData[key].length > MAX_POINTS) historyData[key].shift();
      drawGraph(key);
    }

    setInterval(() => {
      fetch('/data')
        .then(response => response.json())
        .then(data => {
          processData('pitch', data.pitch);
          processData('roll', data.roll);
          processData('temp', data.temp);
          processData('hum', data.hum);
          document.getElementById('press').innerText = data.press.toFixed(2); // override display format
          historyData['press'].push(data.press);
          if (historyData['press'].length > MAX_POINTS) historyData['press'].shift();
          drawGraph('press');
          processData('gas', data.gas);
          processData('magX', data.magX);
          processData('magY', data.magY);
          processData('magZ', data.magZ);
        })
        .catch(error => console.error("Telemetry link lost:", error));
    }, 500);
  </script>
</body>
</html>
)rawliteral";

void setup() {
  delay(1000); // Wait for serial monitor
  Serial.begin(115200);
  Serial2.begin(9600, SERIAL_8N1, RXD2, TXD2);

  Serial.println("\nInitializing ESP32 Receiver Dashboard...");

  // Setup Wi-Fi AP
  WiFi.mode(WIFI_AP);
  WiFi.softAP(ssid, password);
  Serial.print("AP IP address: ");
  Serial.println(WiFi.softAPIP());
  
  // Start DNS server for captive portal (Redirects ALL domain names to the ESP32 IP)
  dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());

  // Serve the main HTML page
  server.on("/", HTTP_GET, []() {
    server.send(200, "text/html", index_html);
  });

  // Serve the latest JSON data endpoint
  server.on("/data", HTTP_GET, []() {
    server.send(200, "application/json", latestJson);
  });
  
  // Captive Portal Redirect: If the phone asks for any other URL, redirect it to our IP!
  server.onNotFound([]() {
    server.sendHeader("Location", String("http://") + WiFi.softAPIP().toString(), true);
    server.send(302, "text/plain", "");
  });

  // Start the standard web server
  server.begin();
  Serial.println("Web server started successfully!");
}

void loop() {
  // Process DNS requests to force the captive portal popup
  dnsServer.processNextRequest();
  
  // Handle web clients
  server.handleClient();
  
  // Check for incoming RF data
  if (Serial2.available()) {
    String incomingJson = Serial2.readStringUntil('\n');
    incomingJson.trim();
    
    if (incomingJson.length() > 0) {
      // Validate JSON (using larger buffer for new fields)
      StaticJsonDocument<512> doc; 
      DeserializationError error = deserializeJson(doc, incomingJson);
      
      if (!error) {
        // If valid, save it to the global variable so the web server can send it
        latestJson = incomingJson;
        Serial.print("Received valid telemetry: ");
        Serial.println(latestJson);
      } else {
        Serial.println("JSON parse error from RF module");
      }
    }
  }
  
}