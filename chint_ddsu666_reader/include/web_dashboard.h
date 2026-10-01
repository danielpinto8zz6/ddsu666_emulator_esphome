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
      padding-top: 1.5rem;
      padding-bottom: 3rem;
    }
    .header-bar {
      display: flex;
      justify-content: space-between;
      align-items: center;
      flex-wrap: wrap;
      gap: 1rem;
      margin-bottom: 1.5rem;
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
    }
    .live-dot.paused {
      background: var(--pico-muted-color);
      box-shadow: none;
    }
    .hero-card {
      text-align: center;
      padding: 2rem 1.5rem;
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
  </style>
</head>
<body class="container" x-data="chintReaderApp()" x-init="init()">
  <!-- Header -->
  <header class="header-bar">
    <div class="header-title">
      <h1>Chint DDSU666 Modbus Master</h1>
      <small>XIAO ESP32-S3 &bull; Dedicated Core 1 Modbus RTU &bull; WebSocket Stream (:81)</small>
    </div>
    <div class="header-actions">
      <button class="outline" :class="liveMode ? 'contrast' : 'secondary'" @click="toggleLiveMode()" style="padding: 0.3rem 0.75rem; font-size: 0.82rem; margin-bottom: 0;">
        <span class="live-dot" :class="{ 'paused': !liveMode }"></span>
        <span x-text="liveMode ? 'Live: ON' : 'Live: PAUSED'"></span>
      </button>
      <span class="badge" :class="connBadgeClass" x-text="connBadgeText"></span>
      <span class="badge" :class="meterOnline ? 'badge-ok' : 'badge-err'" x-text="meterOnline ? 'METER ONLINE' : 'METER OFFLINE'"></span>
      <button class="outline contrast" @click="openModal()" style="padding: 0.3rem 0.75rem; font-size: 0.82rem; margin-bottom: 0;">⚙️ Slave ID</button>
      <a href="/update" role="button" class="contrast" style="padding: 0.3rem 0.75rem; font-size: 0.82rem; margin-bottom: 0;">📦 OTA</a>
      <button class="outline secondary" @click="rebootDevice()" style="padding: 0.3rem 0.75rem; font-size: 0.82rem; margin-bottom: 0; color: var(--rose); border-color: var(--rose);">↺ Reboot</button>
    </div>
  </header>

  <!-- Hero Card: Active Power -->
  <article class="hero-card">
    <div class="card-heading" style="justify-content: center;">Active Grid Power</div>
    <div class="hero-val">
      <span x-text="wattsDisplay"></span><span class="hero-unit">W</span>
    </div>
    <span class="badge" :class="flowBadgeClass" x-text="flowText"></span>
  </article>

  <!-- Electricals Grid -->
  <div class="grid">
    <article>
      <div class="card-heading">Voltage</div>
      <div class="val-display" x-text="(telemetry.volts ? telemetry.volts.toFixed(1) : '0.0') + ' V'"></div>
    </article>
    <article>
      <div class="card-heading">Current</div>
      <div class="val-display" x-text="(telemetry.amps ? telemetry.amps.toFixed(2) : '0.00') + ' A'"></div>
    </article>
    <article>
      <div class="card-heading">Power Factor</div>
      <div class="val-display" x-text="telemetry.pf ? telemetry.pf.toFixed(2) : '1.00'"></div>
    </article>
    <article>
      <div class="card-heading">Frequency</div>
      <div class="val-display" x-text="(telemetry.freq ? telemetry.freq.toFixed(1) : '50.0') + ' Hz'"></div>
    </article>
  </div>

  <!-- Energy Accumulators Grid -->
  <div class="grid">
    <article>
      <div class="card-heading">Total Import Energy</div>
      <div class="val-display" x-text="(telemetry.import_kwh ? telemetry.import_kwh.toFixed(2) : '0.00') + ' kWh'"></div>
    </article>
    <article>
      <div class="card-heading">Total Export Energy</div>
      <div class="val-display" x-text="(telemetry.export_kwh ? telemetry.export_kwh.toFixed(2) : '0.00') + ' kWh'"></div>
    </article>
  </div>

  <!-- Diagnostics & Modbus Bus Health -->
  <article>
    <div class="card-heading">🩺 Modbus Bus Health &amp; Telemetry Diagnostics</div>
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

  <!-- Modbus Slave ID Modal Dialog -->
  <dialog :open="modalOpen">
    <article style="max-width: 420px; width: 100%;">
      <header style="display: flex; justify-content: space-between; align-items: center; margin-bottom: 1rem;">
        <strong>⚙️ Modbus Slave ID Configuration</strong>
        <button aria-label="Close" rel="prev" @click="closeModal()" style="border: none; background: none; font-size: 1.2rem; cursor: pointer; padding: 0;">✕</button>
      </header>
      <label>
        Chint DDSU666 Slave ID (Default: 2)
        <input type="number" x-model.number="inputSlave" min="1" max="247" style="margin-top: 0.5rem;">
      </label>
      <small style="color: var(--pico-muted-color); display: block; margin-top: 0.25rem;">
        Saved to ESP32 NVS flash and applied immediately.
      </small>
      <footer style="display: flex; justify-content: flex-end; gap: 0.75rem; margin-top: 1.5rem;">
        <button class="secondary outline" @click="closeModal()" style="width: auto; margin-bottom: 0;">Cancel</button>
        <button @click="saveSettings()" style="width: auto; margin-bottom: 0;">Save &amp; Apply</button>
      </footer>
    </article>
  </dialog>

  <footer style="text-align: center; font-size: 0.8rem; color: var(--pico-muted-color); margin-top: 2rem;">
    Chint DDSU666 Modbus RTU Reader &bull; WebSocket Stream on Port 81 &bull; Health API: <a href="/health">/health</a>
  </footer>

  <script>
    function chintReaderApp() {
      return {
        liveMode: localStorage.getItem('chint_live_mode') !== 'false',
        connState: 'live',
        meterOnline: true,
        telemetry: { watts: 0, volts: 0, amps: 0, pf: 1, freq: 50, import_kwh: 0, export_kwh: 0 },
        modbus: { rate_hz: 0, latency_ms: 0, slave_id: 2, total_queries: 0, crc_errors: 0, timeouts: 0 },
        websocket: { clients: 0, packets_sent: 0 },
        uptime_sec: 0,
        wifi: { rssi: 0 },
        modalOpen: false,
        inputSlave: 2,

        ws: null,
        wsTimer: null,
        pollTimer: null,

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
          if (d.watts !== undefined) this.telemetry.watts = d.watts;
          if (d.volts !== undefined) this.telemetry.volts = d.volts;
          if (d.amps !== undefined) this.telemetry.amps = d.amps;
          if (d.pf !== undefined) this.telemetry.pf = d.pf;
          if (d.freq !== undefined) this.telemetry.freq = d.freq;
          if (d.import_kwh !== undefined) this.telemetry.import_kwh = d.import_kwh;
          if (d.export_kwh !== undefined) this.telemetry.export_kwh = d.export_kwh;

          if (d.poll_hz !== undefined) this.modbus.rate_hz = d.poll_hz;
          if (d.latency_ms !== undefined) this.modbus.latency_ms = d.latency_ms;
          if (d.slave_id !== undefined) this.modbus.slave_id = d.slave_id;
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
              }
              if (!this.ws || this.ws.readyState !== WebSocket.OPEN) {
                if (data.telemetry) this.applyTelemetry(data.telemetry);
              }
            }).catch(e => {
              if (this.connState !== 'live') this.connState = 'reconnecting';
            });
        },

        openModal() {
          this.inputSlave = this.modbus.slave_id || 2;
          this.modalOpen = true;
          fetch('/health').then(r => r.json()).then(d => {
            if (d && d.modbus && d.modbus.slave_id) {
              this.inputSlave = d.modbus.slave_id;
            }
          }).catch(e => {});
        },

        closeModal() {
          this.modalOpen = false;
        },

        saveSettings() {
          const slave = parseInt(this.inputSlave, 10);
          if (this.ws && this.ws.readyState === WebSocket.OPEN) {
            this.ws.send(JSON.stringify({ action: 'set_slave', slave: slave }));
            this.closeModal();
            setTimeout(() => this.pollHealth(), 500);
          } else {
            fetch('/api/settings?slave=' + slave, { method: 'POST' })
              .then(r => r.json())
              .then(d => {
                this.closeModal();
                this.pollHealth();
              }).catch(e => this.closeModal());
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
