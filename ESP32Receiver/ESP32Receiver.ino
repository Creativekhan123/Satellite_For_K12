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
  <title>K12-SAT // Mission Control</title>
  <script src="/three.min.js"></script>
  <script src="/GLTFLoader.js"></script>
  <script>
    if (typeof THREE === 'undefined') {
      const s1 = document.createElement('script');
      s1.src = 'https://cdnjs.cloudflare.com/ajax/libs/three.js/r128/three.min.js';
      document.head.appendChild(s1);
      const s2 = document.createElement('script');
      s2.src = 'https://cdn.jsdelivr.net/npm/three@0.128.0/examples/js/loaders/GLTFLoader.js';
      document.head.appendChild(s2);
    }
  </script>
  <style>
    /* Share Tech Mono served locally via fallback chain — no CDN needed in field deployment */
    /* @import url('https://fonts.googleapis.com/css2?family=Share+Tech+Mono&display=swap'); */
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
      align-items:center;
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
    /* Fullscreen & HUD buttons */
    .btn-fullscreen, .btn-hud {
      background: transparent;
      border: 1px solid var(--border-color);
      color: var(--text-dim);
      font-family: inherit;
      font-size: 11px;
      line-height: 1;
      padding: 5px 9px;
      cursor: pointer;
      border-radius: 3px;
      transition: all 0.2s;
      flex-shrink: 0;
      text-transform: uppercase;
      letter-spacing: 1px;
    }
    .btn-fullscreen { font-size: 15px; padding: 4px 8px; }
    .btn-fullscreen:hover, .btn-hud:hover {
      border-color: var(--accent-color);
      color: var(--accent-color);
      box-shadow: 0 0 8px rgba(0,255,234,0.4);
    }
    .btn-hud.active {
      border-color: var(--accent-color);
      color: var(--accent-color);
      background: rgba(0,255,234,0.12);
      box-shadow: 0 0 8px rgba(0,255,234,0.35);
    }
    /* Audio button in mission bar */
    .audio-btn {
      border-color: var(--border-color);
      color: var(--text-dim);
      display: inline-flex;
      align-items: center;
      gap: 6px;
      transition: all 0.3s ease;
    }
    .audio-btn.active {
      border-color: var(--accent-color);
      color: var(--accent-color);
      background: rgba(0, 255, 234, 0.12);
      box-shadow: 0 0 10px rgba(0, 255, 234, 0.4);
    }
    .audio-btn:hover {
      border-color: var(--accent-color);
      color: var(--accent-color);
    }
    /* Fullscreen overlay: the canvas wrapper becomes fullscreen */
    .sat-3d-wrapper {
      position: relative;
      width: 100%;
      height: 380px;
      overflow: hidden;
    }
    /* PFD Artificial Horizon HUD Canvas */
    #pfd-hud-canvas {
      position: absolute;
      inset: 0;
      width: 100%;
      height: 100%;
      pointer-events: none;
      z-index: 4;
    }
    /* 3D model load error overlay */
    #model-error {
      display: none;
      position: absolute;
      inset: 0;
      background: rgba(3,10,22,0.88);
      color: var(--danger-color);
      font-size: 14px;
      letter-spacing: 1px;
      text-transform: uppercase;
      align-items: center;
      justify-content: center;
      flex-direction: column;
      gap: 8px;
      z-index: 15;
      pointer-events: none;
    }
    #model-error.show { display: flex; }
    #model-error span { font-size: 36px; }
    /* Fullscreen: Native API + CSS Fallback (.is-fullscreen) for iOS Safari & Mobile WebViews */
    .sat-3d-wrapper:fullscreen,
    .sat-3d-wrapper:-webkit-full-screen,
    .sat-3d-wrapper:-moz-full-screen,
    .sat-3d-wrapper.is-fullscreen {
      position: fixed !important;
      inset: 0 !important;
      top: 0 !important;
      left: 0 !important;
      width: 100vw !important;
      height: 100vh !important;
      max-width: 100vw !important;
      max-height: 100vh !important;
      z-index: 99999 !important;
      background: #030a16 !important;
      margin: 0 !important;
      padding: 0 !important;
      border: none !important;
    }
    /* When fullscreen, canvas fills the wrapper */
    .sat-3d-wrapper:fullscreen #sat-3d-canvas,
    .sat-3d-wrapper:-webkit-full-screen #sat-3d-canvas,
    .sat-3d-wrapper:-moz-full-screen #sat-3d-canvas,
    .sat-3d-wrapper.is-fullscreen #sat-3d-canvas,
    .sat-3d-wrapper:fullscreen #pfd-hud-canvas,
    .sat-3d-wrapper:-webkit-full-screen #pfd-hud-canvas,
    .sat-3d-wrapper:-moz-full-screen #pfd-hud-canvas,
    .sat-3d-wrapper.is-fullscreen #pfd-hud-canvas {
      width: 100% !important;
      height: 100% !important;
    }
    /* Fullscreen overlay toolbar — shown ONLY when in fullscreen */
    #fs-overlay {
      display: none;
      position: absolute;
      top: 0; left: 0; right: 0;
      z-index: 20;
      padding: 12px 16px;
      background: linear-gradient(to bottom, rgba(3,10,22,0.92) 0%, transparent 100%);
      pointer-events: none;
      align-items: center;
      justify-content: space-between;
      gap: 12px;
      flex-wrap: wrap;
    }
    .sat-3d-wrapper:fullscreen #fs-overlay,
    .sat-3d-wrapper:-webkit-full-screen #fs-overlay,
    .sat-3d-wrapper:-moz-full-screen #fs-overlay,
    .sat-3d-wrapper.is-fullscreen #fs-overlay { display: flex !important; }
    #fs-overlay-title {
      font-size: 13px;
      color: var(--accent-color);
      text-shadow: 0 0 10px rgba(0,255,234,0.5);
      text-transform: uppercase;
      letter-spacing: 2px;
    }
    #fs-overlay-angles {
      display: flex;
      gap: 18px;
      font-size: 12px;
      color: var(--text-dim);
    }
    #fs-overlay-angles strong { color: var(--accent-color); }
    #fs-exit-btn {
      pointer-events: all;
      background: rgba(3,10,22,0.85);
      border: 1px solid var(--border-color);
      color: var(--accent-color);
      font-family: inherit;
      font-size: 13px;
      padding: 6px 14px;
      cursor: pointer;
      border-radius: 3px;
      display: flex;
      align-items: center;
      gap: 6px;
      letter-spacing: 1px;
      text-transform: uppercase;
      transition: all 0.2s;
    }
    #fs-exit-btn:hover {
      background: var(--accent-color);
      color: #030a16;
      box-shadow: 0 0 12px rgba(0,255,234,0.6);
    }
    /* Bottom mode badge inside fullscreen */
    #fs-badge {
      display: none;
      position: absolute;
      bottom: 14px;
      left: 50%;
      transform: translateX(-50%);
      font-size: 11px;
      background: rgba(0,255,234,0.1);
      border: 1px solid var(--border-color);
      color: var(--accent-color);
      padding: 5px 14px;
      border-radius: 3px;
      pointer-events: none;
      z-index: 20;
      text-transform: uppercase;
      letter-spacing: 1.5px;
      white-space: nowrap;
    }
    .sat-3d-wrapper:fullscreen #fs-badge,
    .sat-3d-wrapper:-webkit-full-screen #fs-badge,
    .sat-3d-wrapper:-moz-full-screen #fs-badge,
    .sat-3d-wrapper.is-fullscreen #fs-badge { display: block !important; }
    @media (max-width: 600px) {
      .sat-header { flex-direction: column; align-items: flex-start; gap: 6px; }
      .sat-angles { gap: 10px; font-size: 12px; }
      .sat-footer { flex-direction: column; align-items: flex-start; gap: 6px; }
      .sat-hint   { font-size: 9px; }
      /* hide title on small phones in fullscreen so exit btn stays visible */
      .sat-3d-wrapper:fullscreen #fs-overlay-title,
      .sat-3d-wrapper:-webkit-full-screen #fs-overlay-title,
      .sat-3d-wrapper:-moz-full-screen #fs-overlay-title,
      .sat-3d-wrapper.is-fullscreen #fs-overlay-title { display: none !important; }
      #fs-overlay-angles { font-size: 10px; gap: 10px; }
    }
    /* Toast notification */
    #toast {
      position: fixed;
      bottom: 28px;
      left: 50%;
      transform: translateX(-50%) translateY(20px);
      background: rgba(10,25,46,0.96);
      border: 1px solid var(--border-color);
      color: var(--accent-color);
      font-size: 12px;
      padding: 10px 22px;
      border-radius: 4px;
      letter-spacing: 1px;
      text-transform: uppercase;
      opacity: 0;
      pointer-events: none;
      z-index: 9999;
      transition: opacity 0.3s ease, transform 0.3s ease;
      white-space: nowrap;
    }
    #toast.show {
      opacity: 1;
      transform: translateX(-50%) translateY(0);
    }
    /* Attitude status colour coding */
    .status-stable  { color: var(--success-color); }
    .status-tilted  { color: var(--warn-color); }
    .status-tumbled { color: var(--danger-color); }
    /* Responsive compass canvas */
    #compass-canvas { max-width: 100%; max-height: 100%; }

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
        <h1>K12-SAT // Mission Control</h1>
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
        <div style="display:flex; gap:8px; align-items:center; flex-wrap:wrap;">
          <button id="btn-audio-toggle" class="action-btn audio-btn" onclick="toggleAudio()" title="Toggle Comms Audio & Voice Alerts">
            <span id="audio-icon">🔇</span> <span id="audio-label">AUDIO: OFF</span>
          </button>
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
          <button class="btn-hud active" id="btn-hud-toggle" onclick="toggleHUD()" title="Toggle Artificial Horizon HUD">✈ HUD: ON</button>
          <button class="btn-fullscreen" id="btn-fullscreen" onclick="toggleFullscreen()" title="Fullscreen">&#x26F6;</button>
        </div>
      </div>
      <div class="sat-3d-wrapper" id="sat-3d-wrapper">
        <canvas id="sat-3d-canvas" style="width:100%; height:100%; display:block; cursor:grab;"></canvas>
        <canvas id="pfd-hud-canvas"></canvas>
        <!-- Model load error overlay -->
        <div id="model-error"><span>&#x26A0;</span>3D Model Failed to Load<br><small style="font-size:11px;color:#8ba9c9;">Check flash storage or re-upload firmware</small></div>
        <!-- Fullscreen overlay toolbar (top bar) -->
        <div id="fs-overlay">
          <div id="fs-overlay-title">&#x1F6F0; K12-SAT // Live Attitude</div>
          <div id="fs-overlay-angles">
            <span>Pitch: <strong id="fs-pitch">0.0</strong>&deg;</span>
            <span>Roll: <strong id="fs-roll">0.0</strong>&deg;</span>
            <span>Heading: <strong id="fs-head">0</strong>&deg;</span>
          </div>
          <div style="display:flex; gap:8px; align-items:center; pointer-events:all;">
            <button id="fs-hud-btn" class="action-btn secondary" onclick="toggleHUD()">HUD: ON</button>
            <button id="fs-exit-btn" onclick="toggleFullscreen()" title="Exit Fullscreen">&#x2715; Exit Fullscreen</button>
          </div>
        </div>
        <!-- Fullscreen bottom badge -->
        <div id="fs-badge">&#x25CF; Tracking Live Attitude &bull; Drag to Inspect</div>
      </div>
      <div class="sat-footer">
        <div id="sat-mode-badge" class="sat-badge">&#x25CF; TRACKING LIVE ATTITUDE</div>
        <div class="sat-hint">&#x1F4F1; Drag / Swipe to inspect &bull; ✈ Toggle HUD &bull; &#x26F6; Fullscreen</div>
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
      <div class="section-title">&#x25C6; Direction &amp; Heading (BMM-350 Compass)</div>

      <!-- Heading sparkline card -->
      <div class="card">
        <h3>Heading</h3>
        <div class="val-container"><div class="val"><span id="head-val">---</span><span class="unit">&deg;</span></div></div>
        <div class="sub-stats">
          <div>DIR: <strong id="head-dir-card">---</strong></div>
          <div>MIN: <strong id="head-min">---</strong></div>
          <div>MAX: <strong id="head-max">---</strong></div>
        </div>
        <canvas class="sparkline" id="canvas-head" width="240" height="55"></canvas>
      </div>

      <div class="card card-compass">
        <h3>Compass Rose (Tilt-Compensated)</h3>
        <div style="display:flex; align-items:center; gap:20px; justify-content:center; flex-wrap:wrap; width:100%; padding:8px 0;">
          <canvas id="compass-canvas" width="180" height="180" style="max-width:180px; max-height:180px;"></canvas>
          <div style="text-align:center;">
            <div style="font-size:11px; color:var(--text-dim); text-transform:uppercase; letter-spacing:1.5px; margin-bottom:6px;">Bearing</div>
            <div style="font-size:48px; color:var(--accent-color); text-shadow:0 0 14px rgba(0,255,234,0.45); font-weight:bold;">
              <span id="heading-deg">---</span><span style="font-size:20px; color:var(--text-dim);">&deg;</span>
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
      if (audioEnabled) speakVoice("Mission elapsed time reset.");
      showToast("Mission Clock Reset", false);
    }

    // ── Toast notification (replaces alert()) ─────────────────
    let toastTimer;
    function showToast(msg, isError) {
      const t = document.getElementById('toast');
      t.innerText = msg;
      t.style.borderColor = isError ? 'var(--danger-color)' : 'var(--accent-color)';
      t.style.color       = isError ? 'var(--danger-color)' : 'var(--accent-color)';
      t.classList.add('show');
      clearTimeout(toastTimer);
      toastTimer = setTimeout(() => t.classList.remove('show'), 3000);
    }

    function exportCSV() {
      if (flightLog.length === 0) {
        showToast('No flight telemetry logged yet!', true);
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
      a.download = 'k12sat_telemetry_' + new Date().toISOString().slice(0,19).replace(/[:T]/g,'_') + '.csv';
      document.body.appendChild(a);
      a.click();
      document.body.removeChild(a);
      URL.revokeObjectURL(url);
      showToast('CSV Exported! (' + flightLog.length + ' packets)', false);
    }

    // =====================================================
    // REAL-TIME 3D SATELLITE & PFD HUD ENGINE
    // =====================================================
    const canvas3D  = document.getElementById('sat-3d-canvas');
    const hudCanvas = document.getElementById('pfd-hud-canvas');
    const badge3D   = document.getElementById('sat-mode-badge');
    const scene3D   = new THREE.Scene();
    scene3D.background = new THREE.Color(0x030a16);

    let hudEnabled = true;
    function toggleHUD() {
      hudEnabled = !hudEnabled;
      const btns = [document.getElementById('btn-hud-toggle'), document.getElementById('fs-hud-btn')];
      btns.forEach(b => {
        if (b) {
          b.innerText = hudEnabled ? '✈ HUD: ON' : '✈ HUD: OFF';
          b.classList.toggle('active', hudEnabled);
        }
      });
      if (!hudEnabled && hudCanvas) {
        const ctx = hudCanvas.getContext('2d');
        ctx.clearRect(0, 0, hudCanvas.width, hudCanvas.height);
      }
      showToast(hudEnabled ? 'PFD Artificial Horizon HUD Active' : 'HUD Disabled', false);
    }

    // ── Star field ────────────────────────────────────────────
    (function addStarField() {
      const count = 300;
      const positions = new Float32Array(count * 3);
      for (let i = 0; i < count * 3; i++) {
        positions[i] = (Math.random() - 0.5) * 1200;
      }
      const geo = new THREE.BufferGeometry();
      geo.setAttribute('position', new THREE.BufferAttribute(positions, 3));
      const mat = new THREE.PointsMaterial({ color: 0xaaccff, size: 1.2, sizeAttenuation: true, transparent: true, opacity: 0.7 });
      scene3D.add(new THREE.Points(geo, mat));
    })();

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
      // Show visible error overlay so the user knows what happened
      const errEl = document.getElementById('model-error');
      if (errEl) errEl.classList.add('show');
    });

    // ── Drag helpers ──────────────────────────────────────────
    function startDrag(x, y) {
      isDragging = true;
      lastPointerX = x;
      lastPointerY = y;
      badge3D.innerText = '◐ USER INSPECTION (RELEASE TO ALIGN)';
      badge3D.style.borderColor = 'var(--warn-color)';
      badge3D.style.color = 'var(--warn-color)';
      canvas3D.style.cursor = 'grabbing';
    }
    function moveDrag(x, y) {
      if (!isDragging) return;
      const dx = x - lastPointerX;
      const dy = y - lastPointerY;
      lastPointerX = x;
      lastPointerY = y;
      manualYawOffset   += dx * 0.012;
      manualPitchOffset += dy * 0.012;
    }
    function endDrag() {
      if (!isDragging) return;
      isDragging = false;
      badge3D.innerText = '\u25CF TRACKING LIVE ATTITUDE';
      badge3D.style.borderColor = 'var(--border-color)';
      badge3D.style.color = 'var(--accent-color)';
      canvas3D.style.cursor = 'grab';
    }

    // ── Mouse events ──────────────────────────────────────────
    canvas3D.addEventListener('mousedown', e => { startDrag(e.clientX, e.clientY); });
    window.addEventListener('mousemove',   e => { moveDrag(e.clientX, e.clientY); });
    window.addEventListener('mouseup',     () => endDrag());

    // ── Touch events (passive:false so we can preventDefault) ─
    canvas3D.addEventListener('touchstart', e => {
      e.preventDefault();  // stop scroll hijacking the drag
      if (e.touches.length === 1) startDrag(e.touches[0].clientX, e.touches[0].clientY);
    }, { passive: false });

    canvas3D.addEventListener('touchmove', e => {
      e.preventDefault();
      if (e.touches.length === 1) moveDrag(e.touches[0].clientX, e.touches[0].clientY);
    }, { passive: false });

    canvas3D.addEventListener('touchend',    () => endDrag(), { passive: true });
    canvas3D.addEventListener('touchcancel', () => endDrag(), { passive: true });

    // ── Bulletproof Fullscreen Engine (Native + CSS Fallback for iOS & Mobile WebViews) ──
    function getSatWrapper() {
      return document.getElementById('sat-3d-wrapper');
    }

    function isFullscreenActive() {
      const wrap = getSatWrapper();
      return !!(document.fullscreenElement || document.webkitFullscreenElement || document.mozFullScreenElement || document.msFullscreenElement) ||
             (wrap && wrap.classList.contains('is-fullscreen'));
    }

    function updateFsUI(inFs) {
      const btn = document.getElementById('btn-fullscreen');
      if (btn) {
        btn.innerText = inFs ? '\u2715' : '\u26F6';
        btn.title = inFs ? 'Exit Fullscreen' : 'Fullscreen';
      }
      setTimeout(resize3D, 50);
      setTimeout(resize3D, 200);
    }

    function toggleFullscreen() {
      const wrap = getSatWrapper();
      if (!wrap) return;
      const inFs = isFullscreenActive();
      if (!inFs) {
        // Enter Fullscreen
        try {
          const req = wrap.requestFullscreen || wrap.webkitRequestFullscreen || wrap.mozRequestFullScreen || wrap.msRequestFullscreen;
          if (req) {
            const p = req.call(wrap);
            if (p && p.catch) p.catch(() => {});
          }
        } catch (e) {}

        wrap.classList.add('is-fullscreen');
        document.body.style.overflow = 'hidden';
        updateFsUI(true);
      } else {
        // Exit Fullscreen
        try {
          const ex = document.exitFullscreen || document.webkitExitFullscreen || document.mozCancelFullScreen || document.msExitFullscreen;
          if (ex && (document.fullscreenElement || document.webkitFullscreenElement || document.mozFullScreenElement || document.msFullscreenElement)) {
            const p = ex.call(document);
            if (p && p.catch) p.catch(() => {});
          }
        } catch (e) {}

        wrap.classList.remove('is-fullscreen');
        document.body.style.overflow = '';
        updateFsUI(false);
      }
    }

    function onFsChange() {
      const wrap = getSatWrapper();
      const nativeInFs = !!(document.fullscreenElement || document.webkitFullscreenElement || document.mozFullScreenElement || document.msFullscreenElement);
      if (!nativeInFs && wrap && wrap.classList.contains('is-fullscreen')) {
        wrap.classList.remove('is-fullscreen');
        document.body.style.overflow = '';
      }
      updateFsUI(isFullscreenActive());
    }

    document.addEventListener('fullscreenchange',       onFsChange);
    document.addEventListener('webkitfullscreenchange', onFsChange);
    document.addEventListener('mozfullscreenchange',    onFsChange);
    document.addEventListener('MSFullscreenChange',     onFsChange);

    // Keep the fullscreen overlay angles in sync
    function syncFsOverlay(pitch, roll, heading) {
      const fp = document.getElementById('fs-pitch');
      const fr = document.getElementById('fs-roll');
      const fh = document.getElementById('fs-head');
      if (fp) fp.innerText = pitch.toFixed(1);
      if (fr) fr.innerText = roll.toFixed(1);
      if (fh) fh.innerText = Math.round(heading);
    }

    // ── Handle Window / Fullscreen Resize ─────────────────────
    function resize3D() {
      const parent = canvas3D.parentElement;
      if (parent && parent.clientWidth > 0 && parent.clientHeight > 0) {
        camera3D.aspect = parent.clientWidth / parent.clientHeight;
        camera3D.updateProjectionMatrix();
        renderer3D.setSize(parent.clientWidth, parent.clientHeight);
        if (hudCanvas) {
          hudCanvas.width = parent.clientWidth;
          hudCanvas.height = parent.clientHeight;
        }
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

    // ── PFD Artificial Horizon HUD Drawing Engine ─────────────
    function drawHUD(pitch, roll, heading) {
      if (!hudEnabled || !hudCanvas) return;
      if (hudCanvas.width !== hudCanvas.clientWidth || hudCanvas.height !== hudCanvas.clientHeight) {
        hudCanvas.width = hudCanvas.clientWidth;
        hudCanvas.height = hudCanvas.clientHeight;
      }
      const ctx = hudCanvas.getContext('2d');
      const W = hudCanvas.width, H = hudCanvas.height;
      ctx.clearRect(0, 0, W, H);

      const cx = W / 2;
      const cy = H / 2;
      const pitchScale = 3.2; // pixels per degree
      const hudColor = 'rgba(0, 255, 234, 0.88)';
      const hudDim   = 'rgba(0, 255, 234, 0.35)';

      ctx.save();

      // 1. Center Boresight Aircraft Reticle (Fixed to screen center)
      ctx.strokeStyle = '#00ffea';
      ctx.lineWidth = 2;
      ctx.shadowColor = '#00ffea';
      ctx.shadowBlur = 6;
      ctx.beginPath();
      ctx.arc(cx, cy, 3, 0, Math.PI * 2);
      ctx.fillStyle = '#00ffea';
      ctx.fill();
      ctx.beginPath();
      ctx.moveTo(cx - 30, cy); ctx.lineTo(cx - 10, cy);
      ctx.moveTo(cx + 10, cy); ctx.lineTo(cx + 30, cy);
      ctx.moveTo(cx, cy - 4); ctx.lineTo(cx, cy - 10);
      ctx.stroke();
      ctx.shadowBlur = 0;

      // 2. Roll Arc at Top
      const arcR = Math.min(W, H) * 0.38;
      const arcTopY = cy;
      ctx.strokeStyle = hudDim;
      ctx.lineWidth = 1.5;
      ctx.beginPath();
      ctx.arc(cx, arcTopY, arcR, -Math.PI / 2 - Math.PI / 3, -Math.PI / 2 + Math.PI / 3);
      ctx.stroke();

      // Roll ticks: 0, ±10, ±20, ±30, ±45, ±60
      const rollTicks = [-60, -45, -30, -20, -10, 0, 10, 20, 30, 45, 60];
      rollTicks.forEach(deg => {
        const rad = (-90 + deg) * Math.PI / 180;
        const isMajor = deg === 0 || Math.abs(deg) === 30 || Math.abs(deg) === 60;
        const tLen = isMajor ? 9 : 5;
        const x1 = cx + Math.cos(rad) * arcR;
        const y1 = arcTopY + Math.sin(rad) * arcR;
        const x2 = cx + Math.cos(rad) * (arcR + tLen);
        const y2 = arcTopY + Math.sin(rad) * (arcR + tLen);
        ctx.beginPath();
        ctx.moveTo(x1, y1); ctx.lineTo(x2, y2);
        ctx.strokeStyle = isMajor ? '#00ffea' : hudDim;
        ctx.stroke();
      });

      // Roll Pointer (rotates with roll around top arc)
      const curRollRad = (-90 + roll) * Math.PI / 180;
      const ptrX = cx + Math.cos(curRollRad) * (arcR - 2);
      const ptrY = arcTopY + Math.sin(curRollRad) * (arcR - 2);
      ctx.save();
      ctx.translate(ptrX, ptrY);
      ctx.rotate(curRollRad + Math.PI / 2);
      ctx.beginPath();
      ctx.moveTo(0, 0);
      ctx.lineTo(-5, -8);
      ctx.lineTo(5, -8);
      ctx.closePath();
      ctx.fillStyle = '#00ffea';
      ctx.shadowColor = '#00ffea'; ctx.shadowBlur = 8;
      ctx.fill();
      ctx.restore();

      // Roll readout above arc
      ctx.font = '11px Share Tech Mono, monospace';
      ctx.fillStyle = '#00ffea';
      ctx.textAlign = 'center';
      ctx.fillText(`BANK ${roll >= 0 ? '+' : ''}${roll.toFixed(1)}°`, cx, arcTopY - arcR - 12);

      // 3. Pitch Ladder & Horizon (Rotates with -Roll and translates with Pitch)
      ctx.save();
      ctx.beginPath();
      ctx.rect(cx - 180, cy - 130, 360, 260);
      ctx.clip();

      ctx.translate(cx, cy);
      ctx.rotate(-roll * Math.PI / 180);
      ctx.translate(0, pitch * pitchScale);

      // Horizon line (Pitch 0°)
      ctx.strokeStyle = '#00ffea';
      ctx.lineWidth = 2;
      ctx.shadowColor = '#00ffea'; ctx.shadowBlur = 6;
      ctx.beginPath();
      ctx.moveTo(-150, 0); ctx.lineTo(-38, 0);
      ctx.moveTo(38, 0);  ctx.lineTo(150, 0);
      ctx.moveTo(-150, 0); ctx.lineTo(-150, 6);
      ctx.moveTo(150, 0);  ctx.lineTo(150, 6);
      ctx.stroke();
      ctx.shadowBlur = 0;

      ctx.font = '10px Share Tech Mono, monospace';
      ctx.fillStyle = '#00ffea';
      ctx.textAlign = 'right';
      ctx.fillText('00', -42, 3);
      ctx.textAlign = 'left';
      ctx.fillText('00', 42, 3);

      // Pitch rungs: ±10°, ±20°, ±30°, ±40°, ±50°, ±60°
      for (let d = -60; d <= 60; d += 10) {
        if (d === 0) continue;
        const y = -d * pitchScale;
        const isUp = d > 0;
        const w = 42;
        const gap = 18;
        const tab = isUp ? 5 : -5;

        ctx.beginPath();
        if (!isUp) ctx.setLineDash([3, 3]);
        else ctx.setLineDash([]);

        ctx.strokeStyle = isUp ? hudColor : 'rgba(255, 184, 77, 0.85)';
        ctx.lineWidth = 1.4;

        ctx.moveTo(-w - gap, y);
        ctx.lineTo(-gap, y);
        ctx.lineTo(-gap, y + tab);

        ctx.moveTo(gap, y + tab);
        ctx.lineTo(gap, y);
        ctx.lineTo(w + gap, y);
        ctx.stroke();

        ctx.setLineDash([]);
        ctx.fillStyle = isUp ? hudColor : 'rgba(255, 184, 77, 0.85)';
        ctx.textAlign = 'right';
        ctx.fillText(Math.abs(d).toString(), -gap - w - 4, y + 3);
        ctx.textAlign = 'left';
        ctx.fillText(Math.abs(d).toString(), gap + w + 4, y + 3);
      }
      ctx.restore(); // end pitch ladder clip

      // 4. Tactical Data Corners
      // Heading Indicator at bottom
      ctx.fillStyle = 'rgba(10, 25, 46, 0.7)';
      ctx.strokeStyle = 'rgba(0, 90, 143, 0.6)';
      ctx.lineWidth = 1;
      const bW = 130, bH = 22;
      ctx.fillRect(cx - bW / 2, H - 28, bW, bH);
      ctx.strokeRect(cx - bW / 2, H - 28, bW, bH);
      ctx.fillStyle = '#00ffea';
      ctx.font = '11px Share Tech Mono, monospace';
      ctx.textAlign = 'center';
      const dirs = ['N','NE','E','SE','S','SW','W','NW'];
      const dirTxt = dirs[Math.round(heading / 45) % 8];
      ctx.fillText(`HDG ${Math.round(heading)}° [ ${dirTxt} ]`, cx, H - 13);

      // Pitch readout (left)
      ctx.fillStyle = 'rgba(10, 25, 46, 0.7)';
      ctx.fillRect(10, cy - 14, 80, 28);
      ctx.strokeRect(10, cy - 14, 80, 28);
      ctx.fillStyle = '#00ffea';
      ctx.fillText(`P ${pitch >= 0 ? '+' : ''}${pitch.toFixed(1)}°`, 50, cy + 4);

      // Roll / Attitude status (right)
      const absRoll = Math.abs(roll);
      let attText = 'STABLE', attColor = '#00ff66';
      if (absRoll > 45 || Math.abs(pitch) > 45) { attText = 'TUMBLED'; attColor = '#ff4d4d'; }
      else if (absRoll > 15 || Math.abs(pitch) > 15) { attText = 'TILTED'; attColor = '#ffb84d'; }

      ctx.fillStyle = 'rgba(10, 25, 46, 0.7)';
      ctx.strokeStyle = attColor;
      ctx.fillRect(W - 95, cy - 14, 85, 28);
      ctx.strokeRect(W - 95, cy - 14, 85, 28);
      ctx.fillStyle = attColor;
      ctx.fillText(attText, W - 52, cy + 4);

      ctx.restore();
    }

    // Animation loop with smooth interpolation and spring snap-back
    function animate3D() {
      requestAnimationFrame(animate3D);

      if (satModel) {
        // Buttery-smooth interpolation towards sensor telemetry targets
        // lerpAngle() always takes the SHORTEST arc, fixing the 359°→0° wrap-around bug
        const smoothSpeed = 0.08;
        currentPitch = lerpAngle(currentPitch, targetPitch, smoothSpeed);
        currentRoll  = lerpAngle(currentRoll,  targetRoll,  smoothSpeed);
        currentYaw   = lerpAngle(currentYaw,   targetYaw,   smoothSpeed);

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

      // Render PFD Artificial Horizon HUD synchronized with 3D model & user inspection
      const dispPitchDeg = ((currentPitch + manualPitchOffset) * 180) / Math.PI;
      const dispRollDeg  = (currentRoll * 180) / Math.PI;
      const dispYawDeg   = (((currentYaw + manualYawOffset) * 180) / Math.PI + 360) % 360;
      drawHUD(dispPitchDeg, dispRollDeg, dispYawDeg);
    }
    animate3D();

    // =====================================================
    // SPARKLINE GRAPH ENGINE
    // =====================================================
    const MAX_POINTS = 30;
    const hist = { pitch:[], roll:[], temp:[], press:[], alt:[], head:[] };

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
      // Draw baseline zero line if zero is within range
      if (mn <= 0 && mx >= 0) {
        const zy = H - ((0 - mn)/rng)*H*0.78 - H*0.11;
        ctx.beginPath();
        ctx.strokeStyle='rgba(0,255,234,0.18)';
        ctx.lineWidth=1;
        ctx.setLineDash([4,4]);
        ctx.moveTo(0,zy); ctx.lineTo(W,zy);
        ctx.stroke();
        ctx.setLineDash([]);
      }
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
      // Needle — negated so CW physical rotation → CW needle rotation
      const nAng=-(heading+90)*Math.PI/180;
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
    // COMMS AUDIO & VOICE SYNTHESIS ENGINE (Web Audio API)
    // =====================================================
    let audioCtx = null;
    let audioEnabled = false;
    let lastTumbleAlertTime = 0;
    let lastVoiceTime = 0;

    function initAudio() {
      if (!audioCtx) {
        const AudioContext = window.AudioContext || window.webkitAudioContext;
        if (AudioContext) audioCtx = new AudioContext();
      }
      if (audioCtx && audioCtx.state === 'suspended') {
        audioCtx.resume();
      }
    }

    function toggleAudio() {
      initAudio();
      audioEnabled = !audioEnabled;
      const btn = document.getElementById('btn-audio-toggle');
      const icon = document.getElementById('audio-icon');
      const label = document.getElementById('audio-label');

      if (btn) btn.classList.toggle('active', audioEnabled);
      if (icon) icon.innerText = audioEnabled ? '🔊' : '🔇';
      if (label) label.innerText = audioEnabled ? 'AUDIO: ACTIVE' : 'AUDIO: OFF';

      if (audioEnabled) {
        playChime([523.25, 659.25, 783.99]);
        speakVoice("Comms audio online. Telemetry link active.");
        showToast("Comms Audio & Voice Alerts Active", false);
      } else {
        if ('speechSynthesis' in window) window.speechSynthesis.cancel();
        showToast("Comms Audio Muted", false);
      }
    }

    function playTone(freq, type = 'sine', duration = 0.1, gainVal = 0.08) {
      if (!audioEnabled || !audioCtx) return;
      try {
        const osc = audioCtx.createOscillator();
        const gain = audioCtx.createGain();
        osc.type = type;
        osc.frequency.setValueAtTime(freq, audioCtx.currentTime);
        gain.gain.setValueAtTime(gainVal, audioCtx.currentTime);
        gain.gain.exponentialRampToValueAtTime(0.0001, audioCtx.currentTime + duration);
        osc.connect(gain);
        gain.connect(audioCtx.destination);
        osc.start();
        osc.stop(audioCtx.currentTime + duration);
      } catch (e) {}
    }

    function playChime(freqs, delay = 0.09) {
      if (!audioEnabled || !audioCtx) return;
      freqs.forEach((f, i) => {
        setTimeout(() => playTone(f, 'sine', 0.2, 0.08), i * delay * 1000);
      });
    }

    function playWarningAlarm() {
      if (!audioEnabled || !audioCtx) return;
      playTone(880, 'sawtooth', 0.12, 0.09);
      setTimeout(() => playTone(660, 'sawtooth', 0.16, 0.09), 130);
    }

    function playPacketChirp() {
      // Subtle 12ms telemetry chirp
      playTone(2200, 'sine', 0.015, 0.025);
    }

    function speakVoice(text) {
      if (!audioEnabled || !('speechSynthesis' in window)) return;
      const now = Date.now();
      if (now - lastVoiceTime < 4000) return; // 4s cooldown
      lastVoiceTime = now;
      try {
        window.speechSynthesis.cancel();
        const utt = new SpeechSynthesisUtterance(text);
        utt.rate = 1.05;
        utt.pitch = 1.0;
        utt.volume = 1.0;
        window.speechSynthesis.speak(utt);
      } catch (e) {}
    }

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
            
            // Audio telemetry pulse & link restoration
            playPacketChirp();
            if (window._linkWasLost) {
              window._linkWasLost = false;
              playChime([440, 554, 659]);
              speakVoice("Telemetry link restored.");
            }

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
            syncFsOverlay(d.pitch || 0, d.roll || 0, d.heading || 0);

            // Dynamics — 3-state attitude status
            document.getElementById('pitch').innerText=d.pitch.toFixed(1);
            document.getElementById('roll').innerText=d.roll.toFixed(1);
            function attitudeStatus(el, deg) {
              const abs = Math.abs(deg);
              if (abs > 45)      { el.innerText='TUMBLED'; el.className='status-tumbled'; }
              else if (abs > 15) { el.innerText='TILTED';  el.className='status-tilted';  }
              else               { el.innerText='STABLE';  el.className='status-stable';  }
            }
            attitudeStatus(document.getElementById('pitch-status'), d.pitch);
            attitudeStatus(document.getElementById('roll-status'),  d.roll);
            pushAndDraw('pitch',d.pitch);
            pushAndDraw('roll',d.roll);

            // Audio warning if satellite attitude tumbles (> 45°)
            if (Math.abs(d.pitch) > 45 || Math.abs(d.roll) > 45) {
              if (now - lastTumbleAlertTime > 12000) {
                lastTumbleAlertTime = now;
                playWarningAlarm();
                speakVoice("Warning: Satellite attitude critical.");
              }
            }

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

            // Compass + heading card
            drawCompass(d.heading);
            const hRound = Math.round(d.heading);
            const hLabel = bearingLabel(d.heading);
            document.getElementById('heading-deg').innerText = hRound;
            document.getElementById('heading-dir').innerText = hLabel;
            // Heading card
            document.getElementById('head-val').innerText = hRound;
            document.getElementById('head-dir-card').innerText = hLabel;
            if (!window.minHead || d.heading < window.minHead) { window.minHead=d.heading; document.getElementById('head-min').innerText=Math.round(window.minHead)+'\u00b0'; }
            if (!window.maxHead || d.heading > window.maxHead) { window.maxHead=d.heading; document.getElementById('head-max').innerText=Math.round(window.maxHead)+'\u00b0'; }
            pushAndDraw('head', d.heading);
          } else {
            dot.className = 'dot lost';
            indicator.className = 'status-indicator lost';
            text.innerText = 'LINK LOST \u26A0\uFE0F';
            grid.className = 'grid grid-container lost';
            document.getElementById('pkt-rate').innerText = '0.0';
            // Alert on mobile & audio when link is lost (only once per loss event)
            if (!window._linkWasLost) {
              window._linkWasLost = true;
              showToast('\u26A0\uFE0F RF Link Lost!', true);
              if (navigator.vibrate) navigator.vibrate([200, 100, 200, 100, 200]);
              playTone(330, 'square', 0.35, 0.09);
              speakVoice("Alert: Telemetry link lost.");
            }
          }
          if (d.connected) window._linkWasLost = false;
        })
        .catch(()=>{});
    }, 400);
  </script>
  <!-- Toast element (global, always in DOM) -->
  <div id="toast"></div>
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