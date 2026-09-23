#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <DNSServer.h>
#include <ArduinoJson.h>

// Wi-Fi Access Point Credentials
const char* ssid     = "K12 Satellite";
const char* password = "K12 Satellite";

// DNS Server for Captive Portal (Port 53)
const byte DNS_PORT = 53;
DNSServer dnsServer;

// AsyncWebServer on standard HTTP port 80
AsyncWebServer server(80);
// AsyncWebSocket on "/ws" endpoint
AsyncWebSocket ws("/ws");

// RF Module connected to Serial2
#define RXD2 33
#define TXD2 32

// Default JSON — temp, press, alt, roll, pitch, heading
String latestJson = "{\"temp\":0,\"press\":0,\"alt\":0,\"roll\":0,\"pitch\":0,\"heading\":0}";
unsigned long lastRxTime = 0;
const unsigned long LINK_TIMEOUT_MS = 6000; // 6 seconds silence timeout (within 6-10s window)
float baselineAltitude = -999999.0;
bool baselineCaptured = false;

// Lightweight Landing Page & Quick-Launch Portal (<1.5 KB, zero RAM burden)
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
    <div class="badge">● GATEWAY ONLINE (WEBSOCKET PORT 80)</div>
    <h1>🛰️ K12-SAT // GROUND STATION</h1>
    <p>This ESP32 is running as a dedicated high-speed RF-to-WebSocket bridge (ws://192.168.4.1/ws). Heavy 3D assets are rendered locally on your device GPU via the PWA app.</p>
    <a href="/data" class="btn btn-secondary">View Live JSON Raw Data (/data)</a>
    <div class="status-box">
      <strong>Field Operation Quick-Start:</strong><br>
      1. Open your saved <strong>K12-SAT Mission Control PWA</strong> app.<br>
      2. If not saved, launch from your local folder / GitHub Pages.<br>
      3. Live orientation &amp; telemetry stream automatically connects to <code>ws://192.168.4.1/ws</code>.
    </div>
  </div>
</body>
</html>
)rawliteral";

// WebSocket Event Handler
void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type,
               void *arg, uint8_t *data, size_t len) {
  if (type == WS_EVT_CONNECT) {
    Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
    // Immediately send current state upon connection
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
    client->text(response);
  } else if (type == WS_EVT_DISCONNECT) {
    Serial.printf("WebSocket client #%u disconnected\n", client->id());
  } else if (type == WS_EVT_DATA) {
    // Handle client commands (e.g. zero_alt)
    AwsFrameInfo *info = (AwsFrameInfo*)arg;
    if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
      data[len] = 0;
      if (strstr((char*)data, "zero_alt") != NULL) {
        baselineCaptured = false;
        Serial.println("Zero Alt command received via WebSocket!");
      }
    }
  }
}

void setup() {
  delay(100);
  Serial.begin(115200);
  Serial2.begin(9600, SERIAL_8N1, RXD2, TXD2);
  Serial2.setTimeout(50); // Snappy 50ms read timeout

  Serial.println("\nInitializing ESP32 Lightweight Gateway (WebSocket Bridge)...");

  // Setup Wi-Fi Access Point
  WiFi.mode(WIFI_AP);
  WiFi.softAP(ssid, password);
  Serial.print("AP IP address: ");
  Serial.println(WiFi.softAPIP());

  // Captive Portal DNS — any domain → ESP32
  dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());

  // Attach WebSocket handler to /ws
  ws.onEvent(onWsEvent);
  server.addHandler(&ws);

  // Landing page handler
  auto handleLanding = [](AsyncWebServerRequest *request) {
    AsyncWebServerResponse *response = request->beginResponse_P(200, "text/html", landing_html, sizeof(landing_html) - 1);
    response->addHeader("Cache-Control", "no-cache, no-store, must-revalidate");
    request->send(response);
  };

  server.on("/", HTTP_GET, handleLanding);
  server.on("/index.html", HTTP_GET, handleLanding);
  server.on("/generate_204", HTTP_GET, handleLanding);        // Android captive portal
  server.on("/gen_204", HTTP_GET, handleLanding);             // Android captive portal
  server.on("/hotspot-detect.html", HTTP_GET, handleLanding); // Apple/iOS captive portal
  server.on("/canonical.html", HTTP_GET, handleLanding);      // Android captive portal
  server.on("/connecttest.txt", HTTP_GET, handleLanding);     // Windows captive portal
  server.on("/ncsi.txt", HTTP_GET, handleLanding);            // Windows captive portal

  // Live JSON data endpoint (HTTP Fallback)
  server.on("/data", HTTP_GET, [](AsyncWebServerRequest *request) {
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
    request->send(200, "application/json", response);
  });

  // Zero / Tare relative altitude endpoint
  server.on("/zero_alt", HTTP_GET, [](AsyncWebServerRequest *request) {
    baselineCaptured = false; // Next received RF packet will set new baseline
    request->send(200, "text/plain", "OK");
  });

  // Captive Portal 404 handler
  server.onNotFound([](AsyncWebServerRequest *request) {
    String uri = request->url();
    if (uri.endsWith(".html") || uri.endsWith(".htm") || uri == "/" || uri.indexOf("generate_204") >= 0 || uri.indexOf("hotspot") >= 0) {
      AsyncWebServerResponse *response = request->beginResponse_P(200, "text/html", landing_html, sizeof(landing_html) - 1);
      response->addHeader("Cache-Control", "no-cache, no-store, must-revalidate");
      request->send(response);
    } else {
      request->redirect(String("http://") + WiFi.softAPIP().toString());
    }
  });

  server.begin();
  Serial.println("Async WebServer & WebSocket Bridge online on port 80!");
}

void loop() {
  dnsServer.processNextRequest();
  ws.cleanupClients(); // Efficiently clean up disconnected WebSocket clients

  // Check incoming commands from USB Serial (Web Serial API)
  if (Serial.available()) {
    String usbCmd = Serial.readStringUntil('\n');
    usbCmd.trim();
    if (usbCmd.indexOf("zero_alt") >= 0) {
      baselineCaptured = false; // Next received RF packet will set new baseline
    }
  }

  // Check incoming RF packet on Serial2
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

        // Build augmented telemetry payload for real-time WebSocket & USB broadcast
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

        // Instant broadcast to all connected WebSocket clients with zero polling latency!
        ws.textAll(broadcastPayload);

        // Simultaneous broadcast to USB Serial (115200 baud) for direct cable connection
        Serial.println(broadcastPayload);
      }
    }
  }
}