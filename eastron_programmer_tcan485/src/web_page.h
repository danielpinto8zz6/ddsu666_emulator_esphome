#pragma once

const char EASTON_WEB_PAGE[] PROGMEM = R"rawliteral(<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>⚡ Eastron SDM120 Programmer — LILYGO T-CAN485</title>
  <style>
    :root {
      --bg: #0f172a;
      --card: #1e293b;
      --card-border: #334155;
      --text: #f8fafc;
      --muted: #94a3b8;
      --accent: #38bdf8;
      --accent-hover: #0284c7;
      --emerald: #10b981;
      --amber: #f59e0b;
      --rose: #ef4444;
    }
    * { box-sizing: border-box; margin: 0; padding: 0; }
    body {
      font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif;
      background: var(--bg);
      color: var(--text);
      line-height: 1.5;
      padding: 1.5rem;
      max-width: 800px;
      margin: 0 auto;
    }
    h1 { font-size: 1.5rem; font-weight: 700; color: #fff; margin-bottom: 0.25rem; display: flex; align-items: center; gap: 0.5rem; }
    .subtitle { color: var(--muted); font-size: 0.85rem; margin-bottom: 1.5rem; }
    .card {
      background: var(--card);
      border: 1px solid var(--card-border);
      border-radius: 12px;
      padding: 1.25rem;
      margin-bottom: 1.25rem;
      box-shadow: 0 4px 6px -1px rgba(0,0,0,0.2);
    }
    .card-title {
      font-size: 1.05rem;
      font-weight: 600;
      margin-bottom: 0.75rem;
      display: flex;
      align-items: center;
      gap: 0.5rem;
      color: var(--accent);
    }
    .grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(200px, 1fr)); gap: 0.75rem; }
    label { font-size: 0.8rem; color: var(--muted); display: block; margin-bottom: 0.25rem; }
    input, select {
      width: 100%;
      background: #0f172a;
      border: 1px solid var(--card-border);
      color: #fff;
      padding: 0.5rem 0.75rem;
      border-radius: 6px;
      font-size: 0.9rem;
      outline: none;
    }
    input:focus, select:focus { border-color: var(--accent); }
    .btn {
      background: var(--accent);
      color: #0f172a;
      border: none;
      border-radius: 6px;
      padding: 0.6rem 1.2rem;
      font-weight: 600;
      font-size: 0.9rem;
      cursor: pointer;
      display: inline-flex;
      align-items: center;
      gap: 0.4rem;
      transition: all 0.15s ease;
    }
    .btn:hover { background: var(--accent-hover); }
    .btn-emerald { background: var(--emerald); color: #fff; }
    .btn-emerald:hover { background: #059669; }
    .btn-outline { background: transparent; border: 1px solid var(--card-border); color: var(--text); }
    .btn-outline:hover { background: rgba(255,255,255,0.05); border-color: var(--accent); }
    .btn:disabled { opacity: 0.5; cursor: not-allowed; }
    .log-box {
      background: #090d16;
      border: 1px solid #1e293b;
      border-radius: 8px;
      padding: 0.85rem;
      font-family: ui-monospace, SFMono-Regular, Menlo, monospace;
      font-size: 0.8rem;
      color: #a5f3fc;
      white-space: pre-wrap;
      max-height: 250px;
      overflow-y: auto;
      margin-top: 0.75rem;
    }
    .stat-val { font-size: 1.4rem; font-weight: 700; color: #fff; }
    .stat-lbl { font-size: 0.75rem; color: var(--muted); }
    .notice {
      background: rgba(245, 158, 11, 0.1);
      border: 1px solid rgba(245, 158, 11, 0.3);
      border-radius: 8px;
      padding: 0.75rem 1rem;
      font-size: 0.82rem;
      color: #fde68a;
      margin-bottom: 1rem;
    }
    .badge {
      display: inline-block;
      padding: 0.2rem 0.5rem;
      border-radius: 999px;
      font-size: 0.72rem;
      font-weight: 600;
      background: rgba(16, 185, 129, 0.2);
      color: var(--emerald);
    }
  </style>
</head>
<body>
  <h1>⚡ Eastron SDM120 Modbus Programmer</h1>
  <div class="subtitle">Hardware: LILYGO T-CAN485 (ESP32 RS-485 transceiver)</div>

  <div class="notice">
    <strong>⚠️ Important Setup Instructions:</strong><br>
    1. Long-press the front push-button on the Eastron SDM120 for 3–5 seconds until <strong>-SET-</strong> appears on the LCD screen.<br>
    2. Click <strong>Quick Program (9600 Baud / ID 3)</strong> below.<br>
    3. When complete, power cycle the meter (trip circuit breaker for 3 seconds) for the changes to apply!
  </div>

  <!-- Quick Setup Card -->
  <div class="card">
    <div class="card-title">🚀 Quick Setup (Recommended for Hoymiles Bus)</div>
    <p style="font-size: 0.85rem; color: var(--muted); margin-bottom: 1rem;">
      Auto-detects your Eastron meter and configures it to <strong>Slave ID 3</strong> at <strong>9600 bps</strong> so it can coexist on the RS-485 bus alongside Hoymiles (IDs 1 &amp; 2).
    </p>
    <div style="display: flex; gap: 0.75rem; flex-wrap: wrap; align-items: center;">
      <button class="btn btn-emerald" id="btn-quick" onclick="quickProgram()">⚡ Quick Program (ID: 3, 9600 Baud)</button>
      <button class="btn btn-outline" id="btn-scan" onclick="scanBus()">🔍 Auto-Scan Bus</button>
      <span id="quick-status" style="font-size: 0.82rem; color: var(--muted);"></span>
    </div>
  </div>

  <!-- Custom Setup Card -->
  <div class="card">
    <div class="card-title">🛠️ Custom Modbus Parameters</div>
    <div class="grid">
      <div>
        <label>Current / Detected Slave ID</label>
        <input type="number" id="cur-id" value="0" min="0" max="247">
        <small style="color: var(--muted); font-size: 0.7rem;">0 = Auto-scan (1..8)</small>
      </div>
      <div>
        <label>Current / Detected Baud Rate</label>
        <select id="cur-baud">
          <option value="0">0 = Auto-detect</option>
          <option value="2400" selected>2400 bps (Factory Default)</option>
          <option value="9600">9600 bps</option>
          <option value="4800">4800 bps</option>
          <option value="1200">1200 bps</option>
        </select>
      </div>
      <div>
        <label>Target Slave ID (New)</label>
        <input type="number" id="new-id" value="3" min="1" max="247">
      </div>
      <div>
        <label>Target Baud Rate (New)</label>
        <select id="new-baud">
          <option value="9600" selected>9600 bps (Standard for Hoymiles)</option>
          <option value="2400">2400 bps</option>
          <option value="4800">4800 bps</option>
          <option value="19200">19200 bps</option>
        </select>
      </div>
    </div>
    <div style="margin-top: 1rem;">
      <button class="btn" id="btn-custom" onclick="customProgram()">Write Parameters</button>
    </div>
  </div>

  <!-- Live Telemetry Card -->
  <div class="card">
    <div class="card-title" style="justify-content: space-between;">
      <span>📊 Live Meter Telemetry (Verification)</span>
      <button class="btn btn-outline" style="padding: 0.3rem 0.75rem; font-size: 0.78rem;" onclick="readTelemetry()">🔄 Read Now</button>
    </div>
    <div class="grid" style="grid-template-columns: repeat(4, 1fr); margin-top: 0.5rem;">
      <div>
        <div class="stat-lbl">Voltage</div>
        <div class="stat-val" id="val-v">-- V</div>
      </div>
      <div>
        <div class="stat-lbl">Active Power</div>
        <div class="stat-val" id="val-w">-- W</div>
      </div>
      <div>
        <div class="stat-lbl">Current</div>
        <div class="stat-val" id="val-a">-- A</div>
      </div>
      <div>
        <div class="stat-lbl">Frequency</div>
        <div class="stat-val" id="val-hz">-- Hz</div>
      </div>
    </div>
  </div>

  <!-- Diagnostic Output Log -->
  <div class="card">
    <div class="card-title">📝 Modbus Console &amp; Output Log</div>
    <div class="log-box" id="log-box">Ready. Click "Quick Program" or "Auto-Scan Bus" to begin.</div>
  </div>

  <script>
    function setLog(txt) {
      const box = document.getElementById('log-box');
      box.textContent = txt;
      box.scrollTop = box.scrollHeight;
    }
    function appendLog(txt) {
      const box = document.getElementById('log-box');
      box.textContent += '\n' + txt;
      box.scrollTop = box.scrollHeight;
    }

    async function quickProgram() {
      const btn = document.getElementById('btn-quick');
      btn.disabled = true;
      setLog('🚀 Starting Quick Program to ID 3 @ 9600 baud...');
      try {
        const res = await fetch('/api/program?cur_id=0&cur_baud=0&new_id=3&new_baud=9600', { method: 'POST' });
        const d = await res.json();
        setLog(d.log || 'Done');
        if (d.success) {
          alert('Success! Please power cycle the Eastron meter for 3 seconds to complete setup.');
        }
      } catch (e) {
        appendLog('Error: ' + e.message);
      } finally {
        btn.disabled = false;
      }
    }

    async function scanBus() {
      const btn = document.getElementById('btn-scan');
      btn.disabled = true;
      setLog('🔍 Scanning Modbus bus across 2400/9600/4800/1200 baud & IDs 1..8...');
      try {
        const res = await fetch('/api/scan');
        const d = await res.json();
        setLog(d.log || 'Scan finished.');
        if (d.found && d.found.length > 0) {
          const m = d.found[0];
          document.getElementById('cur-id').value = m.id;
          document.getElementById('cur-baud').value = m.baud;
          readTelemetry(m.id, m.baud);
        }
      } catch (e) {
        appendLog('Error: ' + e.message);
      } finally {
        btn.disabled = false;
      }
    }

    async function customProgram() {
      const curId = document.getElementById('cur-id').value;
      const curBaud = document.getElementById('cur-baud').value;
      const newId = document.getElementById('new-id').value;
      const newBaud = document.getElementById('new-baud').value;
      const btn = document.getElementById('btn-custom');
      btn.disabled = true;
      setLog(`Writing parameters: Cur ID ${curId} (${curBaud} baud) -> New ID ${newId} (${newBaud} baud)...`);
      try {
        const res = await fetch(`/api/program?cur_id=${curId}&cur_baud=${curBaud}&new_id=${newId}&new_baud=${newBaud}`, { method: 'POST' });
        const d = await res.json();
        setLog(d.log || 'Done');
      } catch (e) {
        appendLog('Error: ' + e.message);
      } finally {
        btn.disabled = false;
      }
    }

    async function readTelemetry(id, baud) {
      const tid = id || document.getElementById('cur-id').value || 3;
      const tbaud = baud || document.getElementById('cur-baud').value || 9600;
      try {
        const res = await fetch(`/api/read?id=${tid}&baud=${tbaud}`);
        const d = await res.json();
        if (d.valid) {
          document.getElementById('val-v').textContent = d.voltage.toFixed(1) + ' V';
          document.getElementById('val-w').textContent = d.active_power.toFixed(1) + ' W';
          document.getElementById('val-a').textContent = d.current.toFixed(2) + ' A';
          document.getElementById('val-hz').textContent = d.frequency.toFixed(1) + ' Hz';
          appendLog(`Read OK (ID ${tid}): ${d.voltage.toFixed(1)}V, ${d.active_power.toFixed(1)}W, ${d.current.toFixed(2)}A, ${d.frequency.toFixed(1)}Hz`);
        } else {
          appendLog(`Read attempt failed for ID ${tid} @ ${tbaud} baud (Is meter powered & connected?)`);
        }
      } catch (e) {
        appendLog('Read error: ' + e.message);
      }
    }
  </script>
</body>
</html>
)rawliteral";
