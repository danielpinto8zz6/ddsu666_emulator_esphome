# Dual CHINT DDSU666 Modbus RTU Emulator for Hoymiles Hybrid Inverters

This repository contains firmware to emulate **two independent CHINT DDSU666 Modbus RTU meters simultaneously** on a single RS-485 bus using an ESP32. It bridges wireless smart meters (**Shelly Pro EM**, **OpenDTU-onBattery**) directly to a **Hoymiles Hybrid Inverter** (e.g., HYS/HYB series) without requiring hardwired physical CT clamps.

---

## 📑 Table of Contents

- [Overview & Architecture](#-overview--architecture)
- [Supported Hardware & Wiring](#-supported-hardware--wiring)
- [Repository Structure](#-repository-structure)
- [Solution 1: ESPHome](#-solution-1-esphome)
  - [Prerequisites & Configuration](#1-prerequisites--configuration)
  - [Compiling & Flashing](#2-compiling--flashing)
  - [Shelly Pro EM High-Frequency Script](#3-shelly-pro-em-high-frequency-script-zero-export-stability)
  - [OpenDTU MQTT Integration](#4-opendtu-mqtt-integration)
  - [Failsafe & Watchdog Protections](#5-failsafe--watchdog-protections)
- [Solution 2: Native C++ / FreeRTOS (Advanced High-Speed)](#-solution-2-native-c--freertos-advanced-high-speed)
  - [Key Advantages](#key-advantages)
  - [Configuration (`secrets.h`)](#configuration-secretsh)
  - [Compiling & Flashing (PlatformIO)](#compiling--flashing-platformio)
  - [ESP32-C3 SuperMini Wireless Transmitter Bridge](#esp32-c3-supermini-wireless-transmitter-bridge)
  - [Embedded WebUI & Diagnostic Health API](#embedded-webui--diagnostic-health-api)
- [Feature Comparison: ESPHome vs Native C++](#-feature-comparison-esphome-vs-native-c)
- [Troubleshooting & Diagnostics](#-troubleshooting--diagnostics)

---

## 🌐 Overview & Architecture

Hoymiles HYS/HYB hybrid inverters poll two CHINT DDSU666 meters over RS-485 Modbus RTU at 9600 baud (8N1):

```
┌──────────────────────────┐                   ┌───────────────────────────────┐
│  Shelly Pro EM 50A       │──[MQTT / WS]─────>│                               │
│  (Main House Grid Meter) │                   │  ESP32 Meter Emulator         │──[RS-485 Modbus RTU]──> Hoymiles Hybrid Inverter
├──────────────────────────┤                   │  • Slave ID 2: Grid Meter     │   (COM Port Pins 1 & 2)  (HYS / HYB Series)
│  OpenDTU-onBattery       │──[MQTT]──────────>│  • Slave ID 1: PV Solar Meter │
│  (PV Microinverters)     │                   │                               │
└──────────────────────────┘                   └───────────────────────────────┘
```

- **Slave ID 2 (Grid Meter)**: Emulates the main grid connection meter. Provides active power, voltage, current, power factor, frequency, and cumulative import/export kWh for the inverter's **Zero-Export (Autoconsumo)** PID control loop.
- **Slave ID 1 (PV Meter)**: Emulates a PV string meter using generation data from OpenDTU. Allows the inverter to display solar generation and battery charging on the Hoymiles S-Miles app without a separate physical CT.

---

## 🔌 Supported Hardware & Wiring

### 1. LILYGO T-CAN485 (ESP32)
* Built-in RS-485 transceiver and 5V boost converter.
* **RS-485 TX**: GPIO 22
* **RS-485 RX**: GPIO 21
* **Direction Control (DE/!RE)**: GPIO 17
* **5V Boost Power Enable**: GPIO 16
* **RS-485 Chip Enable**: GPIO 19

### 2. Waveshare ESP32-S3 RS485 / CAN
* Dual-core ESP32-S3 with onboard isolated RS-485 transceiver.
* **RS-485 TX**: GPIO 17
* **RS-485 RX**: GPIO 18
* **Direction Control (DE/!RE)**: GPIO 21

### RS-485 to Inverter Wiring

| ESP32 Terminal | Hoymiles Inverter COM Pin | Description |
| :--- | :--- | :--- |
| **A (A+)** | **Pin 1 (METER_A)** | RS-485 Differential Data Positive |
| **B (B-)** | **Pin 2 (METER_B)** | RS-485 Differential Data Negative |
| **GND** | **Pin 3 (GND)** *(Optional)* | Common Signal Ground |

> [!WARNING]
> If the inverter reports **"Meter Disconnected"** or **F05/F06 alarms**, swap the **A** and **B** wires. Modbus polarity labeling can vary across vendors.

---

## 📁 Repository Structure

```
ddsu666_emulator_esphome/
├── ddsu666.yaml                    # ESPHome Configuration: LILYGO T-CAN485
├── ddsu666_waveshare.yaml          # ESPHome Configuration: Waveshare ESP32-S3
├── secrets.yaml.example            # ESPHome Secrets template
├── shelly_pro_em_script.js         # Shelly Pro EM 500ms high-frequency mJS script
│
├── platformio.ini                  # PlatformIO configuration (Native C++ Firmware)
├── include/                        # Native C++ Headers
│   ├── config.h                    # Board pinouts, Modbus, and Shelly configuration
│   ├── meter_data.h                # In-memory DDSU666 register state & conversions
│   ├── esp_now_receiver.h          # ESP-NOW sniffer & receiver definitions
│   ├── web_ui_gz.h                 # Compressed WebUI dashboard binary
│   └── secrets.h.example           # Native C++ secrets template
├── src/                            # Native C++ Source Code
│   ├── main.cpp                    # Dual-core tasks & FreeRTOS initialization
│   ├── modbus_rtu.cpp / .h         # High-speed Modbus RTU slave engine (Core 1)
│   ├── meter_data.cpp              # Telemetry sync & Modbus FP32 conversions
│   ├── mqtt_manager.cpp / .h       # Shelly WebSocket client & MQTT (Core 0)
│   ├── web_server.cpp / .h         # REST API & WebUI dashboard (Core 0)
│   └── esp_now_receiver.cpp        # ESP-NOW sniffer & dynamic peer tracker
├── web_ui_source.html              # Uncompressed HTML/CSS/JS source for WebUI
│
├── shelly_espnow_transmitter/      # ESP32-C3 SuperMini Wireless Transmitter Bridge
│   ├── platformio.ini              # Transmitter build environment
│   ├── include/                    # Transmitter headers
│   │   ├── transmitter_config.h    # Wi-Fi, Shelly IP, and broadcast/unicast settings
│   │   ├── packet_format.h         # Binary packet structure (Magic 0xD5)
│   │   └── lolin_web_ui.h          # Compressed web dashboard
│   ├── src/main.cpp                # Shelly WebSocket client & ESP-NOW broadcaster
│   └── transmitter_web_ui_source.html # Transmitter web dashboard source
│
└── examples/
    └── espnow_transmitter_example.ino # Minimal standalone Arduino transmitter
```

---

## 🚀 Solution 1: ESPHome

The **ESPHome** solution integrates natively with Home Assistant, provides painless over-the-air (OTA) updates from the dashboard, and includes comprehensive failsafes.

### 1. Prerequisites & Configuration

1. Install ESPHome in Python or use the Home Assistant ESPHome add-on:
   ```bash
   pip install esphome
   ```
2. Copy [`secrets.yaml.example`](file:///home/danielpinto/Documentos/ddsu666_emulator_esphome/secrets.yaml.example) to `secrets.yaml`:
   ```bash
   cp secrets.yaml.example secrets.yaml
   ```
3. Edit `secrets.yaml` with your Wi-Fi credentials:
   ```yaml
   wifi_ssid: "YourWiFiSSID"
   wifi_password: "YourWiFiPassword"
   ota_password: "admin"
   ```
4. Verify your MQTT broker IP in [ddsu666.yaml](file:///home/danielpinto/Documentos/ddsu666_emulator_esphome/ddsu666.yaml#L122-L128) (default is `10.0.0.1:1883`).

### 2. Compiling & Flashing

Connect your ESP32 board via USB and run:

```bash
# For LILYGO T-CAN485 (ESP32):
esphome run ddsu666.yaml

# For Waveshare ESP32-S3 RS485:
esphome run ddsu666_waveshare.yaml
```

Once flashed via USB, all subsequent updates can be pushed wirelessly over Wi-Fi OTA.

---

### 3. Shelly Pro EM High-Frequency Script (Zero-Export Stability)

Hybrid inverters rely on rapid feedback (every ~500ms) to calculate battery charge/discharge rates. If the inverter receives delayed telemetry (e.g., standard 30s or 60s updates), the PID control loop will violently oscillate ("hunting"), causing rapid export spikes and grid imports.

To achieve smooth, rock-solid zero-export control without overwhelming Wi-Fi bandwidth, run [shelly_pro_em_script.js](file:///home/danielpinto/Documentos/ddsu666_emulator_esphome/shelly_pro_em_script.js) on your Shelly Pro EM 50A:

1. Open your Shelly Pro EM web browser interface (`http://<SHELLY_IP>/`).
2. Navigate to **Scripts** &rarr; **Add Script**.
3. Paste the contents of [`shelly_pro_em_script.js`](file:///home/danielpinto/Documentos/ddsu666_emulator_esphome/shelly_pro_em_script.js):
   ```javascript
   let CONFIG = {
     update_interval_ms: 500, // Publish every 500ms
     mqtt_topic: "shelly-em/realtime"
   };

   if (MQTT.isConnected()) {
     Timer.set(CONFIG.update_interval_ms, true, function() {
       let em = Shelly.getComponentStatus("em1", 0);
       if (em) {
         let payload = JSON.stringify({
           "v": em.voltage,
           "c": em.current,
           "p": em.act_power,
           "s": em.aprt_power,
           "pf": em.pf,
           "f": em.freq
         });
         MQTT.publish(CONFIG.mqtt_topic, payload, 0, false);
       }
     });
   }
   ```
4. Click **Save**, then click **Start**, and ensure **Run on boot** is toggled **ON**.

#### Energy Counters (`shelly-em/status/em1data:0`)
In addition to the 500ms fast telemetry, the ESP32 subscribes to the Shelly's native `em1data:0` topic every 60 seconds to retrieve lifetime cumulative energy import (`total_act_energy`) and export (`total_act_ret_energy`).

> [!TIP]
> In your Shelly MQTT Settings, ensure **"Generic status update over MQTT"** is **CHECKED**. You can safely uncheck "RPC status notifications over MQTT" to minimize network traffic.

---

### 4. OpenDTU MQTT Integration

For the PV Meter (Slave ID 1), the ESPHome firmware subscribes to two MQTT topics published by OpenDTU or OpenDTU-onBattery:

- **`solar/ac/power`**: Current solar active power in Watts (automatically converted to kW).
- **`solar/ac/yieldtotal`**: Total cumulative solar energy in kWh.

These topics can be customized directly in [ddsu666.yaml](file:///home/danielpinto/Documentos/ddsu666_emulator_esphome/ddsu666.yaml#L147-L175) if your OpenDTU topic prefix differs (e.g., `solar/11223344/0/power`).

---

### 5. Failsafe & Watchdog Protections

If communication with the smart meter drops, standard emulators might leave the last known power value frozen in memory. If you were exporting 3 kW when Wi-Fi disconnected, the inverter could continuously discharge the battery into the grid.

To guarantee electrical safety, this firmware includes multi-tier hardware failsafes:

1. **10-Second Watchdog Timer**: If no MQTT telemetry is received for 10 seconds, the ESP32 automatically cuts power to the RS-485 transceiver (GPIO 16 and GPIO 19).
2. **Wi-Fi Disconnect Trigger**: If Wi-Fi drops, the RS-485 bus is immediately powered down.
3. **Inverter Response**: The Hoymiles inverter immediately detects a **"Meter Communication Fault"** and gracefully drops into its safe idle/standby state, preventing uncontrolled battery drain or grid export.

---

## ⚡ Solution 2: Native C++ / FreeRTOS (Advanced High-Speed)

The **Native C++** implementation is an ultra-high-performance alternative engineered specifically for zero-latency PID response times.

### Key Advantages
* **Dual-Core FreeRTOS Isolation**:
  * **Core 1**: Dedicated 100% exclusively to RS-485 Modbus RTU polling. Sub-millisecond response latency with zero jitter.
  * **Core 0**: Handles Wi-Fi, direct Shelly WebSockets, MQTT, ESP-NOW, WebUI, and OTA updates.
* **Direct Shelly WebSocket Client (200ms)**: Connects directly to `ws://<SHELLY_IP>/rpc` without requiring an MQTT broker or Shelly script.
* **Rolling Modbus Health & Query Rate Monitor**: WebUI displays live query frequency (~14.5 Hz) and inverter link status.
* **ESP-NOW Wireless Bridge Support**: Allows an optional satellite microcontroller (ESP32-C3 SuperMini) to beam telemetry directly over the air.

---

### Configuration (`secrets.h`)

1. Copy [`include/secrets.h.example`](file:///home/danielpinto/Documentos/ddsu666_emulator_esphome/include/secrets.h.example) to `include/secrets.h`:
   ```bash
   cp include/secrets.h.example include/secrets.h
   ```
2. Edit `include/secrets.h` with your network configuration:
   ```c
   #pragma once

   // Wi-Fi Credentials (shared by Lilygo and ESP32-C3 transmitter)
   #define WIFI_SSID       "YourWiFiSSID"
   #define WIFI_PASSWORD   "YourWiFiPassword"

   // Shelly Pro EM IP Address
   #define SHELLY_IP       "10.0.0.187"

   // MQTT Broker Configuration (for OpenDTU PV meter)
   #define MQTT_BROKER_IP  "10.0.0.1"
   #define MQTT_PORT       1883
   #define MQTT_USER       ""
   #define MQTT_PASSWORD   ""
   #define MQTT_CLIENT_ID  "ddsu666-emulator"

   // OTA Password
   #define OTA_PASSWORD    "admin"
   ```

---

### Compiling & Flashing (PlatformIO)

Activate your Python virtual environment or use global `pio`:

```bash
# 1. Build firmware
pio run -e lilygo-t-can485

# 2. Flash via USB
pio run -e lilygo-t-can485 -t upload

# 3. View real-time logs
pio device monitor -b 115200
```

#### Over-The-Air (OTA) Updates:
Future updates can be uploaded directly over HTTP:
```bash
curl -F "file=@.pio/build/lilygo-t-can485/firmware.bin" -H "Expect:" http://<ESP32_IP>/update
```

---

### ESP32-C3 SuperMini Wireless Transmitter Bridge

If your Shelly Pro EM is located in a main electrical panel far from the inverter, running long Ethernet or RS-485 cables can be challenging.

The [`shelly_espnow_transmitter/`](file:///home/danielpinto/Documentos/ddsu666_emulator_esphome/shelly_espnow_transmitter/) subproject allows a miniature **ESP32-C3 SuperMini** (or Lolin C3 Mini) to act as a wireless bridge:

```
[Main Electrical Panel]                                  [Inverter Location]
 Shelly Pro EM 50A ──(WiFi/WS)──> ESP32-C3 SuperMini ──(ESP-NOW 2.4GHz)──> LILYGO T-CAN485 ──(RS485)──> Hoymiles Inverter
```

- **Zero Router Congestion**: Uses low-latency 2.4 GHz ESP-NOW frames (sub-5ms transit time).
- **Auto-Learning Receiver**: The Lilygo automatically discovers and tracks the transmitter's MAC address on the first packet.
- **Broadcast & Unicast Modes**: Defaults to robust Broadcast mode (`FF:FF:FF:FF:FF:FF`) for 100% transmission throughput without pairing, with optional Unicast targeting the Lilygo MAC.
- **Onboard Status LED (GPIO 8)**: Blinks while connecting to Wi-Fi, stays OFF when idle, and emits a crisp 20ms pulse on every transmitted packet.

#### Flashing the ESP32-C3 SuperMini:
```bash
pio run -e esp32-c3-supermini -t upload
```

---

### Embedded WebUI & Diagnostic Health API

The native C++ firmware hosts a dark-mode diagnostic web dashboard at `http://<ESP32_IP>/`:

- **Hoymiles Inverter Link**: Shows live RS-485 polling rate (e.g., `ACTIVE (14.6 Hz, last query 0.1s ago)`).
- **Grid Power Card**: Real-time wattage, direction (Import / Export / Balanced), voltage, current, and power factor.
- **Shelly WebSocket Badge**: Direct link status and latency.
- **ESP-NOW Link Card**: Active packets received, RSSI, and sender MAC.
- **JSON Health API**: Accessible programmatically at `http://<ESP32_IP>/health`:
  ```json
  {
    "status": "HEALTHY",
    "uptime_sec": 308,
    "wifi": { "connected": true, "ip": "10.0.0.110", "rssi": -40 },
    "espnow": { "enabled": true, "packets": 398, "last_watts": -8.8, "rssi": -93 },
    "shelly": { "ws_connected": true, "ip": "10.0.0.187", "age_sec": 0 },
    "modbus": { "rs485_active": true, "queries": 4474, "crc_errors": 0, "rate_hz": 14.6 },
    "grid": { "source": "ESPNOW", "watts": -8.8, "flow": "EXPORT", "volts": 242.0 },
    "pv": { "watts": 0, "yield_kwh": 835.2 }
  }
  ```

---

## 📊 Feature Comparison: ESPHome vs Native C++

| Feature | ESPHome Solution (Primary) | Native C++ Solution (Advanced) |
| :--- | :--- | :--- |
| **Target Audience** | Home Assistant users, quick setup | Performance enthusiasts, zero-export perfection |
| **Grid Data Source** | MQTT (`shelly-em/realtime` via Shelly script) | Direct Shelly WebSocket (`ws://<IP>/rpc`) + ESP-NOW |
| **Update Interval** | 500 ms | **200 ms** (5 Hz) |
| **Modbus Core Allocation** | Shared FreeRTOS event loop | **Dedicated Core 1** (Dual-core isolation) |
| **Inverter Query Rate** | ~5–10 Hz | **14–15 Hz** (Instantaneous hardware response) |
| **MQTT Broker Needed?** | Yes | Optional (only for OpenDTU PV meter) |
| **Home Assistant Integration** | **Native 1-click discovery & control** | Sensor entities via MQTT / REST API |
| **Web Interface** | Standard ESPHome web server | Custom high-speed WebUI dashboard |
| **Hardware Failsafes** | Yes (10s watchdog cuts RS-485 bus power) | Yes (10s watchdog cuts RS-485 bus power) |
| **ESP-NOW Wireless Bridge** | No | **Yes** (ESP32-C3 SuperMini support) |

---

## 🛠️ Troubleshooting & Diagnostics

### 1. Inverter shows "Meter Communication Error" (F05 / F06)
- **Check Polarity**: Swap RS-485 **A** and **B** connections at the terminal block.
- **Check Baud Rate**: Hoymiles inverters require 9600 baud, 8 data bits, no parity, 1 stop bit (8N1).
- **Check Modbus Slave IDs**: Main Grid meter must be **Slave ID 2**; PV meter must be **Slave ID 1**.

### 2. Inverter oscillates (Hunting between import and export)
- Ensure the Shelly script is running with `update_interval_ms: 500` (for ESPHome) or native C++ polling at `200ms`.
- Standard 30s or 60s updates from native cloud integrations are too slow for zero-export control loops.

### 3. Failsafe activated (RS-485 power drops)
- The firmware automatically cuts RS-485 power if telemetry is stale (>10s) or Wi-Fi disconnects. Check Wi-Fi signal strength (`rssi`) and verify your MQTT broker / Shelly IP connectivity.
