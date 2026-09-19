#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <ArduinoJson.h>
#include "three_js.h"
#include "gltf_loader.h"
#include "satellite_model.h"

// Wi-Fi Access Point Credentials
const char* ssid     = "K12 Satellite";
const char* password = "K12 Satellite";

// DNS Server for Captive Portal (Port 53)
const byte DNS_PORT = 53;
DNSServer dnsServer;

// Standard WebServer on port 80
WebServer server(80);

// RF Module connected to Serial2
#define RXD2 33
#define TXD2 32

// Default JSON — temp, press, alt, roll, pitch, heading
String latestJson = "{\"temp\":0,\"press\":0,\"alt\":0,\"roll\":0,\"pitch\":0,\"heading\":0}";
unsigned long lastRxTime = 0;

// Embedded HTML/CSS/JS (High-Tech Satellite Telemetry UI)
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>CanSat // Mission Control</title>
  <script src="/three.min.js"></script>
  <script src="/GLTFLoader.js"></script>
  <script>
    if (typeof THREE === 'undefined') {
      document.write('<script src="https://cdnjs.cloudflare.com/ajax/libs/three.js/r128/three.min.js"><\\/script>');
      document.write('<script src="https://cdn.jsdelivr.net/npm/three@0.128.0/examples/js/loaders/GLTFLoader.js"><\\/script>');
    }
  </script>
  <style>
    @import url('https://fonts.googleapis.com/css2?family=Share+Tech+Mono&display=swap');
    :root {
      --bg-color: #030a16;
      --panel-bg: rgba(16, 33, 58, 0.75);
      --accent-color: #00ffea;
      --accent-dim: rgba(0, 255, 234, 0.25);
      --text-main: #e2f1ff;
      --text-dim: #8ba9c9;
      --border-color: #005a8f;
      --danger-color: #ff4d4d;
      --warn-color: #ffb84d;
      --success-color: #00ff66;
    }
    * { box-sizing: border-box; }
    body {
      background-color: var(--bg-color);
      background-image:
        linear-gradient(rgba(0,255,234,0.06) 1px, transparent 1px),
        linear-gradient(90deg, rgba(0,255,234,0.06) 1px, transparent 1px);
      background-size: 24px 24px;
      color: var(--text-main);
      font-family: 'Share Tech Mono', 'SF Mono', 'Segoe UI Mono', 'Roboto Mono', 'Cascadia Code', 'Ubuntu Mono', monospace;
      margin: 0;
      padding: 16px;
      min-height: 100vh;
    }
    .container {
      max-width: 1240px;
      margin: 0 auto;
      width: 100%;
    }

    /* Header */
    .header {
      border-bottom: 2px solid var(--border-color);
      padding-bottom: 14px;
      margin-bottom: 18px;
    }
    .header-top {
      display: flex;
      justify-content: space-between;
      align-items: center;
      flex-wrap: wrap;
      gap: 12px;
    }
    .header h1 {
      margin: 0;
      font-size: 24px;
      color: var(--accent-color);
      text-shadow: 0 0 12px rgba(0,255,234,0.55);
      text-transform: uppercase;
      letter-spacing: 2px;
    }
    .status-indicator {
      display: inline-flex;
      align-items: center;
      gap: 8px;
      font-size: 13px;
      color: var(--accent-color);
      background: rgba(0, 255, 234, 0.08);
      border: 1px solid var(--border-color);
      padding: 6px 12px;
      border-radius: 4px;
      transition: all 0.3s;
    }
    .dot {
      height: 10px;
      width: 10px;
      background-color: var(--success-color);
      border-radius: 50%;
      box-shadow: 0 0 10px var(--success-color);
      animation: blink 1s infinite;
      transition: all 0.3s;
    }
    .dot.lost {
      background-color: var(--danger-color);
      box-shadow: 0 0 10px var(--danger-color);
    }
    .status-indicator.lost {
      color: var(--danger-color);
      border-color: var(--danger-color);
      background: rgba(255, 77, 77, 0.1);
    }
    @keyframes blink { 0%,100%{opacity:1} 50%{opacity:0.3} }

    /* Mission Operations Sub-Bar */
    .mission-bar {
      display: flex;
      justify-content: space-between;
      align-items: center;
      flex-wrap: wrap;
      gap: 12px;
      margin-top: 14px;
      padding: 10px 14px;
      background: rgba(10, 25, 46, 0.85);
      border: 1px solid var(--border-color);
      border-left: 3px solid var(--accent-color);
      font-size: 12px;
    }
    .mission-stats {
      display: flex;
      gap: 18px;
      flex-wrap: wrap;
      align-items: center;
    }
    .mission-stats span {
      color: var(--text-dim);
      text-transform: uppercase;
    }
    .mission-stats strong {
      color: var(--accent-color);
      margin-left: 4px;
      letter-spacing: 1px;
    }
    .action-btn {
      background: transparent;
      color: var(--accent-color);
      border: 1px solid var(--accent-color);
      font-family: inherit;
      font-size: 11px;
      text-transform: uppercase;
      letter-spacing: 1px;
      padding: 6px 12px;
      cursor: pointer;
      display: inline-flex;
      align-items: center;
      gap: 6px;
      transition: all 0.2s ease;
    }
    .action-btn:hover {
      background: var(--accent-color);
      color: #030a16;
      box-shadow: 0 0 12px rgba(0,255,234,0.6);
    }
    .action-btn.secondary {
      border-color: var(--border-color);
      color: var(--text-dim);
      padding: 3px 8px;
      font-size: 10px;
    }
    .action-btn.secondary:hover {
      background: var(--border-color);
      color: var(--text-main);
    }

    /* 3D Satellite Panel */
    .sat-panel {
      background: var(--panel-bg);
      border: 1px solid var(--border-color);
      border-left: 4px solid var(--accent-color);
      position: relative;
      margin-bottom: 22px;
      padding: 14px 18px;
      box-shadow: inset 0 0 25px rgba(0,0,0,0.6);
    }
    .sat-panel::before, .sat-panel::after {
      content:'';
      position:absolute;
      width:12px;
      height:12px;
      border:1px solid var(--accent-color);
    }
    .sat-panel::before { top:-1px; right:-1px; border-left:none; border-bottom:none; }
    .sat-panel::after  { bottom:-1px; right:-1px; border-left:none; border-top:none; }
    .sat-header {
      display:flex;
      justify-content:space-between;
      align-items:center;
      flex-wrap:wrap;
      gap:10px;
      margin-bottom:8px;
    }
    .sat-header span {
      font-size:11px;
      color:var(--text-dim);
      text-transform:uppercase;
      letter-spacing:1px;
    }
    .sat-angles {
      display:flex;
      gap:16px;
      font-size:13px;
      flex-wrap:wrap;
    }
    .sat-angles div { color:var(--text-dim); }
    .sat-angles strong { color:var(--accent-color); }
    .sat-footer {
      display: flex;
      justify-content: space-between;
      align-items: center;
      flex-wrap: wrap;
      gap: 10px;
      margin-top: 10px;
      padding-top: 8px;
      border-top: 1px solid rgba(0, 90, 143, 0.4);
    }
    .sat-badge {
      font-size: 11px;
      background: rgba(0,255,234,0.12);
      border: 1px solid var(--border-color);
      color: var(--accent-color);
      padding: 4px 8px;
      border-radius: 3px;
      white-space: nowrap;
      display: inline-flex;
      align-items: center;
    }
    .sat-hint {
      font-size: 10px;
      color: var(--text-dim);
      text-transform: uppercase;
      letter-spacing: 0.5px;
    }
    @media (max-width: 600px) {
      .sat-header {
        flex-direction: column;
        align-items: flex-start;
        gap: 6px;
      }
      .sat-angles {
        gap: 10px;
        font-size: 12px;
      }
      .sat-footer {
        flex-direction: column;
        align-items: flex-start;
        gap: 6px;
      }
      .sat-hint {
        font-size: 9px;
      }
    }

    /* Data Grid */
    .grid { opacity: 1; transition: opacity 0.4s; }
    .grid.lost { opacity: 0.45; }
    .grid-container {
      display: grid;
      grid-template-columns: repeat(auto-fit, minmax(250px, 1fr));
      gap: 16px;
    }
    .card {
      background: var(--panel-bg);
      padding: 16px;
      border: 1px solid var(--border-color);
      border-left: 4px solid var(--accent-color);
      position: relative;
      box-shadow: inset 0 0 20px rgba(0,0,0,0.55);
      display: flex;
      flex-direction: column;
    }
    .card::before, .card::after {
      content:'';
      position:absolute;
      width:10px;
      height:10px;
      border:1px solid var(--accent-color);
    }
    .card::before { top:-1px; right:-1px; border-left:none; border-bottom:none; }
    .card::after  { bottom:-1px; right:-1px; border-left:none; border-top:none; }
    .card h3 {
      margin:0 0 6px 0;
      font-size:11px;
      color:var(--text-dim);
      text-transform:uppercase;
      letter-spacing:1px;
    }
    .val-container {
      display:flex;
      align-items:baseline;
      margin-bottom:6px;
    }
    .val {
      font-size:32px;
      color:var(--accent-color);
      text-shadow:0 0 10px rgba(0,255,234,0.35);
      font-weight: bold;
    }
    .unit {
      font-size:14px;
      color:var(--text-dim);
      margin-left:6px;
    }
    .sub-stats {
      display: flex;
      gap: 12px;
      font-size: 11px;
      color: var(--text-dim);
      margin-bottom: 8px;
      padding-bottom: 6px;
      border-bottom: 1px solid rgba(0,90,143,0.35);
      flex-wrap: wrap;
    }
    .sub-stats strong {
      color: var(--text-main);
    }
    .sparkline {
      width:100%;
      height:55px;
      border-bottom:1px solid rgba(0,255,234,0.12);
      margin-top:auto;
      display:block;
    }
    .section-title {
      grid-column:1/-1;
      margin-top:14px;
      margin-bottom:4px;
      color:var(--accent-color);
      font-size:12px;
      text-transform:uppercase;
      letter-spacing:1.5px;
      border-bottom:1px dashed var(--border-color);
      padding-bottom:6px;
      display: flex;
      align-items: center;
      gap: 8px;
    }
    .card-compass {
      align-items: center;
    }
    @media (min-width: 768px) {
      .card-compass {
        grid-column: span 2;
      }
    }
  </style>
</head>
<body>
  <div class="container">
    <!-- Header -->
    <div class="header">
      <div class="header-top">
        <h1>CanSat // Mission Control</h1>
        <div class="status-indicator" id="status-indicator">
          <div class="dot" id="status-dot"></div>
          <span id="status-text">LINK ACTIVE</span>
        </div>
      </div>

      <!-- Live Operations Bar -->
      <div class="mission-bar">
        <div class="mission-stats">
          <div>MET: <strong id="met-display">00:00:00</strong> <button class="action-btn secondary" onclick="resetMET()" title="Reset Mission Clock">↺</button></div>
          <div>Packets: <strong id="pkt-count">0</strong></div>
          <div>Rate: <strong id="pkt-rate">0.0</strong> Hz</div>
        </div>
        <div>
          <button class="action-btn" onclick="exportCSV()">💾 Export Flight CSV</button>
        </div>
      </div>
    </div>

    <!-- 3D Satellite Attitude Visualizer -->
    <div class="sat-panel">
      <div class="sat-header">
        <span>&#x25B6; 3D Satellite Attitude // Live Sensor Mirror</span>
        <div class="sat-angles">
          <div>Pitch: <strong><span id="sat-pitch">0.0</span>&deg;</strong></div>
          <div>Roll: <strong><span id="sat-roll">0.0</span>&deg;</strong></div>
          <div>Heading: <strong><span id="sat-head">0</span>&deg;</strong></div>
        </div>
      </div>
      <div style="position:relative; width:100%; height:260px; overflow:hidden;">
        <canvas id="sat-3d-canvas" style="width:100%; height:100%; display:block; cursor:grab;"></canvas>
      </div>
      <div class="sat-footer">
        <div id="sat-mode-badge" class="sat-badge">&#x25CF; TRACKING LIVE ATTITUDE</div>
        <div class="sat-hint">&#x1F5B1; DRAG TO INSPECT &bull; RELEASE TO SNAP BACK</div>
      </div>
    </div>

    <div class="grid grid-container" id="data-grid">

      <!-- Altitude & Barometer (BMP-280) -->
      <div class="section-title">&#x25C6; Flight Telemetry (BMP-280)</div>
      
      <div class="card">
        <h3>Altitude</h3>
        <div class="val-container"><div class="val"><span id="alt">--</span><span class="unit">m</span></div></div>
        <div class="sub-stats">
          <div>APOGEE: <strong id="alt-max">-- m</strong></div>
          <div>V-SPEED: <strong id="alt-vspeed">-- m/s</strong></div>
        </div>
        <canvas class="sparkline" id="canvas-alt" width="240" height="55"></canvas>
      </div>

      <div class="card">
        <h3>Pressure</h3>
        <div class="val-container"><div class="val"><span id="press">--</span><span class="unit">hPa</span></div></div>
        <div class="sub-stats">
          <div>MIN: <strong id="press-min">--</strong></div>
          <div>MAX: <strong id="press-max">--</strong></div>
        </div>
        <canvas class="sparkline" id="canvas-press" width="240" height="55"></canvas>
      </div>

      <div class="card">
        <h3>Temperature</h3>
        <div class="val-container"><div class="val"><span id="temp">--</span><span class="unit">&deg;C</span></div></div>
        <div class="sub-stats">
          <div>MIN: <strong id="temp-min">--</strong></div>
          <div>MAX: <strong id="temp-max">--</strong></div>
        </div>
        <canvas class="sparkline" id="canvas-temp" width="240" height="55"></canvas>
      </div>

      <!-- Attitude Control (MPU-6050) -->
      <div class="section-title">&#x25C6; Attitude Dynamics (MPU-6050)</div>

      <div class="card">
        <h3>Pitch Angle</h3>
        <div class="val-container"><div class="val"><span id="pitch">--</span><span class="unit">&deg;</span></div></div>
        <div class="sub-stats">
          <div>STATUS: <strong id="pitch-status">LEVEL</strong></div>
        </div>
        <canvas class="sparkline" id="canvas-pitch" width="240" height="55"></canvas>
      </div>

      <div class="card">
        <h3>Roll Angle</h3>
        <div class="val-container"><div class="val"><span id="roll">--</span><span class="unit">&deg;</span></div></div>
        <div class="sub-stats">
          <div>STATUS: <strong id="roll-status">LEVEL</strong></div>
        </div>
        <canvas class="sparkline" id="canvas-roll" width="240" height="55"></canvas>
      </div>

      <!-- Navigation (BMM-350 Compass) -->
      <div class="section-title">&#x25C6; Direction & Heading (BMM-350 Compass)</div>

      <div class="card card-compass">
        <h3>Heading (Tilt-Compensated)</h3>
        <div style="display:flex; align-items:center; gap:28px; justify-content:center; flex-wrap:wrap; width:100%; padding:8px 0;">
          <canvas id="compass-canvas" width="200" height="200"></canvas>
          <div style="text-align:center;">
            <div style="font-size:11px; color:var(--text-dim); text-transform:uppercase; letter-spacing:1.5px; margin-bottom:6px;">Bearing</div>
            <div style="font-size:52px; color:var(--accent-color); text-shadow:0 0 14px rgba(0,255,234,0.45); font-weight:bold;">
              <span id="heading-deg">---</span><span style="font-size:22px; color:var(--text-dim);">&deg;</span>
            </div>
            <div id="heading-dir" style="font-size:18px; color:var(--text-dim); margin-top:4px; letter-spacing:2px; font-weight:bold;">---</div>
          </div>
        </div>
      </div>

    </div>
  </div>

  <script>
    // =====================================================
    // MISSION DATA & CSV RECORDER
    // =====================================================
    const flightLog = [];
    let packetCount = 0;
    let lastPacketTime = 0;
    let rateCount = 0;
    let lastRateCalc = Date.now();
    let metStart = Date.now();

    // Stats
    let maxAltitude = -9999;
    let lastAlt = null;
    let lastAltTime = null;
    let vSpeed = 0;
    let minTemp = 9999, maxTemp = -9999;
    let minPress = 9999, maxPress = -9999;

    function formatMET(ms) {
      const totalSec = Math.floor(ms / 1000);
      const hrs = String(Math.floor(totalSec / 3600)).padStart(2, '0');
      const mins = String(Math.floor((totalSec % 3600) / 60)).padStart(2, '0');
      const secs = String(totalSec % 60).padStart(2, '0');
      return `${hrs}:${mins}:${secs}`;
    }

    setInterval(() => {
      document.getElementById('met-display').innerText = formatMET(Date.now() - metStart);
    }, 500);

    function resetMET() {
      metStart = Date.now();
      flightLog.length = 0; // Fresh log on reset
      maxAltitude = -9999;
      minTemp = 9999; maxTemp = -9999;
      minPress = 9999; maxPress = -9999;
    }

    function exportCSV() {
      if (flightLog.length === 0) {
        alert("No flight telemetry logged yet!");
        return;
      }
      let csv = "Timestamp_ISO,MET_Seconds,Temperature_C,Pressure_hPa,Altitude_m,Roll_deg,Pitch_deg,Heading_deg\n";
      flightLog.forEach(row => {
        csv += `${row.time},${row.met},${row.temp},${row.press},${row.alt},${row.roll},${row.pitch},${row.heading}\n`;
      });
      const blob = new Blob([csv], { type: 'text/csv' });
      const url = URL.createObjectURL(blob);
      const a = document.createElement('a');
      a.href = url;
      a.download = 'cansat_telemetry_' + new Date().toISOString().slice(0,19).replace(/[:T]/g,'_') + '.csv';
      document.body.appendChild(a);
      a.click();
      document.body.removeChild(a);
      URL.revokeObjectURL(url);
    }

    // =====================================================
    // REAL-TIME 3D SATELLITE ENGINE (Three.js + GLTF)
    // =====================================================
    const canvas3D = document.getElementById('sat-3d-canvas');
    const badge3D  = document.getElementById('sat-mode-badge');
    const scene3D  = new THREE.Scene();
    scene3D.background = new THREE.Color(0x030a16);

    const camera3D = new THREE.PerspectiveCamera(40, (canvas3D.clientWidth || 800) / (canvas3D.clientHeight || 260), 0.1, 2000);
    const renderer3D = new THREE.WebGLRenderer({ canvas: canvas3D, antialias: true, alpha: true });
    renderer3D.setSize(canvas3D.clientWidth || 800, canvas3D.clientHeight || 260);
    renderer3D.setPixelRatio(Math.min(window.devicePixelRatio, 2));

    // Space Lighting
    const ambLight = new THREE.AmbientLight(0xffffff, 0.95);
    scene3D.add(ambLight);
    const sunLight = new THREE.DirectionalLight(0xffffff, 1.4);
    sunLight.position.set(200, 300, 150);
    scene3D.add(sunLight);
    const rimLight = new THREE.DirectionalLight(0x00ffea, 0.7);
    rimLight.position.set(-200, -150, -200);
    scene3D.add(rimLight);

    let satModel = null;
    let targetPitch = 0;
    let targetRoll  = 0;
    let targetYaw   = 0;
    let currentPitch = 0;
    let currentRoll  = 0;
    let currentYaw   = 0;

    let manualYawOffset   = 0;
    let manualPitchOffset = 0;
    let isDragging        = false;
    let lastPointerX = 0, lastPointerY = 0;

    const loader = new THREE.GLTFLoader();
    loader.load('/satellite.glb', function(gltf) {
      satModel = gltf.scene;

      // Auto-center & fit into view
      const box = new THREE.Box3().setFromObject(satModel);
      const center = box.getCenter(new THREE.Vector3());
      const size = box.getSize(new THREE.Vector3());
      satModel.position.sub(center);

      const maxDim = Math.max(size.x, size.y, size.z);
      const scale = 52.0 / (maxDim || 1);
      satModel.scale.set(scale, scale, scale);

      camera3D.position.set(0, 22, 90);
      camera3D.lookAt(0, 0, 0);

      scene3D.add(satModel);
    }, undefined, function(err) {
      console.warn("Could not load /satellite.glb:", err);
    });

    // Touch & Mouse Drag Controls
    canvas3D.addEventListener('pointerdown', e => {
      isDragging = true;
      lastPointerX = e.clientX;
      lastPointerY = e.clientY;
      badge3D.innerText = '◐ USER INSPECTION (RELEASE TO ALIGN)';
      badge3D.style.borderColor = 'var(--warn-color)';
      badge3D.style.color = 'var(--warn-color)';
      canvas3D.style.cursor = 'grabbing';
    });

    window.addEventListener('pointermove', e => {
      if (!isDragging) return;
      const dx = e.clientX - lastPointerX;
      const dy = e.clientY - lastPointerY;
      lastPointerX = e.clientX;
      lastPointerY = e.clientY;

      manualYawOffset   += dx * 0.01;
      manualPitchOffset += dy * 0.01;
    });

    window.addEventListener('pointerup', () => {
      if (isDragging) {
        isDragging = false;
        badge3D.innerText = '● TRACKING LIVE ATTITUDE';
        badge3D.style.borderColor = 'var(--border-color)';
        badge3D.style.color = 'var(--accent-color)';
        canvas3D.style.cursor = 'grab';
      }
    });

    // Handle Window Resize
    function resize3D() {
      const parent = canvas3D.parentElement;
      if (parent && parent.clientWidth > 0 && parent.clientHeight > 0) {
        camera3D.aspect = parent.clientWidth / parent.clientHeight;
        camera3D.updateProjectionMatrix();
        renderer3D.setSize(parent.clientWidth, parent.clientHeight);
      }
    }
    window.addEventListener('resize', resize3D);
    setTimeout(resize3D, 250);

    // Shortest angular distance interpolation for yaw / heading (handles 0° <-> 360° boundary)
    function lerpAngle(cur, target, alpha) {
      let diff = (target - cur) % (Math.PI * 2);
      if (diff < -Math.PI) diff += Math.PI * 2;
      if (diff > Math.PI)  diff -= Math.PI * 2;
      return cur + diff * alpha;
    }

    // Animation loop with smooth interpolation and spring snap-back
    function animate3D() {
      requestAnimationFrame(animate3D);

      if (satModel) {
        // Buttery-smooth interpolation towards sensor telemetry targets
        const smoothSpeed = 0.08;
        currentPitch += (targetPitch - currentPitch) * smoothSpeed;
        currentRoll  += (targetRoll  - currentRoll)  * smoothSpeed;
        currentYaw    = lerpAngle(currentYaw, targetYaw, smoothSpeed);

        // Smoothly spring manual offsets back to zero upon release
        if (!isDragging) {
          manualYawOffset   += (0 - manualYawOffset) * 0.08;
          manualPitchOffset += (0 - manualPitchOffset) * 0.08;
        }

        // Aerospace Tait-Bryan Euler (Pitch on X, Heading on Y, Roll on Z)
        const euler = new THREE.Euler(
          currentPitch + manualPitchOffset,
          currentYaw   + manualYawOffset,
          currentRoll,
          'YXZ'
        );
        satModel.setRotationFromEuler(euler);
      }

      renderer3D.render(scene3D, camera3D);
    }
    animate3D();

    // =====================================================
    // SPARKLINE GRAPH ENGINE
    // =====================================================
    const MAX_POINTS = 30;
    const hist = { pitch:[], roll:[], temp:[], press:[], alt:[] };

    function drawSparkline(key) {
      const canvas=document.getElementById('canvas-'+key);
      if (!canvas) return;
      const ctx=canvas.getContext('2d');
      const W=canvas.width, H=canvas.height;
      const data=hist[key];
      ctx.clearRect(0,0,W,H);
      if(data.length<2)return;
      let mn=Math.min(...data), mx=Math.max(...data);
      if(mn===mx){mn-=1;mx+=1;}
      const rng=mx-mn;
      ctx.beginPath(); ctx.strokeStyle='#00ffea'; ctx.lineWidth=2; ctx.lineJoin='round';
      for(let i=0;i<data.length;i++){
        const x=(i/(MAX_POINTS-1))*W;
        const y=H-((data[i]-mn)/rng)*H*0.78-H*0.11;
        i===0?ctx.moveTo(x,y):ctx.lineTo(x,y);
      }
      ctx.stroke();
      ctx.lineTo(W,H);ctx.lineTo(0,H);ctx.closePath();
      const g=ctx.createLinearGradient(0,0,0,H);
      g.addColorStop(0,'rgba(0,255,234,0.22)');
      g.addColorStop(1,'rgba(0,255,234,0)');
      ctx.fillStyle=g; ctx.fill();
    }
    function pushAndDraw(key,val){
      hist[key].push(val);
      if(hist[key].length>MAX_POINTS)hist[key].shift();
      drawSparkline(key);
    }

    // =====================================================
    // COMPASS ROSE ENGINE
    // =====================================================
    function bearingLabel(deg){
      const dirs=['N','NNE','NE','ENE','E','ESE','SE','SSE','S','SSW','SW','WSW','W','WNW','NW','NNW'];
      return dirs[Math.round(deg/22.5)%16];
    }
    function drawCompass(heading){
      const canvas=document.getElementById('compass-canvas');
      const ctx=canvas.getContext('2d');
      const W=canvas.width, H=canvas.height;
      const cx=W/2, cy=H/2, R=Math.min(W,H)/2-8;
      ctx.clearRect(0,0,W,H);
      // Outer ring
      ctx.beginPath();ctx.arc(cx,cy,R,0,Math.PI*2);
      ctx.strokeStyle='#005a8f';ctx.lineWidth=2;ctx.shadowColor='#00ffea';ctx.shadowBlur=6;ctx.stroke();ctx.shadowBlur=0;
      // Inner ring
      ctx.beginPath();ctx.arc(cx,cy,R*0.72,0,Math.PI*2);
      ctx.strokeStyle='rgba(0,90,143,0.4)';ctx.lineWidth=1;ctx.stroke();
      // Tick marks
      for(let i=0;i<12;i++){
        const ang=(i/12)*Math.PI*2-Math.PI/2;
        const isMain=i%3===0;
        const r1=isMain?R-14:R-8;
        ctx.beginPath();ctx.moveTo(cx+Math.cos(ang)*r1,cy+Math.sin(ang)*r1);
        ctx.lineTo(cx+Math.cos(ang)*R,cy+Math.sin(ang)*R);
        ctx.strokeStyle=isMain?'rgba(0,255,234,0.7)':'rgba(0,90,143,0.8)';
        ctx.lineWidth=isMain?2:1;ctx.stroke();
      }
      // Labels
      [{label:'N',deg:0},{label:'NE',deg:45},{label:'E',deg:90},{label:'SE',deg:135},
       {label:'S',deg:180},{label:'SW',deg:225},{label:'W',deg:270},{label:'NW',deg:315}]
      .forEach(c=>{
        const ang=(c.deg-90)*Math.PI/180;
        const isC=c.label.length===1;
        ctx.fillStyle=c.label==='N'?'#ff4d4d':(isC?'#00ffea':'rgba(0,255,234,0.6)');
        ctx.font=isC?'bold 13px Share Tech Mono, monospace':'10px Share Tech Mono, monospace';
        ctx.textAlign='center'; ctx.textBaseline='middle';
        if(c.label==='N'){ctx.shadowColor='#ff4d4d';ctx.shadowBlur=6;}
        ctx.fillText(c.label,cx+Math.cos(ang)*(R-(isC?24:26)),cy+Math.sin(ang)*(R-(isC?24:26)));
        ctx.shadowBlur=0;
      });
      // Needle
      const nAng=(heading-90)*Math.PI/180;
      const nLen=R*0.6, tLen=R*0.28;
      ctx.beginPath();
      ctx.moveTo(cx-Math.cos(nAng)*tLen,cy-Math.sin(nAng)*tLen);
      ctx.lineTo(cx+Math.cos(nAng)*nLen,cy+Math.sin(nAng)*nLen);
      ctx.strokeStyle='#00ffea';ctx.lineWidth=2.5;ctx.shadowColor='#00ffea';ctx.shadowBlur=10;ctx.stroke();ctx.shadowBlur=0;
      ctx.beginPath();
      ctx.moveTo(cx,cy);ctx.lineTo(cx+Math.cos(nAng+Math.PI)*tLen,cy+Math.sin(nAng+Math.PI)*tLen);
      ctx.strokeStyle='#ff4d4d';ctx.lineWidth=2;ctx.shadowColor='#ff4d4d';ctx.shadowBlur=6;ctx.stroke();ctx.shadowBlur=0;
      ctx.beginPath();ctx.arc(cx,cy,5,0,Math.PI*2);
      ctx.fillStyle='#00ffea';ctx.shadowColor='#00ffea';ctx.shadowBlur=8;ctx.fill();ctx.shadowBlur=0;
    }
    drawCompass(0);

    // =====================================================
    // TELEMETRY INGEST & WATCHDOG (every 400ms)
    // =====================================================
    setInterval(()=>{
      fetch('/data')
        .then(r=>r.json())
        .then(d=>{
          const dot = document.getElementById('status-dot');
          const indicator = document.getElementById('status-indicator');
          const text = document.getElementById('status-text');
          const grid = document.getElementById('data-grid');

          if (d.connected) {
            dot.className = 'dot';
            indicator.className = 'status-indicator';
            text.innerText = 'LINK ACTIVE';
            grid.className = 'grid grid-container';
            
            // Packet tracking
            packetCount++;
            rateCount++;
            document.getElementById('pkt-count').innerText = packetCount;
            const now = Date.now();
            if (now - lastRateCalc >= 1000) {
              const hz = (rateCount / ((now - lastRateCalc) / 1000)).toFixed(1);
              document.getElementById('pkt-rate').innerText = hz;
              rateCount = 0;
              lastRateCalc = now;
            }

            // Log entry
            flightLog.push({
              time: new Date().toISOString(),
              met: ((now - metStart) / 1000).toFixed(1),
              temp: d.temp,
              press: d.press,
              alt: d.alt,
              roll: d.roll,
              pitch: d.pitch,
              heading: d.heading
            });

            // 3D satellite attitude targets (Pitch on X, Roll on Z, Heading on Y)
            targetPitch = ((d.pitch || 0) * Math.PI) / 180;
            targetRoll  = ((d.roll  || 0) * Math.PI) / 180;
            targetYaw   = ((d.heading || 0) * Math.PI) / 180;
            document.getElementById('sat-pitch').innerText=(d.pitch || 0).toFixed(1);
            document.getElementById('sat-roll').innerText=(d.roll || 0).toFixed(1);
            document.getElementById('sat-head').innerText=Math.round(d.heading || 0);

            // Dynamics
            document.getElementById('pitch').innerText=d.pitch.toFixed(1);
            document.getElementById('roll').innerText=d.roll.toFixed(1);
            document.getElementById('pitch-status').innerText = Math.abs(d.pitch) > 45 ? 'TUMBLED' : 'STABLE';
            document.getElementById('roll-status').innerText = Math.abs(d.roll) > 45 ? 'TUMBLED' : 'STABLE';
            pushAndDraw('pitch',d.pitch);
            pushAndDraw('roll',d.roll);

            // Altitude & V-Speed
            document.getElementById('alt').innerText=d.alt.toFixed(1);
            if (d.alt > maxAltitude) {
              maxAltitude = d.alt;
              document.getElementById('alt-max').innerText = maxAltitude.toFixed(1) + ' m';
            }
            if (lastAlt !== null && lastAltTime !== null) {
              const dt = (now - lastAltTime) / 1000.0;
              if (dt > 0.1) {
                const instantV = (d.alt - lastAlt) / dt;
                vSpeed = (vSpeed * 0.65) + (instantV * 0.35); // Filtered
                const prefix = vSpeed >= 0 ? '+' : '';
                document.getElementById('alt-vspeed').innerText = prefix + vSpeed.toFixed(1) + ' m/s';
              }
            }
            lastAlt = d.alt;
            lastAltTime = now;
            pushAndDraw('alt',d.alt);

            // Pressure Stats
            document.getElementById('press').innerText=d.press.toFixed(2);
            if (d.press < minPress) minPress = d.press;
            if (d.press > maxPress) maxPress = d.press;
            document.getElementById('press-min').innerText = minPress.toFixed(1);
            document.getElementById('press-max').innerText = maxPress.toFixed(1);
            pushAndDraw('press',d.press);

            // Temperature Stats
            document.getElementById('temp').innerText=d.temp.toFixed(1);
            if (d.temp < minTemp) minTemp = d.temp;
            if (d.temp > maxTemp) maxTemp = d.temp;
            document.getElementById('temp-min').innerText = minTemp.toFixed(1) + '°';
            document.getElementById('temp-max').innerText = maxTemp.toFixed(1) + '°';
            pushAndDraw('temp',d.temp);

            // Compass
            drawCompass(d.heading);
            document.getElementById('heading-deg').innerText=Math.round(d.heading);
            document.getElementById('heading-dir').innerText=bearingLabel(d.heading);
          } else {
            dot.className = 'dot lost';
            indicator.className = 'status-indicator lost';
            text.innerText = 'LINK LOST \u26A0\uFE0F';
            grid.className = 'grid grid-container lost';
            document.getElementById('pkt-rate').innerText = '0.0';
          }
        })
        .catch(()=>{});
    }, 400);
  </script>
</body>
</html>
)rawliteral";

void setup() {
  delay(100);
  Serial.begin(115200);
  Serial2.begin(9600, SERIAL_8N1, RXD2, TXD2);

  Serial.println("\nInitializing ESP32 Receiver Dashboard...");

  // Setup Wi-Fi Access Point
  WiFi.mode(WIFI_AP);
  WiFi.softAP(ssid, password);
  Serial.print("AP IP address: ");
  Serial.println(WiFi.softAPIP());

  // Captive Portal DNS — any domain → ESP32
  dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());

  // Serve the main HTML page
  server.on("/", HTTP_GET, []() {
    server.send(200, "text/html", index_html);
  });

  // Serve 3D Libraries & Model (Gzipped for fast, 100% offline flight loading)
  server.on("/three.min.js", HTTP_GET, []() {
    server.sendHeader("Content-Encoding", "gzip");
    server.send_P(200, "application/javascript", (const char*)three_min_js_gz, three_min_js_gz_len);
  });

  server.on("/GLTFLoader.js", HTTP_GET, []() {
    server.sendHeader("Content-Encoding", "gzip");
    server.send_P(200, "application/javascript", (const char*)gltf_loader_js_gz, gltf_loader_js_gz_len);
  });

  server.on("/satellite.glb", HTTP_GET, []() {
    server.sendHeader("Content-Encoding", "gzip");
    server.send_P(200, "model/gltf-binary", (const char*)satellite_glb_gz, satellite_glb_gz_len);
  });

  // Serve live JSON data
  server.on("/data", HTTP_GET, []() {
    bool isConnected = (millis() - lastRxTime) < 3000;
    String response = latestJson;
    // Strip the last '}' and add the connected status
    if (response.endsWith("}")) {
      response = response.substring(0, response.length() - 1);
      response += ",\"connected\":" + String(isConnected ? "true" : "false") + "}";
    }
    server.send(200, "application/json", response);
  });

  // Redirect any other URL to dashboard (captive portal)
  server.onNotFound([]() {
    server.sendHeader("Location", String("http://") + WiFi.softAPIP().toString(), true);
    server.send(302, "text/plain", "");
  });

  server.begin();
  Serial.println("Web server started successfully!");
}

void loop() {
  dnsServer.processNextRequest();
  server.handleClient();

  if (Serial2.available()) {
    String incomingJson = Serial2.readStringUntil('\n');
    incomingJson.trim();

    if (incomingJson.length() > 0) {
      StaticJsonDocument<256> doc;
      DeserializationError error = deserializeJson(doc, incomingJson);

      if (!error) {
        latestJson = incomingJson;
        lastRxTime = millis(); // Reset the watchdog timer
        Serial.println(latestJson);
      } else {
        Serial.println("JSON parse error");
      }
    }
  }
}