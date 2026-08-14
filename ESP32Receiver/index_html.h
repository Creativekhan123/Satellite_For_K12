#ifndef INDEX_HTML_H
#define INDEX_HTML_H

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>CanSat Dashboard</title>
  <script src="https://cdn.jsdelivr.net/npm/chart.js"></script>
  <style>
    :root {
      --bg: #0f172a;
      --panel: #1e293b;
      --text: #f8fafc;
      --accent: #38bdf8;
      --accent2: #818cf8;
      --danger: #ef4444;
      --success: #10b981;
    }
    body {
      margin: 0;
      font-family: 'Inter', -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, Helvetica, Arial, sans-serif;
      background: var(--bg);
      color: var(--text);
    }
    header {
      background: linear-gradient(135deg, var(--panel), #0f172a);
      padding: 20px 40px;
      display: flex;
      justify-content: space-between;
      align-items: center;
      box-shadow: 0 4px 20px rgba(0,0,0,0.5);
      border-bottom: 1px solid rgba(255,255,255,0.1);
    }
    h1 {
      margin: 0;
      font-size: 24px;
      letter-spacing: 2px;
      font-weight: 700;
      background: linear-gradient(90deg, var(--accent), var(--accent2));
      -webkit-background-clip: text;
      -webkit-text-fill-color: transparent;
    }
    .status {
      display: flex;
      align-items: center;
      gap: 12px;
      font-weight: 600;
      font-size: 14px;
      color: #94a3b8;
    }
    .indicator {
      width: 12px;
      height: 12px;
      border-radius: 50%;
      background: var(--danger);
      box-shadow: 0 0 12px var(--danger);
      transition: all 0.3s ease;
    }
    .indicator.active {
      background: var(--success);
      box-shadow: 0 0 12px var(--success);
    }
    .container {
      max-width: 1200px;
      margin: 0 auto;
      padding: 30px 20px;
    }
    .grid {
      display: grid;
      grid-template-columns: repeat(auto-fit, minmax(240px, 1fr));
      gap: 25px;
      margin-bottom: 30px;
    }
    .card {
      background: var(--panel);
      border-radius: 20px;
      padding: 25px;
      box-shadow: 0 10px 30px rgba(0,0,0,0.2);
      transition: transform 0.3s ease, box-shadow 0.3s ease;
      border: 1px solid rgba(255,255,255,0.05);
      position: relative;
      overflow: hidden;
    }
    .card::before {
      content: '';
      position: absolute;
      top: 0; left: 0; right: 0;
      height: 4px;
      background: linear-gradient(90deg, var(--accent), var(--accent2));
      opacity: 0.8;
    }
    .card:hover {
      transform: translateY(-5px);
      box-shadow: 0 15px 35px rgba(0,0,0,0.4);
    }
    .card h3 {
      margin: 0 0 15px 0;
      color: #94a3b8;
      font-size: 13px;
      text-transform: uppercase;
      letter-spacing: 1.5px;
      font-weight: 600;
    }
    .value {
      font-size: 42px;
      font-weight: 700;
      margin: 0;
      color: #fff;
    }
    .unit {
      font-size: 18px;
      color: #64748b;
      font-weight: 500;
    }
    .chart-container {
      background: var(--panel);
      border-radius: 20px;
      padding: 25px;
      box-shadow: 0 10px 30px rgba(0,0,0,0.2);
      border: 1px solid rgba(255,255,255,0.05);
    }
    .chart-container h3 {
      margin: 0 0 20px 0;
      color: #94a3b8;
      font-size: 14px;
      text-transform: uppercase;
      letter-spacing: 1px;
    }
    .orientation-widget {
      display: flex;
      justify-content: space-between;
      text-align: center;
      gap: 15px;
    }
    .angle-box {
      background: rgba(0,0,0,0.2);
      padding: 15px 10px;
      border-radius: 12px;
      flex: 1;
      border: 1px solid rgba(255,255,255,0.02);
    }
    .angle-box .label {
      font-size: 11px;
      color: #64748b;
      letter-spacing: 1px;
      margin-bottom: 5px;
    }
    .angle-box .value {
      font-size: 28px;
    }
    @media (max-width: 768px) {
      .value { font-size: 32px; }
      .angle-box .value { font-size: 24px; }
    }
  </style>
</head>
<body>
  <header>
    <h1>🚀 CanSat Telemetry</h1>
    <div class="status">
      <span>DATA LINK</span>
      <div id="rf-indicator" class="indicator"></div>
    </div>
  </header>
  
  <div class="container">
    <div class="grid">
      <div class="card">
        <h3>Orientation (IMU)</h3>
        <div class="orientation-widget">
          <div class="angle-box">
            <div class="label">PITCH</div>
            <div class="value" id="val-pitch">0&deg;</div>
          </div>
          <div class="angle-box">
            <div class="label">ROLL</div>
            <div class="value" id="val-roll">0&deg;</div>
          </div>
        </div>
      </div>
      <div class="card">
        <h3>Temperature</h3>
        <div class="value"><span id="val-temp">--</span><span class="unit">&deg;C</span></div>
      </div>
      <div class="card">
        <h3>Humidity</h3>
        <div class="value"><span id="val-hum">--</span><span class="unit">%</span></div>
      </div>
      <div class="card">
        <h3>Gas Resistance</h3>
        <div class="value"><span id="val-gas">--</span><span class="unit">k&Omega;</span></div>
      </div>
    </div>

    <div class="chart-container">
      <h3>Live Barometric Pressure (hPa)</h3>
      <canvas id="pressureChart" height="80"></canvas>
    </div>
  </div>

  <script>
    // Initialize Chart.js
    const ctx = document.getElementById('pressureChart').getContext('2d');
    const pressureChart = new Chart(ctx, {
      type: 'line',
      data: {
        labels: [],
        datasets: [{
          label: 'Pressure (hPa)',
          borderColor: '#38bdf8',
          backgroundColor: 'rgba(56, 189, 248, 0.1)',
          data: [],
          borderWidth: 2,
          pointRadius: 0,
          pointHoverRadius: 4,
          fill: true,
          tension: 0.4
        }]
      },
      options: {
        responsive: true,
        animation: {
          duration: 0
        },
        scales: {
          x: { 
            display: false,
            grid: { display: false }
          },
          y: { 
            grid: { color: 'rgba(255,255,255,0.05)' },
            ticks: { color: '#94a3b8' },
            beginAtZero: false
          }
        },
        plugins: { 
          legend: { display: false },
          tooltip: {
            mode: 'index',
            intersect: false,
            backgroundColor: 'rgba(15, 23, 42, 0.9)'
          }
        },
        interaction: {
          mode: 'nearest',
          axis: 'x',
          intersect: false
        }
      }
    });

    // WebSocket Connection
    let ws;
    const indicator = document.getElementById('rf-indicator');
    let timeout;

    function connect() {
      ws = new WebSocket(`ws://${window.location.hostname}/ws`);
      
      ws.onmessage = (event) => {
        try {
          const data = JSON.parse(event.data);
          
          // Update DOM Elements
          document.getElementById('val-pitch').innerHTML = data.pitch.toFixed(1) + '&deg;';
          document.getElementById('val-roll').innerHTML = data.roll.toFixed(1) + '&deg;';
          document.getElementById('val-temp').innerText = data.temp.toFixed(1);
          document.getElementById('val-hum').innerText = data.hum.toFixed(1);
          document.getElementById('val-gas').innerText = data.gas.toFixed(1);
          
          // Update Chart
          const time = new Date().toLocaleTimeString();
          if(pressureChart.data.labels.length > 50) {
            pressureChart.data.labels.shift();
            pressureChart.data.datasets[0].data.shift();
          }
          pressureChart.data.labels.push(time);
          pressureChart.data.datasets[0].data.push(data.press);
          pressureChart.update();

          // RF Indicator visual pulse
          indicator.classList.add('active');
          clearTimeout(timeout);
          timeout = setTimeout(() => {
            indicator.classList.remove('active');
          }, 1500); // Set to inactive if no data for 1.5s

        } catch(e) {
          console.error("JSON parsing error:", e);
        }
      };

      ws.onclose = () => {
        indicator.classList.remove('active');
        setTimeout(connect, 2000); // Reconnect after 2 seconds
      };
    }

    // Start connection
    connect();
  </script>
</body>
</html>
)rawliteral";

#endif
