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
      gap:20px;
      font-size:13px;
      flex-wrap:wrap;
    }
    .sat-angles div { color:var(--text-dim); }
    .sat-angles strong { color:var(--accent-color); }
    #satellite-canvas {
      width:100%;
      display:block;
      height:210px;
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
        <span>&#x25B6; Live Attitude Visualizer // 3D Render</span>
        <div class="sat-angles">
          <div>Pitch: <strong><span id="sat-pitch">0.0</span>&deg;</strong></div>
          <div>Roll: <strong><span id="sat-roll">0.0</span>&deg;</strong></div>
          <div>Heading: <strong><span id="sat-head">0</span>&deg;</strong></div>
        </div>
      </div>
      <canvas id="satellite-canvas" width="800" height="210"></canvas>
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
    // 3D SATELLITE VISUALIZER ENGINE
    // =====================================================
    const satCanvas = document.getElementById('satellite-canvas');
    const satCtx    = satCanvas.getContext('2d');
    let livePitch = 0, liveRoll = 0, autoYaw = 0;

    function resizeSat() {
      const rect = satCanvas.getBoundingClientRect();
      if (rect.width > 0) {
        satCanvas.width = rect.width;
      }
    }
    window.addEventListener('resize', resizeSat);
    resizeSat();

    function makeSatVerts() {
      const b = [28, 18, 12];
      const pw = 45, ph = 2.5, pd = 22;
      return [
        [-b[0],-b[1],-b[2]], [ b[0],-b[1],-b[2]], [ b[0], b[1],-b[2]], [-b[0], b[1],-b[2]],
        [-b[0],-b[1], b[2]], [ b[0],-b[1], b[2]], [ b[0], b[1], b[2]], [-b[0], b[1], b[2]],
        [-b[0]-pw,-ph,-pd],[-b[0],-ph,-pd],[-b[0],ph,-pd],[-b[0]-pw,ph,-pd],
        [-b[0]-pw,-ph, pd],[-b[0],-ph, pd],[-b[0],ph, pd],[-b[0]-pw,ph, pd],
        [ b[0],-ph,-pd],[ b[0]+pw,-ph,-pd],[ b[0]+pw,ph,-pd],[ b[0],ph,-pd],
        [ b[0],-ph, pd],[ b[0]+pw,-ph, pd],[ b[0]+pw,ph, pd],[ b[0],ph, pd],
        [0,-b[1],0],[0,-b[1]-22,0]
      ];
    }
    function makeSatEdges() {
      const addBox = s => {
        const f = [];
        f.push([s,s+1],[s+1,s+2],[s+2,s+3],[s+3,s]);
        f.push([s+4,s+5],[s+5,s+6],[s+6,s+7],[s+7,s+4]);
        f.push([s,s+4],[s+1,s+5],[s+2,s+6],[s+3,s+7]);
        return f;
      };
      return [...addBox(0),...addBox(8),...addBox(16),[24,25]];
    }
    const SAT_VERTS = makeSatVerts();
    const SAT_EDGES = makeSatEdges();

    function rotateVert(v, pitch, roll, yaw) {
      let [x,y,z] = v;
      let x1=x*Math.cos(yaw)+z*Math.sin(yaw), z1=-x*Math.sin(yaw)+z*Math.cos(yaw);
      let y2=y*Math.cos(pitch)-z1*Math.sin(pitch), z2=y*Math.sin(pitch)+z1*Math.cos(pitch);
      let x3=x1*Math.cos(roll)-y2*Math.sin(roll), y3=x1*Math.sin(roll)+y2*Math.cos(roll);
      return [x3,y3,z2];
    }
    function project(v,cx,cy,fov){const s=fov/(fov+v[2]+50);return[cx+v[0]*s,cy+v[1]*s,s];}

    function drawSat() {
      const W=satCanvas.width,H=satCanvas.height,cx=W/2,cy=H/2,fov=260;
      const pR=livePitch*Math.PI/180, rR=liveRoll*Math.PI/180;
      satCtx.clearRect(0,0,W,H);
      const proj=SAT_VERTS.map(v=>project(rotateVert(v,pR,rR,autoYaw),cx,cy,fov));
      for(let pass=0;pass<2;pass++){
        satCtx.shadowColor=pass===0?'#00ffea':'transparent';
        satCtx.shadowBlur=pass===0?10:0;
        satCtx.strokeStyle='#00ffea';
        satCtx.lineWidth=pass===0?4:1.5;
        satCtx.globalAlpha=pass===0?0.25:1.0;
        for(const[a,b]of SAT_EDGES){satCtx.beginPath();satCtx.moveTo(proj[a][0],proj[a][1]);satCtx.lineTo(proj[b][0],proj[b][1]);satCtx.stroke();}
      }
      satCtx.globalAlpha=1.0; satCtx.shadowBlur=0;
      satCtx.fillStyle='#00ffea';
      for(let i=0;i<8;i++){satCtx.beginPath();satCtx.arc(proj[i][0],proj[i][1],2.5,0,Math.PI*2);satCtx.fill();}
      autoYaw+=0.008;
      requestAnimationFrame(drawSat);
    }
    drawSat();

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

            // 3D satellite
            livePitch=d.pitch; liveRoll=d.roll;
            document.getElementById('sat-pitch').innerText=d.pitch.toFixed(1);
            document.getElementById('sat-roll').innerText=d.roll.toFixed(1);
            document.getElementById('sat-head').innerText=Math.round(d.heading);

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