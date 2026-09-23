#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <ArduinoJson.h>

// Wi-Fi Access Point Credentials
const char* ssid     = "K12 Satellite";
const char* password = "K12 Satellite";

// DNS Server for Captive Portal (Port 53)
const byte DNS_PORT = 53;
DNSServer dnsServer;

// Standard 100% Rock-Solid ESP32 WebServer on port 80
WebServer server(80);

// RF Module connected to Serial2
#define RXD2 33
#define TXD2 32

// Default JSON — temp, press, alt, roll, pitch, heading
String latestJson = "{\"temp\":0,\"press\":0,\"alt\":0,\"roll\":0,\"pitch\":0,\"heading\":0}";
unsigned long lastRxTime = 0;
const unsigned long LINK_TIMEOUT_MS = 6000; // 6 seconds silence timeout
float baselineAltitude = -999999.0;
bool baselineCaptured = false;

// Lightweight Gateway Landing Portal (<1.5 KB, zero RAM impact)
const char landing_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>K12-SAT // Ground Station Gateway</title>
  <style>
    body {
      background: #030a16;
      color: #e2f1ff;
      font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, monospace;
      margin: 0; padding: 24px;
      display: flex; flex-direction: column; align-items: center; justify-content: center;
      min-height: 90vh; text-align: center;
    }
    .card {
      background: rgba(16, 33, 58, 0.9);
      border: 1px solid #00ffea;
      box-shadow: 0 0 24px rgba(0,255,234,0.25);
      border-radius: 8px;
      padding: 28px 24px;
      max-width: 480px; width: 100%; box-sizing: border-box;
    }
    h1 { color: #00ffea; font-size: 22px; margin: 0 0 12px 0; letter-spacing: 1.5px; }
    p { font-size: 14px; line-height: 1.6; color: #8ba9c9; margin: 0 0 18px 0; }
    .badge {
      display: inline-block; background: rgba(0,255,102,0.15); border: 1px solid #00ff66;
      color: #00ff66; padding: 4px 12px; border-radius: 4px; font-size: 12px; margin-bottom: 20px;
    }
    .btn {
      display: block; width: 100%; box-sizing: border-box;
      background: #00ffea; color: #030a16; text-decoration: none;
      padding: 12px 18px; border-radius: 4px; font-weight: bold; font-size: 14px;
      letter-spacing: 1px; text-transform: uppercase; margin-bottom: 12px;
      transition: all 0.2s ease;
    }
    .btn:hover { background: #55fff0; box-shadow: 0 0 14px #00ffea; }
    .btn-secondary {
      background: transparent; border: 1px solid #005a8f; color: #8ba9c9;
    }
    .btn-secondary:hover { background: #005a8f; color: #fff; }
    .status-box {
      font-size: 12px; color: #8ba9c9; background: #071529; border: 1px dashed #005a8f;
      padding: 10px; border-radius: 4px; margin-top: 14px; text-align: left;
    }
  </style>
</head>
<body>
  <div class="card">
    <div class="badge">● GATEWAY ONLINE (DUAL STREAM: HTTP + USB)</div>
    <h1>🛰️ K12-SAT // GROUND STATION</h1>
    <p>This ESP32 is running as a lightweight RF Data Bridge. 3D assets are rendered locally on your device GPU via the PWA app.</p>
    <a href="/data" class="btn btn-secondary">View Live JSON Raw Data (/data)</a>
    <div class="status-box">
      <strong>Field Operation Quick-Start:</strong><br>
      1. Open your saved <strong>K12-SAT Mission Control PWA</strong> app.<br>
      2. If using Wi-Fi, it polls <code>http://192.168.4.1/data</code>.<br>
      3. If using USB Cable, click <strong>🔌 USB</strong> in the top bar to connect instantly.
    </div>
  </div>
</body>
</html>
)rawliteral";

void setup() {
  delay(100);
  Serial.begin(115200);
  Serial2.begin(9600, SERIAL_8N1, RXD2, TXD2);
  Serial2.setTimeout(50); // Snappy 50ms read timeout prevents blocking

  Serial.println("\nInitializing ESP32 Ground Station Gateway...");

  // Setup Wi-Fi Access Point
  WiFi.mode(WIFI_AP);
  WiFi.softAP(ssid, password);
  Serial.print("AP IP address: ");
  Serial.println(WiFi.softAPIP());

  // Captive Portal DNS — any domain → ESP32
  dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());

  auto handleLanding = []() {
    server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
    server.send_P(200, "text/html", landing_html, sizeof(landing_html) - 1);
  };

  // Serve landing page
  server.on("/", HTTP_GET, handleLanding);
  server.on("/index.html", HTTP_GET, handleLanding);
  server.on("/generate_204", HTTP_GET, handleLanding);        // Android captive portal
  server.on("/gen_204", HTTP_GET, handleLanding);             // Android captive portal
  server.on("/hotspot-detect.html", HTTP_GET, handleLanding); // Apple/iOS captive portal
  server.on("/canonical.html", HTTP_GET, handleLanding);      // Android captive portal
  server.on("/connecttest.txt", HTTP_GET, handleLanding);     // Windows captive portal
  server.on("/ncsi.txt", HTTP_GET, handleLanding);            // Windows captive portal

  // Serve live JSON telemetry endpoint with full CORS headers
  server.on("/data", HTTP_GET, []() {
    unsigned long nowMs = millis();
    unsigned long silenceMs = (lastRxTime == 0) ? nowMs : (nowMs - lastRxTime);
    bool isConnected = (silenceMs < LINK_TIMEOUT_MS);

    String response = latestJson;
    if (response.endsWith("}")) {
      response = response.substring(0, response.length() - 1);
      response += ",\"connected\":" + String(isConnected ? "true" : "false");
      response += ",\"silence_ms\":" + String(silenceMs);
      if (baselineCaptured) {
        response += ",\"base_alt\":" + String(baselineAltitude, 1);
      }
      response += "}";
    }
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.send(200, "application/json", response);
  });

  // Zero / Tare relative altitude endpoint
  server.on("/zero_alt", HTTP_GET, []() {
    baselineCaptured = false; // Next received RF packet will set new baseline
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.send(200, "text/plain", "OK");
  });

  // Captive Portal fallback
  server.onNotFound([]() {
    String uri = server.uri();
    if (uri.endsWith(".html") || uri.endsWith(".htm") || uri == "/" || uri.indexOf("generate_204") >= 0 || uri.indexOf("hotspot") >= 0) {
      server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
      server.send_P(200, "text/html", landing_html, sizeof(landing_html) - 1);
    } else {
      server.sendHeader("Location", String("http://") + WiFi.softAPIP().toString(), true);
      server.send(302, "text/plain", "");
    }
  });

  server.begin();
  Serial.println("Ground Station Gateway online on port 80!");
}

void loop() {
  dnsServer.processNextRequest();
  server.handleClient();

  // Read incoming commands from USB Serial (Web Serial API)
  if (Serial.available()) {
    String usbCmd = Serial.readStringUntil('\n');
    usbCmd.trim();
    if (usbCmd.indexOf("zero_alt") >= 0) {
      baselineCaptured = false;
    }
  }

  // Read incoming RF packet on Serial2
  if (Serial2.available()) {
    String incomingJson = Serial2.readStringUntil('\n');
    incomingJson.trim();

    if (incomingJson.length() > 0) {
      StaticJsonDocument<256> doc;
      DeserializationError error = deserializeJson(doc, incomingJson);

      if (!error) {
        latestJson = incomingJson;
        lastRxTime = millis(); // Reset the watchdog timer
        
        // Auto-capture initial baseline altitude from first packet after turn-on
        if (!baselineCaptured && doc.containsKey("alt")) {
          baselineAltitude = doc["alt"].as<float>();
          baselineCaptured = true;
        }

        // Build augmented telemetry payload
        String broadcastPayload = latestJson;
        if (broadcastPayload.endsWith("}")) {
          broadcastPayload = broadcastPayload.substring(0, broadcastPayload.length() - 1);
          broadcastPayload += ",\"connected\":true";
          broadcastPayload += ",\"silence_ms\":0";
          if (baselineCaptured) {
            broadcastPayload += ",\"base_alt\":" + String(baselineAltitude, 1);
          }
          broadcastPayload += "}";
        }

        // Simultaneous broadcast to USB Serial (115200 baud) for direct cable connection
        Serial.println(broadcastPayload);
      }
    }
  }
}