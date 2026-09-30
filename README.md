# Dual CHINT DDSU666 Modbus RTU Emulator for Hoymiles Hybrid Inverters

This repository contains firmware to emulate **two independent CHINT DDSU666 Modbus RTU meters simultaneously** on a single RS-485 bus using an ESP32. It bridges wireless smart meters (**Shelly Pro EM**, **OpenDTU-onBattery**) directly to a **Hoymiles Hybrid Inverter** (e.g., HYS/HYB series) without requiring hardwired physical CT clamps.

---

## 📑 Table of Contents

- [Overview & Architecture](#-overview--architecture)
- [Why This Emulator Outperforms Direct Physical Wiring](#-why-this-emulator-outperforms-direct-physical-wiring)
- [Multi-Zone Precision Damping Filter](#-multi-zone-precision-damping-filter)
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
  - [Embedded SPA WebUI & Live WebSocket Telemetry (Port 81)](#embedded-spa-webui--live-websocket-telemetry-port-81)
  - [Home Assistant MQTT Auto-Discovery](#-home-assistant-mqtt-auto-discovery)
  - [Dedicated Chint DDSU666 Master Reader](#-dedicated-chint-ddsu666-master-reader-chint_ddsu666_reader)
- [Feature Comparison: ESPHome vs Native C++](#-feature-comparison-esphome-vs-native-c)
- [Troubleshooting & Diagnostics](#-troubleshooting--diagnostics)

---

## 🌐 Overview & Architecture

Hoymiles HYS/HYB hybrid inverters poll two CHINT DDSU666 meters over RS-485 Modbus RTU at 9600 baud (8N1):

```
┌─────────────────────────────────┐
│  Chint DDSU666 Reader           │──[WebSocket :81]─┐
│  (chint-reader.local / .197)    │                  │
├─────────────────────────────────┤                  ▼
│  Shelly Pro EM 50A              │──[WebSocket / RPC]──>┌───────────────────────────────────────────────┐
│  (Main House Grid Meter)        │                      │  DDSU666 Dual-Meter Emulator                  │──[RS-485 Modbus RTU]──> Hoymiles Hybrid Inverter
├─────────────────────────────────┤                      │  (ddsu666.local / .169)                       │   (COM Port Pins 1 & 2)  (HYS / HYB Series)
│  OpenDTU-onBattery / Solar      │──[MQTT Telemetry]───>│  • Slave ID 2: Grid Meter                     │   (~14.8 Hz Polling, 0 CRC Errors)
│  (10.0.0.1:1883)                │                      │  • Slave ID 1: PV Solar Meter                 │
└─────────────────────────────────┘                      │  • WebSocket Live Stream (:81)                │
                                                         │  • HA MQTT Auto-Discovery (14 Sensors)       │
                                                         └───────────────────────────────────────────────┘
                                                                 │
                                                                 ▼
                                                        ┌───────────────────────────────────────────────┐
                                                        │  Home Assistant Dashboard                     │
                                                        │  (Instant Device Discovery + State Telemetry) │
                                                        └───────────────────────────────────────────────┘
```

- **Slave ID 2 (Grid Meter)**: Emulates the main grid connection meter. Provides active power, voltage, current, power factor, frequency, and cumulative import/export kWh for the inverter's **Zero-Export (Autoconsumo)** PID control loop.
- **Slave ID 1 (PV Meter)**: Emulates a PV string meter using generation data polled from **OpenDTU-onBattery** via MQTT. Allows the inverter to display solar generation and battery charging on the Hoymiles S-Miles app without a separate physical CT.
- **mDNS Discovery**: Zero-configuration `.local` domain access (`http://ddsu666.local` and `http://chint-reader.local`).
- **Home Assistant Auto-Discovery**: 14 sensors announced automatically to MQTT with device registry integration, LWT availability, and a 2-second telemetry cadence.

---

## ⚡ Why This Emulator Outperforms Direct Physical Wiring

A common question is: *is this wireless emulator setup actually better than wiring a physical CHINT DDSU666 directly to the Hoymiles inverter with an RS-485 cable?*

**Yes, both in control theory and in practice.**

| Feature | Direct Physical CHINT DDSU666 | Dual-Meter Emulator Architecture |
| :--- | :--- | :--- |
| **Signal Quality & Noise Rejection** | Raw instantaneous power with +/- 30W to 50W reactive phase jitter at 14.5 Hz. Inverter PID continuously hunts. | **Multi-Zone Adaptive Damping Filter**: Attenuates reactive noise by >96% while maintaining instant 0 ms response on real appliance loads. |
| **Fault Tolerance & Redundancy** | Single point of failure. If the meter glitches, reboots, or wire is cut, zero-export fails completely. | **Automatic Dual-Source Failover**: Primary high-frequency Chint WebSocket stream seamlessly fails over to Shelly Pro EM in 4 seconds. |
| **Dual Meter Support (Grid + PV)** | Only provides Grid Meter (Slave ID 2). A second physical meter must be bought and daisy-chained for PV. | **Simultaneous Dual-Meter Emulation**: Emulates Slave ID 2 (Grid) and Slave ID 1 (PV) populated live from OpenDTU MQTT. |
| **Installation & Safety** | Requires running long physical RS-485 cable through walls/conduit from electrical panel to inverter. Prone to ground loops and lightning surges. | **Galvanic & Wireless Isolation**: Reader sits in the main breaker box; emulator sits right by the inverter. Telemetry bridges over local Wi-Fi. |
| **Diagnostics & Observability** | Completely black-box. No way to inspect query rate, turnaround latency, or CRC errors without a logic analyzer. | **Embedded Diagnostic Dashboard**: Web UI with live power gauges, `/health` JSON endpoint, 14.5 Hz query rate monitor, and passwordless OTA updates. |

---

## 🎯 Multi-Zone Precision Damping Filter

### 1. The Physics of the +/- 50W Measurement Jitter
In modern households, typical idle baseline consumption involves switch-mode power supplies, power factor correction capacitors, and LED drivers. Even when net active power consumption is near 0W, there is frequently 2.5A to 2.8A of reactive current flowing at 240V (~650 VA apparent power) with a power factor close to zero (-0.05 to +0.08).

Because apparent power is ~650 VA, a microscopic phase angle fluctuation of just +/- 0.05 in the power factor calculation swings the physical meter's instantaneous active power calculation between **-50W and +70W** on every single AC cycle.

### 2. The Inverter PID Hunting Problem
The Hoymiles inverter queries the meter at **14.5 Hz (every 68 ms)**. However, the physical power electronics inside the inverter (DC bus capacitors, MOSFET gates, and MPPT stages) have a settling time of **500 ms to 1500 ms**.

When fed raw instantaneous readings:
1. The inverter's internal PI (Proportional-Integral) controller treats every 68 ms reactive ripple packet as an urgent load change.
2. Because the inverter takes ~1 second to respond, its corrections arrive out of phase with the noise.
3. This creates **limit-cycle PID hunting**: the inverter constantly modulates its PV output up and down by 30W to 50W, causing unnecessary thermal stress on its switching transistors.

### 3. The Solution: Multi-Zone Damping Architecture (`GridFilter`)
The filter logic is decoupled into a dedicated module ([src/grid_filter.h](file:///home/danielpinto/Documentos/ddsu666_emulator_esphome/src/grid_filter.h) and [src/grid_filter.cpp](file:///home/danielpinto/Documentos/ddsu666_emulator_esphome/src/grid_filter.cpp)). It applies a continuous non-linear multi-zone response curve:

* **Zone 1 (Deviation <= 35W — Zero-Export Equilibrium)**:
  Strong damping with time constant tau = 2000 ms (alpha = ~0.034 at 14.5 Hz). Attenuates high-frequency reactive phase jitter by over 96%, keeping the zero-export baseline calm and preventing inverter PID hunting.
* **Zone 2 (Deviation 35W to 120W — Normal Household Drifts)**:
  Smoothly scales tau from 2000 ms down to 400 ms. Ensures smooth, overshoot-free convergence when household loads drift naturally (lighting, TV, refrigerator).
* **Zone 3 (Deviation 120W to 200W — Moderate Loads)**:
  Fast tracking with tau scaling from 400 ms down to 100 ms.
* **Zone 4 (Deviation >= 200W — Major Appliances)**:
  **Instant bypass (0 ms delay)**. When an oven, kettle, heat pump, or microwave turns on or off, the filter immediately snaps to the raw reading with zero latency, ensuring instant zero-export response without unnecessary grid import or export.

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

### 3. Seeed Studio XIAO ESP32-S3 + XIAO RS485 Expansion Board
* Ultra-compact ESP32-S3 with plug-and-play [XIAO RS485 Expansion Board](https://wiki.seeedstudio.com/XIAO-RS485-Expansion-Board/).
* **RS-485 TX**: GPIO 5 (XIAO D4)
* **RS-485 RX**: GPIO 6 (XIAO D5)
* **Direction Control (DE/!RE)**: GPIO 3 (XIAO D2)
* **Expansion Board Switches**:
  * **120R Switch (SW1)**: Set to **ON** to enable the 120Ω bus termination resistor (standard when connected directly to inverter).
  * **5V IN/OUT Switch (SW2)**:
    * Set to **OUT** when powering the XIAO ESP32-S3 via USB-C (default).
    * Set to **IN** only when supplying 5V externally via the screw terminals (never set to OUT with external power connected!).

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
├── ddsu666_xiao.yaml               # ESPHome Configuration: Seeed Studio XIAO ESP32-S3
├── secrets.yaml.example            # ESPHome Secrets template
├── shelly_pro_em_script.js         # Shelly Pro EM 500ms high-frequency mJS script
│
├── platformio.ini                  # PlatformIO configuration (Native C++ Firmware)
├── include/                        # Native C++ Headers
│   ├── config.h                    # Board pinouts, Modbus, and Shelly configuration
│   ├── meter_data.h                # In-memory DDSU666 register state & conversions
│   ├── web_ui_gz.h                 # Compressed WebUI dashboard binary
│   └── secrets.h.example           # Native C++ secrets template
├── src/                            # Native C++ Source Code
│   ├── main.cpp                    # Dual-core tasks & FreeRTOS initialization
│   ├── modbus_rtu.cpp / .h         # High-speed Modbus RTU slave engine (Core 1)
│   ├── meter_data.cpp              # Telemetry sync & Modbus FP32 conversions
│   ├── grid_filter.cpp / .h        # Multi-Zone Precision Damping Filter for Zero-Export
│   ├── web_server.cpp / .h         # REST API & WebUI dashboard (Core 0)
│   └── telemetry/                  # Telemetry Source & Sink Abstraction Layer
│       ├── telemetry_types.h       # Normalized Grid & PV measurement structures
│       ├── telemetry_source.h      # ITelemetrySource & ITelemetrySink interfaces
│       ├── telemetry_manager.cpp/.h# Core hub: arbitration, filtering, watchdogs & failsafes
│       └── sources/                # Pluggable Technology Drivers
│           ├── chint_ws_source.cpp/.h      # Chint DDSU666 Master WebSocket client
│           ├── shelly_ws_source.cpp/.h     # Shelly Pro EM RPC WebSocket client
│           └── opendtu_mqtt_source.cpp/.h  # OpenDTU Solar PV MQTT client
├── web_ui_source.html              # Uncompressed HTML/CSS/JS source for WebUI
│
└── chint_ddsu666_reader/           # Clean Core Split RS-485 Master Reader (14.5 Hz)
    ├── platformio.ini              # Xiao ESP32-S3 build environment
    ├── include/                    # Reader configuration and Modbus definitions
    └── src/                        # Dedicated Core 1 Modbus engine & Core 0 WebSocket server
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

# For Seeed Studio XIAO ESP32-S3 RS485:
esphome run ddsu666_xiao.yaml
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
  * **Core 0**: Handles Wi-Fi, direct Shelly WebSockets, direct Hoymiles DTU TCP client, MQTT, WebUI, and OTA updates.
* **Direct Shelly WebSocket Client (200ms)**: Connects directly to `ws://<SHELLY_IP>/rpc` without requiring an MQTT broker or Shelly script.
* **Direct Hoymiles DTU-Lite-S / DTU-WLite Polling**: Direct TCP polling on port `10081` with hardware-accelerated AES-128-GCM decryption and automatic clock sync—no OpenDTU or external gateway required.
* **Rolling Modbus Health & Query Rate Monitor**: WebUI displays live query frequency (~14.5 Hz) and inverter link status.
* **Dedicated Chint DDSU666 Modbus Master Support**: High-speed wireless telemetry stream from physical meter at ~14.6 Hz.

---

### Configuration (`secrets.h` & `config.h`)

1. Copy [`include/secrets.h.example`](file:///home/danielpinto/Documentos/ddsu666_emulator_esphome/include/secrets.h.example) to `include/secrets.h`:
   ```bash
   cp include/secrets.h.example include/secrets.h
   ```
2. Edit `include/secrets.h` with your network configuration:
   ```c
   #pragma once

   // Wi-Fi Credentials (shared by Lilygo, XIAO, and ESP32-C3 transmitter)
   #define WIFI_SSID       "YourWiFiSSID"
   #define WIFI_PASSWORD   "YourWiFiPassword"

   // Shelly Pro EM IP Address (Grid Meter)
   #define SHELLY_IP       "10.0.0.187"

   // PV Generation Source Mode:
   // 0 = None / Manual
   // 1 = OpenDTU via MQTT
   // 2 = Direct Hoymiles DTU-Lite-S / DTU-WLite TCP Polling (Recommended)
   #define PV_SOURCE_MODE  2

   // Hoymiles DTU-Lite-S (when PV_SOURCE_MODE == 2)
   #define HOYMILES_DTU_IP "10.0.0.164"

   // MQTT Broker Configuration (for OpenDTU PV meter, or telemetry bridge)
   #define MQTT_BROKER_IP  "10.0.0.187"
   #define MQTT_PORT       1883
   #define MQTT_USER       ""
   #define MQTT_PASSWORD   ""
   #define MQTT_CLIENT_ID  "ddsu666-emulator"

   // OTA Password
   #define OTA_PASSWORD    "admin"
   ```

#### Standalone Python Companion Script:
If you are using ESPHome or want to bridge your Hoymiles DTU-Lite-S directly to MQTT without running native C++ on the meter emulator:
```bash
python3 scripts/hoymiles_dtu_mqtt.py --dtu-host 10.0.0.164 --mqtt-host 10.0.0.187 --interval 10
```

---

### Compiling & Flashing (PlatformIO)

Activate your Python virtual environment or use global `pio`:

```bash
# 1. Build firmware (choose target: lilygo-t-can485, waveshare-esp32-s3, or xiao-esp32-s3)
pio run -e xiao-esp32-s3

# 2. Flash via USB
pio run -e xiao-esp32-s3 -t upload

# 3. View real-time logs
pio device monitor -b 115200
```

#### Over-The-Air (OTA) Updates:
Future updates can be uploaded directly over HTTP using the included script or curl:
```bash
# Using the helper script:
./scripts/flash_ota.sh xiao <ESP32_IP> --build

# Or directly via curl:
curl -u "admin:admin" -F "file=@.pio/build/xiao-esp32-s3/firmware.bin" -H "Expect:" http://<ESP32_IP>/update?password=admin
```

---

### Embedded SPA WebUI & Live WebSocket Telemetry (Port 81)

Both the **DDSU666 Emulator** and the **Chint Master Reader** feature modern Single-Page Applications (SPA) with zero server-side rendering (SSR) overhead:

- **Zero-Configuration mDNS**:
  - Emulator: **`http://ddsu666.local`** (or `http://<EMULATOR_IP>/`)
  - Chint Reader: **`http://chint-reader.local`** (or `http://<CHINT_IP>/`)
- **Real-Time WebSocket Stream (Port 81)**:
  - Connects directly to `ws://<DEVICE_IP>:81` for sub-200ms telemetry streaming at 5 Hz.
  - Interactive **Live Mode Toggle**: Pause telemetry to eliminate browser CPU/network consumption, with automatic reconnect upon unpausing.
- **Bi-Directional Action Control**: Change Grid priority mode, PV priority, or filter damping algorithms directly from the UI without rebooting.
- **Diagnostic Health JSON API**: Accessible programmatically at `http://ddsu666.local/health` or `http://<IP>/health`:
  ```json
  {
    "status": "HEALTHY",
    "uptime_sec": 31541,
    "free_heap": 260228,
    "priority": { "mode": 0, "description": "Auto: Chint WS -> Shelly WS" },
    "pv_priority": { "mode": 2, "description": "OpenDTU MQTT Only" },
    "wifi": { "connected": true, "ip": "10.0.0.169", "rssi": -46 },
    "chint": { "ws_connected": true, "ip": "10.0.0.197", "age_sec": 0 },
    "shelly": { "ws_connected": true, "ip": "10.0.0.187", "age_sec": 0.3 },
    "opendtu": { "mqtt_connected": true, "age_sec": 0 },
    "modbus": {
      "rs485_active": true,
      "queries": 312781,
      "crc_errors": 0,
      "rate_hz": 14.8,
      "inverter_connected": true
    },
    "grid": {
      "source": "CHINT_WS",
      "filter": "damped",
      "watts": -66.5,
      "flow": "EXPORT",
      "volts": 246.7,
      "amps": 2.31,
      "pf": -0.10,
      "freq": 50.0,
      "import_kwh": 19.25,
      "export_kwh": 1.45
    },
    "pv": { "source": "OPENDTU_MQTT", "watts": 0, "yield_kwh": 0 }
  }
  ```

---

### 🏠 Home Assistant MQTT Auto-Discovery

The emulator connects to your local MQTT broker (configured in `AppConfig` or WebUI) using a unified, non-blocking `MqttManager` transport. Upon connection, it automatically announces 14 first-class sensor entities to Home Assistant:

- **Discovery Prefix**: `homeassistant/sensor/ddsu666_emulator/<sensor_id>/config` (Retained)
- **State Topic**: `ddsu666_emulator/state` (Published every 2.0 seconds)
- **Availability Topic**: `ddsu666_emulator/status` (`online` / `offline` with MQTT Last Will and Testament)
- **Device Registry**: All sensors group under a single device named **`DDSU666 Modbus Emulator`** with a clickable link directly to `http://ddsu666.local`.

#### Published Sensor Entities:
| Entity ID | Friendly Name | Unit | Device Class | State Class | Description |
| :--- | :--- | :---: | :---: | :---: | :--- |
| `grid_power` | Grid Active Power | W | `power` | `measurement` | Real-time net grid import (+) / export (−) |
| `pv_power` | PV Active Power | W | `power` | `measurement` | Solar generation feeding the emulator |
| `grid_voltage` | Grid Voltage | V | `voltage` | `measurement` | Line RMS voltage |
| `grid_current` | Grid Current | A | `current` | `measurement` | Line RMS current |
| `grid_frequency` | Grid Frequency | Hz | `frequency`| `measurement` | AC line frequency (50 Hz / 60 Hz) |
| `grid_power_factor` | Grid Power Factor | — | `power_factor` | `measurement` | Grid displacement power factor |
| `import_energy` | Grid Import Energy | kWh | `energy` | `total_increasing` | Cumulative imported energy |
| `export_energy` | Grid Export Energy | kWh | `energy` | `total_increasing` | Cumulative exported solar energy |
| `pv_yield` | PV Total Yield | kWh | `energy` | `total_increasing` | Cumulative solar yield from OpenDTU |
| `modbus_query_rate` | Inverter Query Rate | Hz | — | `measurement` | Real-time Modbus query frequency (~14.8 Hz) |
| `modbus_crc_errors` | Modbus CRC Errors | — | — | `total_increasing` | RS-485 transmission integrity counter |
| `active_grid_source`| Active Grid Source | — | — | — | Current active provider (`CHINT_WS`, `SHELLY_WS`) |
| `active_pv_source` | Active PV Source | — | — | — | Current solar provider (`OPENDTU_MQTT`, `NONE`) |
| `filter_mode` | Power Filter Mode | — | — | — | Active smoothing algorithm (`damped`, `raw`) |

---

### 📡 Dedicated Chint DDSU666 Master Reader (`chint_ddsu666_reader/`)

For installations where the physical Chint DDSU666 meter is in a main distribution box, the `chint_ddsu666_reader` firmware turns a second Seeed XIAO ESP32-S3 into a high-speed wireless bridge:

- **Hardware Task Isolation**: Dedicated Core 1 Modbus RTU master querying the physical meter at **~14.6 Hz** (sub-60ms response latency).
- **Persistent NVS Preferences**: Configured Modbus Slave ID is persisted in ESP32 NVS under namespace `"chint"`, surviving power cuts and reboots.
- **WebSocket Streaming**: Broadcasts fresh telemetry frames directly over `ws://chint-reader.local:81` to the emulator and web dashboards.
- **Embedded Web UI & OTA**: Reachable at `http://chint-reader.local` for live diagnostics and passwordless firmware updates.

---

## 📊 Feature Comparison: ESPHome vs Native C++

| Feature | ESPHome (Legacy) | Native C++ / FreeRTOS (Current Production) |
| :--- | :--- | :--- |
| **Status** | Legacy reference | **Primary production architecture** |
| **Grid Data Sources** | MQTT (`shelly-em/realtime`) | **Chint WebSocket (:81) + Shelly WS (:80)** |
| **Priority Arbitration** | Manual config | **Automatic dynamic failover (Chint -> Shelly in 4s)** |
| **Update Cadence** | ~500 ms (cooperative) | **Instant hardware turnaround (5 Hz stream, 14.8 Hz Modbus)** |
| **Modbus Core Allocation**| Shared event loop | **Dedicated Core 1 FreeRTOS Task (0 jitter)** |
| **Inverter Query Rate** | ~5–10 Hz | **~14.8 Hz (Full inverter speed, 0 CRC errors)** |
| **Home Assistant Integration** | Native ESPHome | **Automatic MQTT Discovery (14 Entities + Device Card + LWT)** |
| **mDNS Discovery** | Standard ESPHome | **`ddsu666.local` & `chint-reader.local` (HTTP + WS)** |
| **Web Interface** | Basic ESPHome server | **Compressed SPA Dashboard + WebSocket stream + Live toggle** |
| **Power Smoothing** | Simple moving average | **Multi-Zone Non-Linear Adaptive Damping Filter** |
| **Hardware Failsafes** | Yes (10s watchdog) | **Yes (Dual-layer hardware task watchdog + RS-485 bus power)** |

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
