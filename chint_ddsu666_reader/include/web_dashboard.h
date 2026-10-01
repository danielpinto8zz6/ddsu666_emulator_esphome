#pragma once

#include <Arduino.h>

// Web UI HTML Dashboard for Chint DDSU666 Modbus Master using Pico.css & Alpine.js
const char CHINT_DASHBOARD_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en" data-theme="dark">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Chint DDSU666 Modbus Master</title>
  <!-- Pico.css Minimal & Semantic CSS Framework via CDN -->
  <link rel="stylesheet" href="https://cdn.jsdelivr.net/npm/@picocss/pico@2/css/pico.min.css">
  <!-- Alpine.js Lightweight Reactive UI Library via CDN -->
  <script defer src="https://cdn.jsdelivr.net/npm/alpinejs@3.x.x/dist/cdn.min.js"></script>
  <style>
    :root {
      --pico-font-size: 92%;
      --emerald: #10b981;
      --amber: #f59e0b;
      --rose: #f43f5e;
      --accent: #38bdf8;
    }
    body {
      padding-top: 1rem;
      padding-bottom: 3rem;
    }
    /* Device Cross-Link Switcher */
    .device-nav {
      display: flex;
      align-items: center;
      gap: 0.5rem;
      margin-bottom: 1rem;
      flex-wrap: wrap;
      padding-bottom: 0.5rem;
      border-bottom: 1px dashed var(--pico-muted-border-color);
    }
    .device-nav-label {
      font-size: 0.74rem;
      text-transform: uppercase;
      letter-spacing: 0.05em;
      color: var(--pico-muted-color);
      font-weight: 600;
    }
    .btn-xs {
      padding: 0.2rem 0.6rem !important;
      font-size: 0.78rem !important;
      margin-bottom: 0 !important;
      border-radius: 6px !important;
    }
    .header-bar {
      display: flex;
      justify-content: space-between;
      align-items: center;
      flex-wrap: wrap;
      gap: 1rem;
      margin-bottom: 1.25rem;
      border-bottom: 1px solid var(--pico-muted-border-color);
      padding-bottom: 1rem;
    }
    .header-title h1 {
      margin-bottom: 0.15rem;
      font-size: 1.5rem;
    }
    .header-title small {
      color: var(--pico-muted-color);
    }
    .header-actions {
      display: flex;
      align-items: center;
      gap: 0.5rem;
      flex-wrap: wrap;
    }
    .badge {
      display: inline-flex;
      align-items: center;
      gap: 0.35rem;
      padding: 0.25rem 0.65rem;
      border-radius: 9999px;
      font-size: 0.78rem;
      font-weight: 600;
      letter-spacing: 0.02em;
    }
    .badge-ok {
      background: rgba(16, 185, 129, 0.18);
      color: var(--emerald);
      border: 1px solid var(--emerald);
    }
    .badge-warn {
      background: rgba(245, 158, 11, 0.18);
      color: var(--amber);
      border: 1px solid var(--amber);
    }
    .badge-err {
      background: rgba(244, 63, 94, 0.18);
      color: var(--rose);
      border: 1px solid var(--rose);
    }
    .badge-accent {
      background: rgba(56, 189, 248, 0.18);
      color: var(--accent);
      border: 1px solid var(--accent);
    }
    .badge-muted {
      background: rgba(148, 163, 184, 0.12);
      color: var(--pico-muted-color);
      border: 1px solid rgba(148, 163, 184, 0.25);
    }
    .live-dot {
      width: 8px;
      height: 8px;
      border-radius: 50%;
      background: var(--emerald);
      box-shadow: 0 0 6px var(--emerald);
      display: inline-block;
      transition: transform 0.1s ease;
    }
    .live-dot.paused {
      background: var(--pico-muted-color);
      box-shadow: none;
    }
    /* Packet flash micro-animation */
    @keyframes packet-pulse {
      0% { transform: scale(1); filter: brightness(1); }
      50% { transform: scale(1.6); filter: brightness(2); box-shadow: 0 0 12px #38bdf8, 0 0 6px #fff; }
      100% { transform: scale(1); filter: brightness(1); }
    }
    .packet-flash {
      animation: packet-pulse 0.18s ease-out;
    }
    .hero-card {
      text-align: center;
      padding: 1.5rem;
      margin-bottom: 1.25rem;
      border: 1px solid var(--pico-muted-border-color);
    }
    .hero-val {
      font-size: 3.5rem;
      font-weight: 800;
      color: var(--pico-color);
      letter-spacing: -0.02em;
      line-height: 1.1;
      margin: 0.5rem 0;
    }
    .hero-unit {
      font-size: 1.5rem;
      font-weight: 500;
      color: var(--pico-muted-color);
      margin-left: 0.25rem;
    }
    /* Bidirectional Power Flow Gauge */
    .flow-gauge-wrap {
      width: 100%;
      max-width: 480px;
      margin: 0.75rem auto 0.5rem auto;
    }
    .flow-gauge-labels {
      display: flex;
      justify-content: space-between;
      font-size: 0.72rem;
      font-weight: 600;
      text-transform: uppercase;
      margin-bottom: 0.25rem;
    }
    .flow-gauge-track {
      position: relative;
      height: 8px;
      background: rgba(255, 255, 255, 0.08);
      border-radius: 9999px;
      overflow: hidden;
    }
    .flow-gauge-center {
      position: absolute;
      left: 50%;
      top: 0;
      bottom: 0;
      width: 2px;
      background: var(--pico-muted-border-color);
      transform: translateX(-50%);
      z-index: 2;
    }
    .flow-gauge-fill {
      position: absolute;
      top: 0;
      bottom: 0;
      transition: left 0.15s ease-out, width 0.15s ease-out, background 0.15s ease-out;
      border-radius: 9999px;
    }
    .val-display {
      font-size: 1.8rem;
      font-weight: 700;
      color: var(--pico-color);
      line-height: 1.1;
      margin: 0.35rem 0;
    }
    .status-row {
      display: flex;
      justify-content: space-between;
      align-items: center;
      padding: 0.4rem 0;
      border-bottom: 1px solid var(--pico-muted-border-color);
      font-size: 0.88rem;
    }
    .status-row:last-child {
      border-bottom: none;
    }
    .card-heading {
      font-size: 0.82rem;
      text-transform: uppercase;
      letter-spacing: 0.05em;
      color: var(--pico-muted-color);
      margin-bottom: 0.5rem;
      display: flex;
      justify-content: space-between;
      align-items: center;
    }
    article {
      margin-bottom: 1.25rem;
      padding: 1.25rem;
      border: 1px solid var(--pico-muted-border-color);
    }
    .tab-bar {
      display: flex;
      gap: 0.5rem;
      margin-bottom: 1rem;
      border-bottom: 1px solid var(--pico-muted-border-color);
      padding-bottom: 0.5rem;
      overflow-x: auto;
    }
    .tab-btn {
      padding: 0.35rem 0.85rem !important;
      font-size: 0.82rem !important;
      margin-bottom: 0 !important;
      border-radius: 6px !important;
      white-space: nowrap;
    }
  </style>
</head>
<body class="container" x-data="chintReaderApp()" x-init="init()">
  <!-- Cross-Device Quick Switcher -->
  <nav class="device-nav">
    <span class="device-nav-label">Device Switcher:</span>
    <a href="http://ddsu666.local" target="_blank" role="button" class="outline secondary btn-xs">⚡ Dual Emulator (.169) ↗</a>
    <button class="contrast btn-xs" style="pointer-events: none; border-color: var(--accent); color: var(--accent);">🌐 Chint Master Reader (This Device)</button>
  </nav>

  <!-- Header -->
  <header class="header-bar">
    <div class="header-title">
      <h1>Chint DDSU666 Modbus Master</h1>
      <small>XIAO ESP32-S3 &bull; Dedicated Core 1 Modbus RTU &bull; WebSocket Stream (:81)</small>
    </div>
    <div class="header-actions">
      <button class="outline" :class="liveMode ? 'contrast' : 'secondary'" @click="toggleLiveMode()" style="padding: 0.3rem 0.75rem; font-size: 0.82rem; margin-bottom: 0;">
        <span class="live-dot" :class="{ 'paused': !liveMode, 'packet-flash': packetFlash }"></span>
        <span x-text="liveMode ? 'Live: ON' : 'Live: PAUSED'"></span>
      </button>
      <span class="badge" :class="connBadgeClass" x-text="connBadgeText"></span>
      <span class="badge" :class="meterOnline ? 'badge-ok' : 'badge-err'" x-text="meterOnline ? 'METER ONLINE' : 'METER OFFLINE'"></span>
      <button class="outline secondary" @click="rebootDevice()" style="padding: 0.3rem 0.75rem; font-size: 0.82rem; margin-bottom: 0; color: var(--rose); border-color: var(--rose);">↺ Reboot</button>
    </div>
  </header>

  <!-- Hero Card: Active Power & Bidirectional Gauge -->
  <article class="hero-card">
    <div class="card-heading" style="justify-content: center;">Active Grid Power</div>
    <div class="hero-val">
      <span x-text="wattsDisplay"></span><span class="hero-unit">W</span>
    </div>
    <span class="badge" :class="flowBadgeClass" x-text="flowText"></span>

    <!-- Bidirectional Power Flow Bar -->
    <div class="flow-gauge-wrap">
      <div class="flow-gauge-labels">
        <small style="color: var(--emerald);">◀ Export (Solar)</small>
        <small style="color: var(--pico-muted-color);">0 W</small>
        <small style="color: var(--amber);">Import (Grid) ▶</small>
      </div>
      <div class="flow-gauge-track">
        <div class="flow-gauge-center"></div>
        <div class="flow-gauge-fill" :style="flowGaugeStyle"></div>
      </div>
    </div>
  </article>


  <!-- Secondary Metrics: Electricals & Energy Accumulators -->
  <div class="grid">
    <article>
      <div class="card-heading">🔌 Grid Electricals</div>
      <div class="status-row"><span>Voltage</span><strong x-text="(telemetry.volts ? telemetry.volts.toFixed(1) : '0.0') + ' V'"></strong></div>
      <div class="status-row"><span>Current</span><strong x-text="(telemetry.amps ? telemetry.amps.toFixed(2) : '0.00') + ' A'"></strong></div>
      <div class="status-row"><span>Power Factor</span><strong x-text="telemetry.pf ? telemetry.pf.toFixed(2) : '1.00'"></strong></div>
      <div class="status-row"><span>Frequency</span><strong x-text="(telemetry.freq ? telemetry.freq.toFixed(1) : '50.0') + ' Hz'"></strong></div>
    </article>

    <article>
      <div class="card-heading">📊 Energy Accumulators</div>
      <div class="status-row"><span>Total Import</span><strong x-text="(telemetry.import_kwh ? telemetry.import_kwh.toFixed(2) : '0.00') + ' kWh'"></strong></div>
      <div class="status-row"><span>Total Export</span><strong x-text="(telemetry.export_kwh ? telemetry.export_kwh.toFixed(2) : '0.00') + ' kWh'"></strong></div>
      <div class="status-row"><span>Physical Meter ID</span><strong style="color: var(--accent);" x-text="'Slave ID ' + (modbus.slave_id || 2)"></strong></div>
      <div class="status-row"><span>Baud Rate</span><strong>9600 8N1 (RS-485)</strong></div>
    </article>
  </div>

  <!-- Bottom Utility Maintenance Section: Tabbed Navigation -->
  <nav class="tab-bar">
    <button class="tab-btn" :class="activeTab === 'diag' ? 'contrast' : 'outline secondary'" @click="activeTab = 'diag'">🩺 Modbus Diagnostics</button>
    <button class="tab-btn" :class="activeTab === 'settings' ? 'contrast' : 'outline secondary'" @click="activeTab = 'settings'">⚙️ Slave ID Configuration</button>
    <button class="tab-btn" :class="activeTab === 'ota' ? 'contrast' : 'outline secondary'" @click="activeTab = 'ota'">📦 Firmware OTA &amp; Control</button>
  </nav>

  <!-- Tab 1: Diagnostics & Modbus Bus Health -->
  <article x-show="activeTab === 'diag'">
    <div class="card-heading">
      <span>🩺 Modbus Bus Health &amp; Telemetry Diagnostics</span>
      <button class="outline secondary btn-xs" @click="copyDiagnostics()" x-text="copyBtnText"></button>
    </div>
    <div class="status-row">
      <span>Modbus Polling Speed</span>
      <strong style="color: var(--accent);" x-text="(modbus.rate_hz ? modbus.rate_hz.toFixed(1) : '0.0') + ' Hz'"></strong>
    </div>
    <div class="status-row">
      <span>Meter Response Latency</span>
      <strong x-text="(modbus.latency_ms ? modbus.latency_ms.toFixed(1) : '0.0') + ' ms'"></strong>
    </div>
    <div class="status-row">
      <span>Modbus Slave ID &amp; Baud Rate</span>
      <strong x-text="'ID ' + (modbus.slave_id || 1) + ' • 9600 8N1'"></strong>
    </div>
    <div class="status-row">
      <span>Total RS-485 Queries / CRC Errors</span>
      <span style="font-size: 0.88rem;">
        <strong x-text="(modbus.total_queries || 0).toLocaleString()"></strong> queries &bull;
        <span style="color: var(--rose);" x-text="(modbus.crc_errors || 0) + ' CRC err'"></span>
      </span>
    </div>
    <div class="status-row">
      <span>Connected WebSocket Clients</span>
      <strong style="color: var(--emerald);" x-text="(websocket.clients || 0) + ' active'"></strong>
    </div>
    <div class="status-row">
      <span>WebSocket Stream Endpoint</span>
      <span style="font-family: monospace; font-size: 0.85rem;" x-text="wsUrl"></span>
    </div>
    <div class="status-row">
      <span>System Uptime &amp; Wi-Fi RSSI</span>
      <span x-text="Math.floor((uptime_sec || 0) / 60) + ' min • ' + (wifi.rssi || 0) + ' dBm'"></span>
    </div>
  </article>

  <!-- Tab 2: Modbus Slave ID Configuration -->
  <article x-show="activeTab === 'settings'">
    <div class="card-heading">⚙️ Modbus Slave ID Configuration</div>
    <p style="font-size: 0.8rem; color: var(--pico-muted-color); margin-bottom: 1rem;">
      Configure the physical Chint DDSU666 Modbus RTU address. Saved to ESP32 NVS flash and applied immediately.
    </p>
    <div style="max-width: 320px;">
      <label>
        Chint DDSU666 Slave ID (Default: 2)
        <input type="number" x-model.number="inputSlave" min="1" max="247" style="margin-top: 0.4rem;">
      </label>
      <div style="display: flex; gap: 0.75rem; margin-top: 1rem;">
        <button @click="saveSettings()" style="width: auto; margin-bottom: 0;">Save &amp; Apply</button>
        <button class="outline secondary" @click="resetSlaveInput()" style="width: auto; margin-bottom: 0;">Reset</button>
      </div>
    </div>
  </article>

  <!-- Tab 3: Firmware OTA & Device Control -->
  <article x-show="activeTab === 'ota'">
    <div class="card-heading">📦 Wireless Web OTA Update &amp; Device Control</div>
    <p style="font-size: 0.8rem; color: var(--pico-muted-color); margin-bottom: 1rem;">
      Upload compiled .bin firmware directly to flash memory over Wi-Fi.
    </p>
    <div style="display: flex; gap: 0.75rem; align-items: center; flex-wrap: wrap;">
      <a href="/update" role="button" class="contrast" style="width: auto; margin-bottom: 0;">Open Web OTA Upload Page ↗</a>
      <button class="outline secondary" @click="rebootDevice()" style="width: auto; margin-bottom: 0; color: var(--rose); border-color: var(--rose);">Reboot Device</button>
    </div>
  </article>

  <footer style="text-align: center; font-size: 0.8rem; color: var(--pico-muted-color); margin-top: 2rem;">
    Chint DDSU666 Modbus RTU Reader &bull; WebSocket Stream on Port 81 &bull; Health API: <a href="/health">/health</a>
  </footer>

  <script>
    function chintReaderApp() {
      return {
        liveMode: localStorage.getItem('chint_live_mode') !== 'false',
        activeTab: 'diag',
        connState: 'live',
        meterOnline: true,
        telemetry: { watts: 0, volts: 0, amps: 0, pf: 1, freq: 50, import_kwh: 0, export_kwh: 0 },
        modbus: { rate_hz: 0, latency_ms: 0, slave_id: 2, total_queries: 0, crc_errors: 0, timeouts: 0 },
        websocket: { clients: 0, packets_sent: 0 },
        uptime_sec: 0,
        wifi: { rssi: 0 },
        inputSlave: 2,
        copyBtnText: "📋 Copy Diagnostics",
        packetFlash: false,

        ws: null,
        wsTimer: null,
        pollTimer: null,
        flashTimer: null,

        get wsUrl() {
          const host = window.location.hostname || "10.0.0.197";
          return `ws://${host}:81/`;
        },

        init() {

          if (this.liveMode) {
            this.connectWS();
            this.startPolling();
          } else {
            this.connState = 'paused';
          }

          document.addEventListener("visibilitychange", () => {
            if (!this.liveMode) return;
            if (document.hidden) {
              if (this.ws && this.ws.readyState === WebSocket.OPEN) this.ws.close();
              this.stopPolling();
            } else {
              this.connectWS();
              this.startPolling();
            }
          });
        },

        resizeCanvas() {
          const canvas = document.getElementById("power-chart");
          if (canvas) {
            const rect = canvas.getBoundingClientRect();
            canvas.width = rect.width * (window.devicePixelRatio || 1);
            canvas.height = 110 * (window.devicePixelRatio || 1);
          }
        },

        get connBadgeClass() {
          if (!this.liveMode || this.connState === 'paused') return 'badge-muted';
          if (this.connState === 'live') return 'badge-accent';
          if (this.connState === 'poll') return 'badge-warn';
          return 'badge-err';
        },
        get connBadgeText() {
          if (!this.liveMode || this.connState === 'paused') return '⏸️ UPDATES PAUSED';
          if (this.connState === 'live') return '⚡ LIVE WS (15Hz)';
          if (this.connState === 'poll') return '⏱ HTTP POLL (2s)';
          return '🔌 RECONNECTING...';
        },
        get wattsDisplay() {
          const w = this.telemetry.watts || 0;
          return Math.abs(w).toFixed(1);
        },
        get flowBadgeClass() {
          const w = this.telemetry.watts || 0;
          if (w > 10.0) return 'badge-warn';
          if (w < -10.0) return 'badge-ok';
          return 'badge-accent';
        },
        get flowText() {
          const w = this.telemetry.watts || 0;
          if (w > 10.0) return '↓ Grid Import';
          if (w < -10.0) return '↑ Solar Export';
          return '⚡ Balanced';
        },
        get flowGaugeStyle() {
          const w = this.telemetry.watts || 0;
          const maxW = 4000;
          const clamped = Math.max(-maxW, Math.min(maxW, w));
          const pct = (Math.abs(clamped) / maxW) * 50;
          if (w >= 0) {
            return {
              left: '50%',
              width: pct + '%',
              background: 'var(--amber)'
            };
          } else {
            return {
              left: (50 - pct) + '%',
              width: pct + '%',
              background: 'var(--emerald)'
            };
          }
        },

        toggleLiveMode() {
          this.liveMode = !this.liveMode;
          localStorage.setItem('chint_live_mode', this.liveMode ? 'true' : 'false');
          if (this.liveMode) {
            this.connState = 'live';
            this.connectWS();
            this.startPolling();
          } else {
            if (this.ws && this.ws.readyState === WebSocket.OPEN) this.ws.close();
            if (this.wsTimer) {
              clearTimeout(this.wsTimer);
              this.wsTimer = null;
            }
            this.stopPolling();
            this.connState = 'paused';
          }
        },

        triggerPacketFlash() {
          this.packetFlash = true;
          if (this.flashTimer) clearTimeout(this.flashTimer);
          this.flashTimer = setTimeout(() => { this.packetFlash = false; }, 180);
        },

        copyDiagnostics() {
          const d = new Date().toISOString().replace('T', ' ').substring(0, 19);
          const text = [
            `=== Chint DDSU666 Modbus Master Diagnostics (${d}) ===`,
            `Meter: ${this.meterOnline ? 'ONLINE' : 'OFFLINE'} | Slave ID: ${this.modbus.slave_id}`,
            `Grid Power: ${this.telemetry.watts.toFixed(1)} W | ${this.telemetry.volts.toFixed(1)} V | ${this.telemetry.amps.toFixed(2)} A | PF: ${this.telemetry.pf.toFixed(2)}`,
            `Accumulated: Import=${this.telemetry.import_kwh.toFixed(2)} kWh | Export=${this.telemetry.export_kwh.toFixed(2)} kWh`,
            `Modbus RTU: Rate=${this.modbus.rate_hz.toFixed(1)} Hz | Latency=${this.modbus.latency_ms.toFixed(1)} ms | Queries=${this.modbus.total_queries} | CRC Errors=${this.modbus.crc_errors}`,
            `WebSocket: Clients=${this.websocket.clients} | Stream=${this.wsUrl}`,
            `Wi-Fi: RSSI=${this.wifi.rssi} dBm | Uptime=${Math.floor(this.uptime_sec / 60)} min`
          ].join('\n');

          if (navigator.clipboard && navigator.clipboard.writeText) {
            navigator.clipboard.writeText(text).then(() => {
              this.copyBtnText = "Copied! ✓";
              setTimeout(() => { this.copyBtnText = "📋 Copy Diagnostics"; }, 2000);
            }).catch(() => {
              this.fallbackCopy(text);
            });
          } else {
            this.fallbackCopy(text);
          }
        },

        fallbackCopy(text) {
          const ta = document.createElement("textarea");
          ta.value = text;
          document.body.appendChild(ta);
          ta.select();
          document.execCommand("copy");
          document.body.removeChild(ta);
          this.copyBtnText = "Copied! ✓";
          setTimeout(() => { this.copyBtnText = "📋 Copy Diagnostics"; }, 2000);
        },


        connectWS() {
          if (!this.liveMode) return;
          try {
            this.ws = new WebSocket(this.wsUrl);
          } catch(e) {
            this.connState = 'poll';
            return;
          }

          this.ws.onopen = () => {
            this.connState = 'live';
            if (this.wsTimer) {
              clearTimeout(this.wsTimer);
              this.wsTimer = null;
            }
          };

          this.ws.onmessage = (e) => {
            if (!this.liveMode) return;
            try {
              const data = JSON.parse(e.data);
              if (data.type === 'telemetry' || data.type === 'welcome') {
                this.applyTelemetry(data);
              }
            } catch(err) {}
          };

          this.ws.onclose = () => {
            if (!this.liveMode) return;
            this.connState = 'poll';
            if (!this.wsTimer) {
              this.wsTimer = setTimeout(() => {
                this.wsTimer = null;
                if (this.liveMode) this.connectWS();
              }, 2500);
            }
          };

          this.ws.onerror = () => {
            this.connState = 'poll';
          };
        },

        applyTelemetry(d) {
          if (!this.liveMode || !d) return;
          this.triggerPacketFlash();

          if (d.watts !== undefined) {
            this.telemetry.watts = d.watts;
          }
          if (d.volts !== undefined) this.telemetry.volts = d.volts;
          if (d.amps !== undefined) this.telemetry.amps = d.amps;
          if (d.pf !== undefined) this.telemetry.pf = d.pf;
          if (d.freq !== undefined) this.telemetry.freq = d.freq;
          if (d.import_kwh !== undefined) this.telemetry.import_kwh = d.import_kwh;
          if (d.export_kwh !== undefined) this.telemetry.export_kwh = d.export_kwh;

          if (d.poll_hz !== undefined) this.modbus.rate_hz = d.poll_hz;
          if (d.latency_ms !== undefined) this.modbus.latency_ms = d.latency_ms;
          if (d.slave_id !== undefined) {
            this.modbus.slave_id = d.slave_id;
            this.inputSlave = d.slave_id;
          }
          if (d.meter_online !== undefined) this.meterOnline = d.meter_online;
        },

        startPolling() {
          if (!this.pollTimer && this.liveMode) {
            this.pollHealth();
            this.pollTimer = setInterval(() => this.pollHealth(), 2000);
          }
        },

        stopPolling() {
          if (this.pollTimer) {
            clearInterval(this.pollTimer);
            this.pollTimer = null;
          }
        },

        pollHealth() {
          if (!this.liveMode) return;
          fetch('/health')
            .then(r => r.json())
            .then(data => {
              if (!this.liveMode || !data) return;
              if (data.websocket) this.websocket = Object.assign({}, this.websocket, data.websocket);
              if (data.uptime_sec !== undefined) this.uptime_sec = data.uptime_sec;
              if (data.wifi) this.wifi = Object.assign({}, this.wifi, data.wifi);
              if (data.modbus) {
                this.modbus = Object.assign({}, this.modbus, data.modbus);
                if (data.modbus.meter_online !== undefined) this.meterOnline = data.modbus.meter_online;
                if (data.modbus.slave_id !== undefined && !this.inputSlave) this.inputSlave = data.modbus.slave_id;
              }
              if (!this.ws || this.ws.readyState !== WebSocket.OPEN) {
                if (data.telemetry) this.applyTelemetry(data.telemetry);
              }
            }).catch(e => {
              if (this.connState !== 'live') this.connState = 'reconnecting';
            });
        },

        resetSlaveInput() {
          this.inputSlave = this.modbus.slave_id || 2;
        },

        saveSettings() {
          const slave = parseInt(this.inputSlave, 10);
          if (this.ws && this.ws.readyState === WebSocket.OPEN) {
            this.ws.send(JSON.stringify({ action: 'set_slave', slave: slave }));
            setTimeout(() => this.pollHealth(), 500);
          } else {
            fetch('/api/settings?slave=' + slave, { method: 'POST' })
              .then(r => r.json())
              .then(d => {
                this.pollHealth();
              }).catch(e => {});
          }
        },

        rebootDevice() {
          if (!confirm('Reboot the Chint Reader device?')) return;
          if (this.ws && this.ws.readyState === WebSocket.OPEN) {
            this.ws.send(JSON.stringify({ action: 'reboot' }));
          } else {
            fetch('/reboot', { method: 'POST' });
          }
          alert('Device rebooting... reloading page in 6s');
          setTimeout(() => location.reload(), 6000);
        }
      };
    }
  </script>
</body>
</html>
)rawliteral";
