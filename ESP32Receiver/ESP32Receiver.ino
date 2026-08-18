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
String latestJson = "{\"temp\":0,\"hum\":0,\"press\":0,\"gas\":0,\"roll\":0,\"pitch\":0,\"magX\":0,\"magY\":0,\"magZ\":0,\"heading\":0}";

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
    * { box-sizing: border-box; }
    body {
      background-color: var(--bg-color);
      background-image:
        linear-gradient(rgba(0,255,234,0.05) 1px, transparent 1px),
        linear-gradient(90deg, rgba(0,255,234,0.05) 1px, transparent 1px);
      background-size: 20px 20px;
      color: var(--text-main);
      font-family: 'Share Tech Mono', monospace;
      margin: 0; padding: 20px;
    }
    .header {
      display: flex; justify-content: space-between; align-items: center;
      border-bottom: 2px solid var(--border-color);
      padding-bottom: 15px; margin-bottom: 25px;
    }
    .header h1 {
      margin: 0; font-size: 24px; color: var(--accent-color);
      text-shadow: 0 0 10px rgba(0,255,234,0.5);
      text-transform: uppercase; letter-spacing: 2px;
    }
    .status-indicator { display: flex; align-items: center; gap: 10px; font-size: 14px; color: var(--accent-color); }
    .dot { height: 10px; width: 10px; background-color: #00ff00; border-radius: 50%; box-shadow: 0 0 10px #00ff00; animation: blink 1s infinite; }
    @keyframes blink { 0%,100%{opacity:1} 50%{opacity:0.3} }

    /* --- 3D Satellite Panel --- */
    .sat-panel {
      background: var(--panel-bg);
      border: 1px solid var(--border-color);
      border-left: 4px solid var(--accent-color);
      position: relative;
      margin-bottom: 25px;
      padding: 15px 20px;
    }
    .sat-panel::before, .sat-panel::after {
      content: ''; position: absolute; width: 12px; height: 12px;
      border: 1px solid var(--accent-color);
    }
    .sat-panel::before { top:-1px; right:-1px; border-left:none; border-bottom:none; }
    .sat-panel::after  { bottom:-1px; right:-1px; border-left:none; border-top:none; }
    .sat-header {
      display: flex; justify-content: space-between; align-items: center;
      margin-bottom: 10px;
    }
    .sat-header span { font-size: 11px; color: var(--text-dim); text-transform: uppercase; letter-spacing: 1px; }
    .sat-angles { display: flex; gap: 25px; font-size: 13px; }
    .sat-angles div { color: var(--text-dim); }
    .sat-angles strong { color: var(--accent-color); }
    #satellite-canvas {
      width: 100%; display: block;
      height: 220px;
    }

    /* --- Data Grid --- */
    .grid {
      display: grid;
      grid-template-columns: repeat(auto-fit, minmax(240px, 1fr));
      gap: 18px;
    }
    .card {
      background: var(--panel-bg);
      padding: 18px;
      border: 1px solid var(--border-color);
      border-left: 4px solid var(--accent-color);
      position: relative;
      box-shadow: inset 0 0 20px rgba(0,0,0,0.5);
      display: flex; flex-direction: column;
    }
    .card::before, .card::after {
      content: ''; position: absolute; width: 10px; height: 10px;
      border: 1px solid var(--accent-color);
    }
    .card::before { top:-1px; right:-1px; border-left:none; border-bottom:none; }
    .card::after  { bottom:-1px; right:-1px; border-left:none; border-top:none; }
    .card h3 { margin: 0 0 5px 0; font-size: 11px; color: var(--text-dim); text-transform: uppercase; letter-spacing: 1px; }
    .val-container { display: flex; align-items: baseline; margin-bottom: 10px; }
    .val { font-size: 30px; color: var(--accent-color); text-shadow: 0 0 8px rgba(0,255,234,0.3); }
    .unit { font-size: 13px; color: var(--text-dim); margin-left: 5px; }
    .sparkline { width: 100%; height: 55px; border-bottom: 1px solid rgba(0,255,234,0.1); margin-top: auto; display: block; }
    .section-title {
      grid-column: 1 / -1; margin-top: 10px; margin-bottom: 3px;
      color: var(--text-dim); font-size: 12px; text-transform: uppercase;
      border-bottom: 1px dashed var(--border-color); padding-bottom: 5px;
    }
  </style>
</head>
<body>
  <div class="header">
    <h1>CanSat // Mission Control</h1>
    <div class="status-indicator"><div class="dot"></div>LINK ACTIVE</div>
  </div>

  <!-- 3D Satellite Attitude Visualizer -->
  <div class="sat-panel">
    <div class="sat-header">
      <span>&#x25B6; Attitude Visualizer // 3D Render</span>
      <div class="sat-angles">
        <div>Pitch: <strong><span id="sat-pitch">0.0</span>&deg;</strong></div>
        <div>Roll: <strong><span id="sat-roll">0.0</span>&deg;</strong></div>
      </div>
    </div>
    <canvas id="satellite-canvas" width="800" height="220"></canvas>
  </div>

  <div class="grid">
    <div class="section-title">Attitude Control (MPU-6050)</div>
    <div class="card">
      <h3>Pitch</h3>
      <div class="val-container"><div class="val"><span id="pitch">--</span><span class="unit">&deg;</span></div></div>
      <canvas class="sparkline" id="canvas-pitch" width="200" height="55"></canvas>
    </div>
    <div class="card">
      <h3>Roll</h3>
      <div class="val-container"><div class="val"><span id="roll">--</span><span class="unit">&deg;</span></div></div>
      <canvas class="sparkline" id="canvas-roll" width="200" height="55"></canvas>
    </div>

    <div class="section-title">Environment (BME-688)</div>
    <div class="card">
      <h3>Temperature</h3>
      <div class="val-container"><div class="val"><span id="temp">--</span><span class="unit">&deg;C</span></div></div>
      <canvas class="sparkline" id="canvas-temp" width="200" height="55"></canvas>
    </div>
    <div class="card">
      <h3>Humidity</h3>
      <div class="val-container"><div class="val"><span id="hum">--</span><span class="unit">%</span></div></div>
      <canvas class="sparkline" id="canvas-hum" width="200" height="55"></canvas>
    </div>
    <div class="card">
      <h3>Pressure</h3>
      <div class="val-container"><div class="val"><span id="press">--</span><span class="unit">hPa</span></div></div>
      <canvas class="sparkline" id="canvas-press" width="200" height="55"></canvas>
    </div>
    <div class="card">
      <h3>Gas Resistance</h3>
      <div class="val-container"><div class="val"><span id="gas">--</span><span class="unit">k&Omega;</span></div></div>
      <canvas class="sparkline" id="canvas-gas" width="200" height="55"></canvas>
    </div>

    <div class="section-title">Geomagnetic (BMM-350)</div>
    <div class="card">
      <h3>Mag X</h3>
      <div class="val-container"><div class="val"><span id="magX">--</span><span class="unit">&micro;T</span></div></div>
      <canvas class="sparkline" id="canvas-magX" width="200" height="55"></canvas>
    </div>
    <div class="card">
      <h3>Mag Y</h3>
      <div class="val-container"><div class="val"><span id="magY">--</span><span class="unit">&micro;T</span></div></div>
      <canvas class="sparkline" id="canvas-magY" width="200" height="55"></canvas>
    </div>
    <div class="card">
      <h3>Mag Z</h3>
      <div class="val-container"><div class="val"><span id="magZ">--</span><span class="unit">&micro;T</span></div></div>
      <canvas class="sparkline" id="canvas-magZ" width="200" height="55"></canvas>
    </div>

    <div class="section-title">Navigation (Compass)</div>

    <!-- Compass Rose Card -->
    <div class="card" style="grid-column: span 2; align-items: center;">
      <h3>Heading (Tilt-Compensated)</h3>
      <div style="display:flex; align-items:center; gap:30px; justify-content:center; flex-wrap:wrap; width:100%;">
        <canvas id="compass-canvas" width="200" height="200"></canvas>
        <div style="text-align:center;">
          <div style="font-size:11px; color:var(--text-dim); text-transform:uppercase; letter-spacing:1px; margin-bottom:8px;">Bearing</div>
          <div style="font-size:52px; color:var(--accent-color); text-shadow:0 0 12px rgba(0,255,234,0.4);"><span id="heading-deg">---</span><span style="font-size:20px; color:var(--text-dim);">&deg;</span></div>
          <div id="heading-dir" style="font-size:18px; color:var(--text-dim); margin-top:5px; letter-spacing:2px;">---</div>
        </div>
      </div>
    </div>
  </div>

  <script>
    // =====================================================
    // 3D SATELLITE VISUALIZER ENGINE
    // Pure JS + Canvas — No internet / No libraries needed
    // =====================================================
    const satCanvas = document.getElementById('satellite-canvas');
    const satCtx = satCanvas.getContext('2d');
    let livePitch = 0, liveRoll = 0, autoYaw = 0;

    // Define satellite vertices:
    // Main body: 28x18x12 half-extents
    // Left solar panel: extends from -28 to -73 on X, thin on Y, wide on Z
    // Right solar panel: mirror of left
    function makeSatVerts() {
      const b = [28, 18, 12];
      const pw = 45, ph = 2.5, pd = 22;
      const verts = [
        // Main body (0-7)
        [-b[0],-b[1],-b[2]], [ b[0],-b[1],-b[2]], [ b[0], b[1],-b[2]], [-b[0], b[1],-b[2]],
        [-b[0],-b[1], b[2]], [ b[0],-b[1], b[2]], [ b[0], b[1], b[2]], [-b[0], b[1], b[2]],
        // Left panel (8-15)
        [-b[0]-pw, -ph, -pd], [-b[0], -ph, -pd], [-b[0], ph, -pd], [-b[0]-pw, ph, -pd],
        [-b[0]-pw, -ph,  pd], [-b[0], -ph,  pd], [-b[0], ph,  pd], [-b[0]-pw, ph,  pd],
        // Right panel (16-23)
        [ b[0], -ph, -pd], [ b[0]+pw, -ph, -pd], [ b[0]+pw, ph, -pd], [ b[0], ph, -pd],
        [ b[0], -ph,  pd], [ b[0]+pw, -ph,  pd], [ b[0]+pw, ph,  pd], [ b[0], ph,  pd],
        // Antenna: small rod sticking up from center top (24-25)
        [0, -b[1], 0], [0, -b[1]-22, 0]
      ];
      return verts;
    }

    function makeSatEdges() {
      const addBox = (s, e) => {
        const f = [];
        f.push([s,s+1],[s+1,s+2],[s+2,s+3],[s+3,s]);
        f.push([s+4,s+5],[s+5,s+6],[s+6,s+7],[s+7,s+4]);
        f.push([s,s+4],[s+1,s+5],[s+2,s+6],[s+3,s+7]);
        return f;
      };
      return [
        ...addBox(0, 7),  // body
        ...addBox(8, 15), // left panel
        ...addBox(16,23), // right panel
        [24,25]           // antenna
      ];
    }

    const SAT_VERTS = makeSatVerts();
    const SAT_EDGES = makeSatEdges();

    function rotateVert(v, pitch, roll, yaw) {
      let [x,y,z] = v;
      // Yaw (Y axis)
      let x1 =  x*Math.cos(yaw) + z*Math.sin(yaw);
      let z1 = -x*Math.sin(yaw) + z*Math.cos(yaw);
      let y1 = y;
      // Pitch (X axis)
      let y2 =  y1*Math.cos(pitch) - z1*Math.sin(pitch);
      let z2 =  y1*Math.sin(pitch) + z1*Math.cos(pitch);
      let x2 = x1;
      // Roll (Z axis)
      let x3 =  x2*Math.cos(roll) - y2*Math.sin(roll);
      let y3 =  x2*Math.sin(roll) + y2*Math.cos(roll);
      return [x3, y3, z2];
    }

    function project(v, cx, cy, fov) {
      const scale = fov / (fov + v[2] + 50);
      return [cx + v[0]*scale, cy + v[1]*scale, scale];
    }

    function drawSat() {
      const W = satCanvas.width, H = satCanvas.height;
      const cx = W/2, cy = H/2;
      const fov = 260;
      const pRad = livePitch * Math.PI / 180;
      const rRad = liveRoll  * Math.PI / 180;

      satCtx.clearRect(0, 0, W, H);

      // Project all vertices
      const proj = SAT_VERTS.map(v => {
        const r = rotateVert(v, pRad, rRad, autoYaw);
        return project(r, cx, cy, fov);
      });

      // Draw edges — glow pass then solid pass
      for (let pass = 0; pass < 2; pass++) {
        satCtx.shadowColor = pass === 0 ? '#00ffea' : 'transparent';
        satCtx.shadowBlur  = pass === 0 ? 10 : 0;
        satCtx.strokeStyle = '#00ffea';
        satCtx.lineWidth   = pass === 0 ? 4 : 1.5;
        satCtx.globalAlpha = pass === 0 ? 0.25 : 1.0;

        for (const [a, b] of SAT_EDGES) {
          satCtx.beginPath();
          satCtx.moveTo(proj[a][0], proj[a][1]);
          satCtx.lineTo(proj[b][0], proj[b][1]);
          satCtx.stroke();
        }
      }
      satCtx.globalAlpha = 1.0;
      satCtx.shadowBlur  = 0;

      // Draw vertex dots at body corners
      satCtx.fillStyle = '#00ffea';
      for (let i = 0; i < 8; i++) {
        satCtx.beginPath();
        satCtx.arc(proj[i][0], proj[i][1], 2.5, 0, Math.PI*2);
        satCtx.fill();
      }

      // Slow auto yaw to keep it alive
      autoYaw += 0.008;
      requestAnimationFrame(drawSat);
    }

    drawSat();

    // =====================================================
    // SPARKLINE GRAPH ENGINE
    // =====================================================
    const MAX_POINTS = 30;
    const hist = { pitch:[], roll:[], temp:[], hum:[], press:[], gas:[], magX:[], magY:[], magZ:[] };

    function drawSparkline(key) {
      const canvas = document.getElementById('canvas-' + key);
      const ctx = canvas.getContext('2d');
      const W = canvas.width, H = canvas.height;
      const data = hist[key];
      ctx.clearRect(0, 0, W, H);
      if (data.length < 2) return;
      let mn = Math.min(...data), mx = Math.max(...data);
      if (mn === mx) { mn -= 1; mx += 1; }
      const rng = mx - mn;
      ctx.beginPath();
      ctx.strokeStyle = '#00ffea';
      ctx.lineWidth = 2;
      ctx.lineJoin = 'round';
      for (let i = 0; i < data.length; i++) {
        const x = (i / (MAX_POINTS-1)) * W;
        const y = H - ((data[i]-mn)/rng)*H*0.78 - H*0.11;
        i === 0 ? ctx.moveTo(x,y) : ctx.lineTo(x,y);
      }
      ctx.stroke();
      // Gradient fill
      ctx.lineTo(W, H); ctx.lineTo(0, H); ctx.closePath();
      const g = ctx.createLinearGradient(0,0,0,H);
      g.addColorStop(0, 'rgba(0,255,234,0.22)');
      g.addColorStop(1, 'rgba(0,255,234,0)');
      ctx.fillStyle = g; ctx.fill();
    }

    function pushAndDraw(key, val) {
      hist[key].push(val);
      if (hist[key].length > MAX_POINTS) hist[key].shift();
      drawSparkline(key);
    }

    // =====================================================
    // DATA FETCH LOOP (every 500ms)
    // =====================================================
    setInterval(() => {
      fetch('/data')
        .then(r => r.json())
        .then(d => {
          // Update 3D satellite angles
          livePitch = d.pitch;
          liveRoll  = d.roll;
          document.getElementById('sat-pitch').innerText = d.pitch.toFixed(1);
          document.getElementById('sat-roll').innerText  = d.roll.toFixed(1);

          // Update data cards
          document.getElementById('pitch').innerText = d.pitch.toFixed(1);
          document.getElementById('roll').innerText  = d.roll.toFixed(1);
          pushAndDraw('pitch', d.pitch);
          pushAndDraw('roll',  d.roll);

          document.getElementById('temp').innerText = d.temp.toFixed(1);
          document.getElementById('hum').innerText  = d.hum.toFixed(1);
          document.getElementById('press').innerText = d.press.toFixed(2);
          document.getElementById('gas').innerText  = d.gas.toFixed(1);
          pushAndDraw('temp',  d.temp);
          pushAndDraw('hum',   d.hum);
          pushAndDraw('press', d.press);
          pushAndDraw('gas',   d.gas);

          document.getElementById('magX').innerText = d.magX.toFixed(1);
          document.getElementById('magY').innerText = d.magY.toFixed(1);
          document.getElementById('magZ').innerText = d.magZ.toFixed(1);
          pushAndDraw('magX', d.magX);
          pushAndDraw('magY', d.magY);
          pushAndDraw('magZ', d.magZ);

          // Update compass rose
          drawCompass(d.heading);
          document.getElementById('heading-deg').innerText = Math.round(d.heading);
          document.getElementById('heading-dir').innerText = bearingLabel(d.heading);
        })
        .catch(() => {});
    }, 500);

    // =====================================================
    // COMPASS ROSE ENGINE
    // =====================================================
    function bearingLabel(deg) {
      const dirs = ['N','NNE','NE','ENE','E','ESE','SE','SSE','S','SSW','SW','WSW','W','WNW','NW','NNW'];
      return dirs[Math.round(deg / 22.5) % 16];
    }

    function drawCompass(heading) {
      const canvas = document.getElementById('compass-canvas');
      const ctx = canvas.getContext('2d');
      const W = canvas.width, H = canvas.height;
      const cx = W / 2, cy = H / 2;
      const R = Math.min(W, H) / 2 - 8;

      ctx.clearRect(0, 0, W, H);

      // Outer ring glow
      ctx.beginPath();
      ctx.arc(cx, cy, R, 0, Math.PI * 2);
      ctx.strokeStyle = '#005a8f';
      ctx.lineWidth = 2;
      ctx.shadowColor = '#00ffea';
      ctx.shadowBlur = 6;
      ctx.stroke();
      ctx.shadowBlur = 0;

      // Inner decorative ring
      ctx.beginPath();
      ctx.arc(cx, cy, R * 0.72, 0, Math.PI * 2);
      ctx.strokeStyle = 'rgba(0,90,143,0.4)';
      ctx.lineWidth = 1;
      ctx.stroke();

      // Tick marks (every 30 degrees = 12 ticks)
      for (let i = 0; i < 12; i++) {
        const ang = (i / 12) * Math.PI * 2 - Math.PI / 2;
        const isMain = i % 3 === 0;
        const r1 = isMain ? R - 14 : R - 8;
        ctx.beginPath();
        ctx.moveTo(cx + Math.cos(ang) * r1, cy + Math.sin(ang) * r1);
        ctx.lineTo(cx + Math.cos(ang) * R,  cy + Math.sin(ang) * R);
        ctx.strokeStyle = isMain ? 'rgba(0,255,234,0.7)' : 'rgba(0,90,143,0.8)';
        ctx.lineWidth = isMain ? 2 : 1;
        ctx.stroke();
      }

      // Cardinal and intercardinal labels
      const cardinals = [
        { label: 'N',  deg: 0   },
        { label: 'NE', deg: 45  },
        { label: 'E',  deg: 90  },
        { label: 'SE', deg: 135 },
        { label: 'S',  deg: 180 },
        { label: 'SW', deg: 225 },
        { label: 'W',  deg: 270 },
        { label: 'NW', deg: 315 }
      ];
      cardinals.forEach(c => {
        const ang = (c.deg - 90) * Math.PI / 180;
        const isCardinal = c.label.length === 1;
        const labelR = R - (isCardinal ? 24 : 26);
        ctx.fillStyle = c.label === 'N' ? '#ff4d4d' : (isCardinal ? '#00ffea' : 'rgba(0,255,234,0.5)');
        ctx.font = isCardinal ? 'bold 13px Share Tech Mono' : '10px Share Tech Mono';
        ctx.textAlign = 'center';
        ctx.textBaseline = 'middle';
        if (c.label === 'N') {
          ctx.shadowColor = '#ff4d4d';
          ctx.shadowBlur = 6;
        }
        ctx.fillText(c.label, cx + Math.cos(ang) * labelR, cy + Math.sin(ang) * labelR);
        ctx.shadowBlur = 0;
      });

      // --- Draw rotating needle ---
      const needleAng = (heading - 90) * Math.PI / 180;
      const needleLen = R * 0.6;
      const tailLen   = R * 0.28;

      // Needle tip (cyan glow — points to heading)
      ctx.beginPath();
      ctx.moveTo(cx - Math.cos(needleAng) * tailLen,
                 cy - Math.sin(needleAng) * tailLen);
      ctx.lineTo(cx + Math.cos(needleAng) * needleLen,
                 cy + Math.sin(needleAng) * needleLen);
      ctx.strokeStyle = '#00ffea';
      ctx.lineWidth = 2.5;
      ctx.shadowColor = '#00ffea';
      ctx.shadowBlur = 10;
      ctx.stroke();
      ctx.shadowBlur = 0;

      // Needle tail (red — points opposite / south)
      const southAng = needleAng + Math.PI;
      ctx.beginPath();
      ctx.moveTo(cx, cy);
      ctx.lineTo(cx + Math.cos(southAng) * tailLen,
                 cy + Math.sin(southAng) * tailLen);
      ctx.strokeStyle = '#ff4d4d';
      ctx.lineWidth = 2;
      ctx.shadowColor = '#ff4d4d';
      ctx.shadowBlur = 6;
      ctx.stroke();
      ctx.shadowBlur = 0;

      // Center pivot dot
      ctx.beginPath();
      ctx.arc(cx, cy, 5, 0, Math.PI * 2);
      ctx.fillStyle = '#00ffea';
      ctx.shadowColor = '#00ffea';
      ctx.shadowBlur = 8;
      ctx.fill();
      ctx.shadowBlur = 0;
    }

    // Draw a default north-facing compass on load
    drawCompass(0);
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
        //Serial.print("Received valid telemetry: ");
        Serial.println(latestJson);
      } else {
        Serial.println("JSON parse error from RF module");
      }
    }
  }
  
}