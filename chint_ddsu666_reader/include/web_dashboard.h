#pragma once

#include <Arduino.h>

// Web UI HTML Dashboard for Chint DDSU666 Modbus Master
const char CHINT_DASHBOARD_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Chint DDSU666 Modbus Master</title>
  <style>
    :root {
      --bg: #0f172a;
      --card-bg: rgba(30, 41, 59, 0.7);
      --border: rgba(255, 255, 255, 0.08);
      --accent: #38bdf8;
      --accent-grad: linear-gradient(135deg, #38bdf8, #818cf8);
      --text: #f8fafc;
      --muted: #94a3b8;
      --green: #10b981;
      --red: #f43f5e;
      --amber: #f59e0b;
    }
    * { box-sizing: border-box; margin: 0; padding: 0; font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif; }
    body { background-color: var(--bg); color: var(--text); padding: 20px; min-height: 100vh; display: flex; flex-direction: column; align-items: center; }
    .container { width: 100%; max-width: 900px; display: flex; flex-direction: column; gap: 20px; }
    header { display: flex; justify-content: space-between; align-items: center; padding-bottom: 10px; border-bottom: 1px solid var(--border); flex-wrap: wrap; gap: 10px; }
    .title-box h1 { font-size: 1.4rem; font-weight: 700; background: var(--accent-grad); -webkit-background-clip: text; -webkit-text-fill-color: transparent; }
    .title-box p { font-size: 0.85rem; color: var(--muted); margin-top: 4px; }
    .badge { padding: 4px 10px; border-radius: 9999px; font-size: 0.75rem; font-weight: 600; text-transform: uppercase; letter-spacing: 0.05em; }
    .badge-online { background: rgba(16, 185, 129, 0.2); color: var(--green); border: 1px solid rgba(16, 185, 129, 0.3); }
    .badge-offline { background: rgba(244, 63, 94, 0.2); color: var(--red); border: 1px solid rgba(244, 63, 94, 0.3); }
    .badge-stream { background: rgba(56, 189, 248, 0.2); color: var(--accent); border: 1px solid rgba(56, 189, 248, 0.4); }
    .hero-card { background: var(--card-bg); border: 1px solid var(--border); border-radius: 16px; padding: 30px; text-align: center; position: relative; backdrop-filter: blur(12px); box-shadow: 0 10px 25px -5px rgba(0,0,0,0.3); }
    .hero-value { font-size: 3.8rem; font-weight: 800; letter-spacing: -0.03em; margin: 10px 0; }
    .hero-unit { font-size: 1.5rem; color: var(--muted); margin-left: 4px; }
    .hero-flow { font-size: 1rem; font-weight: 600; display: inline-flex; align-items: center; gap: 6px; padding: 4px 14px; border-radius: 20px; }
    .flow-import { background: rgba(56, 189, 248, 0.15); color: var(--accent); border: 1px solid rgba(56, 189, 248, 0.3); }
    .flow-export { background: rgba(16, 185, 129, 0.15); color: var(--green); border: 1px solid rgba(16, 185, 129, 0.3); }
    .grid-2 { display: grid; grid-template-columns: repeat(auto-fit, minmax(200px, 1fr)); gap: 15px; }
    .grid-4 { display: grid; grid-template-columns: repeat(auto-fit, minmax(130px, 1fr)); gap: 12px; }
    .card { background: var(--card-bg); border: 1px solid var(--border); border-radius: 12px; padding: 16px; backdrop-filter: blur(10px); }
    .card-label { font-size: 0.75rem; color: var(--muted); text-transform: uppercase; font-weight: 600; margin-bottom: 6px; }
    .card-val { font-size: 1.5rem; font-weight: 700; color: var(--text); }
    .card-unit { font-size: 0.85rem; color: var(--muted); margin-left: 3px; }
    .diag-box { background: rgba(15, 23, 42, 0.6); border-radius: 10px; padding: 14px; border: 1px solid var(--border); font-size: 0.85rem; }
    .diag-row { display: flex; justify-content: space-between; padding: 4px 0; border-bottom: 1px solid rgba(255,255,255,0.03); }
    .diag-row:last-child { border-bottom: none; }
    .diag-label { color: var(--muted); }
    .diag-val { font-weight: 600; }
    .btn { background: rgba(255,255,255,0.06); border: 1px solid var(--border); color: var(--text); padding: 7px 14px; border-radius: 8px; cursor: pointer; font-size: 0.82rem; font-weight: 600; transition: all 0.2s; text-decoration: none; display: inline-flex; align-items: center; gap: 4px; }
    .btn:hover { background: rgba(255,255,255,0.12); }
    .btn-live-on { background: rgba(16, 185, 129, 0.15); color: var(--green); border: 1px solid rgba(16, 185, 129, 0.4); display: inline-flex; align-items: center; gap: 6px; }
    .btn-live-on:hover { background: rgba(16, 185, 129, 0.25); }
    .btn-live-off { background: rgba(148, 163, 184, 0.12); color: var(--muted); border: 1px solid rgba(148, 163, 184, 0.3); display: inline-flex; align-items: center; gap: 6px; }
    .btn-live-off:hover { background: rgba(148, 163, 184, 0.2); color: var(--text); }
    .live-dot { width: 8px; height: 8px; border-radius: 50%; background: var(--green); box-shadow: 0 0 6px var(--green); display: inline-block; }
    .live-dot.paused { background: var(--muted); box-shadow: none; }
    .btn-primary { background: #0284c7; color: #fff; border-color: #0369a1; }
    .btn-primary:hover { background: #0369a1; }
    .btn-danger { background: rgba(244,63,94,0.15); color: var(--red); border-color: rgba(244,63,94,0.3); }
    .btn-danger:hover { background: rgba(244,63,94,0.3); }
    .modal { display: none; position: fixed; inset: 0; background: rgba(0,0,0,0.7); backdrop-filter: blur(5px); justify-content: center; align-items: center; z-index: 100; }
    .modal-box { background: #1e293b; border: 1px solid var(--border); border-radius: 16px; padding: 24px; width: 90%; max-width: 400px; box-shadow: 0 20px 25px -5px rgba(0,0,0,0.5); }
    .modal-box h3 { margin-bottom: 16px; font-size: 1.2rem; }
    .input-group { margin-bottom: 16px; }
    .input-group label { display: block; font-size: 0.8rem; color: var(--muted); margin-bottom: 6px; }
    .input-group input { width: 100%; padding: 10px; border-radius: 8px; background: #0f172a; border: 1px solid var(--border); color: #fff; font-size: 1rem; }
    .modal-actions { display: flex; justify-content: flex-end; gap: 10px; }
    footer { text-align: center; color: var(--muted); font-size: 0.8rem; margin-top: 20px; }
  </style>
</head>
<body>
  <div class="container">
    <header>
      <div class="title-box">
        <h1>Chint DDSU666 Modbus Master</h1>
        <p>Seeed Studio XIAO ESP32-S3 &bull; Ultra-Fast Polling &bull; WebSocket Stream</p>
      </div>
      <div style="display:flex; align-items:center; gap:8px; flex-wrap:wrap;">
        <button id="live-toggle-btn" class="btn btn-live-on" onclick="toggleLiveMode()" title="Toggle live reading updates">
          <span id="live-dot" class="live-dot"></span> <span id="live-text">Live: ON</span>
        </button>
        <span id="badge-stream" class="badge badge-stream">⚡ LIVE WS (15Hz)</span>
        <span id="badge-meter" class="badge badge-offline">Meter Offline</span>
        <button class="btn" onclick="openModal()">⚙️ Slave ID</button>
        <a href="/update" class="btn btn-primary">📦 OTA</a>
        <button class="btn btn-danger" onclick="rebootDevice()">↺ Reboot</button>
      </div>
    </header>

    <div class="hero-card">
      <div class="card-label">Active Grid Power</div>
      <div class="hero-value"><span id="val-watts">0.0</span><span class="hero-unit">W</span></div>
      <div id="badge-flow" class="hero-flow flow-import">Loading...</div>
    </div>

    <div class="grid-4">
      <div class="card">
        <div class="card-label">Voltage</div>
        <div class="card-val"><span id="val-volts">0.0</span><span class="card-unit">V</span></div>
      </div>
      <div class="card">
        <div class="card-label">Current</div>
        <div class="card-val"><span id="val-amps">0.00</span><span class="card-unit">A</span></div>
      </div>
      <div class="card">
        <div class="card-label">Power Factor</div>
        <div class="card-val"><span id="val-pf">1.00</span></div>
      </div>
      <div class="card">
        <div class="card-label">Frequency</div>
        <div class="card-val"><span id="val-freq">50.0</span><span class="card-unit">Hz</span></div>
      </div>
    </div>

    <div class="grid-2">
      <div class="card">
        <div class="card-label">Total Import Energy</div>
        <div class="card-val"><span id="val-imp">0.00</span><span class="card-unit">kWh</span></div>
      </div>
      <div class="card">
        <div class="card-label">Total Export Energy</div>
        <div class="card-val"><span id="val-exp">0.00</span><span class="card-unit">kWh</span></div>
      </div>
    </div>

    <div class="card">
      <div class="card-label" style="margin-bottom:12px;">Diagnostics &amp; Modbus Bus Health</div>
      <div class="diag-box">
        <div class="diag-row">
          <span class="diag-label">Modbus Polling Speed</span>
          <span class="diag-val" style="color:var(--accent);"><span id="val-hz">0.0</span> Hz</span>
        </div>
        <div class="diag-row">
          <span class="diag-label">Meter Response Latency</span>
          <span class="diag-val"><span id="val-lat">0.0</span> ms</span>
        </div>
        <div class="diag-row">
          <span class="diag-label">Modbus Slave ID / Baud Rate</span>
          <span class="diag-val">ID <span id="val-slave">1</span> &bull; 9600 8N1</span>
        </div>
        <div class="diag-row">
          <span class="diag-label">Connected WebSocket Clients</span>
          <span class="diag-val" style="color:var(--green);"><span id="val-clients">0</span> active</span>
        </div>
        <div class="diag-row">
          <span class="diag-label">WebSocket Stream Endpoint</span>
          <span class="diag-val"><span id="val-ws-url">ws://...:81/</span></span>
        </div>
        <div class="diag-row">
          <span class="diag-label">System Uptime / Wi-Fi RSSI</span>
          <span class="diag-val"><span id="val-uptime">0</span>s &bull; <span id="val-rssi">0</span> dBm</span>
        </div>
      </div>
    </div>

    <footer>
      Chint DDSU666 Modbus RTU Reader &bull; WebSocket Stream on Port 81 &bull; Health API: <a href="/health" style="color:var(--accent); text-decoration:none;">/health</a>
    </footer>
  </div>

  <div id="settings-modal" class="modal">
    <div class="modal-box">
      <h3>Modbus Settings</h3>
      <div class="input-group">
        <label>Chint DDSU666 Slave ID (Default 2 on this site)</label>
        <input type="number" id="input-slave" min="1" max="247" value="2">
      </div>
      <div class="modal-actions">
        <button class="btn" onclick="closeModal()">Cancel</button>
        <button class="btn btn-primary" onclick="saveSettings()">Save &amp; Apply</button>
      </div>
    </div>
  </div>

  <script>
    let ws = null;
    let wsTimer = null;
    let pollInterval = null;
    let isLiveMode = localStorage.getItem('chint_live_mode') !== 'false';
    const wsUrl = 'ws://' + window.location.hostname + ':81/';
    document.getElementById('val-ws-url').innerText = wsUrl;

    function updateLiveToggleUI() {
      const btn = document.getElementById('live-toggle-btn');
      const dot = document.getElementById('live-dot');
      const txt = document.getElementById('live-text');
      if (!btn || !dot || !txt) return;

      if (isLiveMode) {
        btn.className = 'btn btn-live-on';
        dot.className = 'live-dot';
        txt.innerText = 'Live: ON';
      } else {
        btn.className = 'btn btn-live-off';
        dot.className = 'live-dot paused';
        txt.innerText = 'Live: PAUSED';
      }
    }

    function toggleLiveMode() {
      isLiveMode = !isLiveMode;
      localStorage.setItem('chint_live_mode', isLiveMode ? 'true' : 'false');
      updateLiveToggleUI();

      if (isLiveMode) {
        setStreamStatus(true);
        connectWS();
        startPolling();
      } else {
        if (ws && ws.readyState === WebSocket.OPEN) {
          ws.close();
        }
        if (wsTimer) {
          clearTimeout(wsTimer);
          wsTimer = null;
        }
        stopPolling();
        setStreamStatus(false);
      }
    }

    function setStreamStatus(live) {
      const badge = document.getElementById('badge-stream');
      if (!badge) return;
      if (!isLiveMode) {
        badge.innerText = '⏸️ UPDATES PAUSED';
        badge.className = 'badge';
        badge.style = 'background:rgba(148,163,184,0.15); color:var(--muted); border:1px solid rgba(148,163,184,0.3);';
      } else if (live) {
        badge.innerText = '⚡ LIVE WS (15Hz)';
        badge.className = 'badge badge-stream';
        badge.style = '';
      } else {
        badge.innerText = '⏱ HTTP POLL (2s)';
        badge.className = 'badge';
        badge.style = 'background:rgba(245,158,11,0.2); color:var(--amber); border:1px solid rgba(245,158,11,0.4);';
      }
    }

    function connectWS() {
      if (!isLiveMode) return;
      try {
        ws = new WebSocket(wsUrl);
      } catch(e) {
        setStreamStatus(false);
        return;
      }

      ws.onopen = () => {
        setStreamStatus(true);
        if (wsTimer) { clearTimeout(wsTimer); wsTimer = null; }
      };

      ws.onmessage = (e) => {
        if (!isLiveMode) return;
        try {
          const data = JSON.parse(e.data);
          if (data.type === 'telemetry' || data.type === 'welcome') {
            updateUI(data);
          }
        } catch(err) {}
      };

      ws.onclose = () => {
        if (!isLiveMode) return;
        setStreamStatus(false);
        if (!wsTimer) {
          wsTimer = setTimeout(() => { wsTimer = null; if (isLiveMode) connectWS(); }, 2500);
        }
      };

      ws.onerror = (err) => {
        setStreamStatus(false);
        ws.close();
      };
    }

    function updateUI(d) {
      if (!isLiveMode || !d) return;
      if (d.watts !== undefined) document.getElementById('val-watts').innerText = d.watts.toFixed(1);
      if (d.volts !== undefined) document.getElementById('val-volts').innerText = d.volts.toFixed(1);
      if (d.amps !== undefined) document.getElementById('val-amps').innerText = d.amps.toFixed(2);
      if (d.pf !== undefined) document.getElementById('val-pf').innerText = d.pf.toFixed(2);
      if (d.freq !== undefined) document.getElementById('val-freq').innerText = d.freq.toFixed(1);
      if (d.import_kwh !== undefined) document.getElementById('val-imp').innerText = d.import_kwh.toFixed(2);
      if (d.export_kwh !== undefined) document.getElementById('val-exp').innerText = d.export_kwh.toFixed(2);
      if (d.poll_hz !== undefined) document.getElementById('val-hz').innerText = d.poll_hz.toFixed(1);
      if (d.latency_ms !== undefined) document.getElementById('val-lat').innerText = d.latency_ms.toFixed(1);
      if (d.slave_id !== undefined) document.getElementById('val-slave').innerText = d.slave_id;

      const meterBadge = document.getElementById('badge-meter');
      if (d.meter_online) {
        meterBadge.innerText = 'Meter Online';
        meterBadge.className = 'badge badge-online';
      } else {
        meterBadge.innerText = 'Meter Offline';
        meterBadge.className = 'badge badge-offline';
      }

      const flowBadge = document.getElementById('badge-flow');
      if (d.watts > 10) {
        flowBadge.innerText = 'Grid Import';
        flowBadge.className = 'hero-flow flow-import';
      } else if (d.watts < -10) {
        flowBadge.innerText = 'Solar Export';
        flowBadge.className = 'hero-flow flow-export';
      } else {
        flowBadge.innerText = 'Balanced';
        flowBadge.className = 'hero-flow';
      }
    }

    function pollHealth() {
      if (!isLiveMode) return;
      fetch('/health')
        .then(r => r.json())
        .then(data => {
          if (!isLiveMode) return;
          if (data.websocket) document.getElementById('val-clients').innerText = data.websocket.clients;
          if (data.uptime_sec !== undefined) document.getElementById('val-uptime').innerText = data.uptime_sec;
          if (data.wifi && data.wifi.rssi !== undefined) document.getElementById('val-rssi').innerText = data.wifi.rssi;
          if (!ws || ws.readyState !== WebSocket.OPEN) {
            updateUI({
              watts: data.telemetry.watts,
              volts: data.telemetry.volts,
              amps: data.telemetry.amps,
              pf: data.telemetry.pf,
              freq: data.telemetry.freq,
              import_kwh: data.telemetry.import_kwh,
              export_kwh: data.telemetry.export_kwh,
              poll_hz: data.modbus.rate_hz,
              latency_ms: data.modbus.latency_ms,
              meter_online: data.modbus.meter_online,
              slave_id: data.modbus.slave_id
            });
          }
        }).catch(e => {});
    }

    function startPolling() {
      if (!pollInterval && isLiveMode) {
        pollHealth();
        pollInterval = setInterval(pollHealth, 2000);
      }
    }

    function stopPolling() {
      if (pollInterval) {
        clearInterval(pollInterval);
        pollInterval = null;
      }
    }

    function openModal() {
      document.getElementById('settings-modal').style.display = 'flex';
      fetch('/health').then(r=>r.json()).then(d=>{
        if (d.modbus && d.modbus.slave_id) {
          document.getElementById('input-slave').value = d.modbus.slave_id;
        }
      });
    }

    function closeModal() {
      document.getElementById('settings-modal').style.display = 'none';
    }

    function saveSettings() {
      const slave = parseInt(document.getElementById('input-slave').value);
      if (ws && ws.readyState === WebSocket.OPEN) {
        ws.send(JSON.stringify({ action: 'set_slave', slave: slave }));
        closeModal();
        setTimeout(pollHealth, 500);
      } else {
        fetch('/api/settings?slave=' + slave, { method: 'POST' })
          .then(r => r.json())
          .then(d => {
            closeModal();
            pollHealth();
          }).catch(e => closeModal());
      }
    }

    function rebootDevice() {
      if (!confirm('Reboot the Chint Reader device?')) return;
      if (ws && ws.readyState === WebSocket.OPEN) {
        ws.send(JSON.stringify({ action: 'reboot' }));
      } else {
        fetch('/reboot', { method: 'POST' });
      }
      alert('Device rebooting... reloading page in 6s');
      setTimeout(() => location.reload(), 6000);
    }

    document.addEventListener("visibilitychange", () => {
      if (!isLiveMode) return;
      if (document.hidden) {
        if (ws && ws.readyState === WebSocket.OPEN) ws.close();
        stopPolling();
      } else {
        connectWS();
        startPolling();
      }
    });

    updateLiveToggleUI();
    if (isLiveMode) {
      connectWS();
      startPolling();
    } else {
      setStreamStatus(false);
    }
  </script>
</body>
</html>
)rawliteral";
