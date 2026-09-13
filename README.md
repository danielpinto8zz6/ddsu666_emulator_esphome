# ESP32 Modbus RTU Emulator for Hoymiles Hybrid Inverters

This project uses an ESP32 (LILYGO T-CAN485) to emulate two independent CHINT DDSU666 Modbus RTU meters simultaneously on a single RS485 bus. It provides a bridge between standard Wi-Fi smart meters (Shelly, OpenDTU) and a Hoymiles Hybrid Inverter (e.g. HYS/HYB series), allowing the inverter to read grid exchange and PV generation data without requiring hardwired physical CT clamps.

## Architecture

- **Slave ID 2 (Grid Meter)**: Emulates the main house Grid meter using direct peer-to-peer **WebSockets** (`ws://10.0.0.187/rpc`) with the Shelly Pro EM 50A.
- **Slave ID 1 (PV Meter)**: Emulates a PV string meter using data published by an OpenDTU-onBattery system over **MQTT**.

## Fast-Polling Configuration (Zero-Export Stability)

Hybrid inverters rely on ultra-fast feedback (every ~500ms) to calculate how much power to charge or discharge from the battery in order to maintain a "Zero-Export" status (Autoconsumo mode). 

If the ESP32 receives delayed data from the Shelly meter (e.g. updating every 5 to 60 seconds), the inverter's PID control loop will aggressively over-correct, causing violent oscillations in Grid power (hunting).

To solve this optimally while preventing cosmetic math mismatches on your inverter dashboard, this project uses a **High-Frequency Minified JSON Payload**:

### 1. High-Frequency Telemetry (`shelly-em/realtime`)
To minimize Wi-Fi bandwidth and ESP32 CPU overhead while still keeping Voltage, Current, and Active Power perfectly in sync, a custom mJS script runs on the Shelly Pro EM 50A. It polls the meter every 500ms and pushes a tiny, minified JSON payload (containing `v`, `c`, `p`, `s`, `pf`, and `f`).

Navigate to your Shelly Pro EM web interface, go to **Scripts -> Create Script**, paste the following, and hit **Start**. Ensure "Enable on startup" is checked.

```javascript
// Shelly Pro EM 50A - High-Frequency MQTT Bridge
// Fast 500ms updates to prevent cosmetic mismatches on Hoymiles S-Miles app

let CONFIG = {
  update_interval_ms: 500, // Publish every 500ms
  mqtt_topic: "shelly-em/realtime"
};

if (MQTT.isConnected()) {
  Timer.set(CONFIG.update_interval_ms, true, function() {
    let em_status = Shelly.getComponentStatus("em1", 0);
    if (em_status) {
      let payload = JSON.stringify({
        "v": em_status.voltage,
        "c": em_status.current,
        "p": em_status.act_power,
        "s": em_status.aprt_power,
        "pf": em_status.pf,
        "f": em_status.freq
      });
      MQTT.publish(CONFIG.mqtt_topic, payload, 0, false);
    }
  });
}
```

The ESP32 instantly parses this fast payload and mathematically calculates the remaining Modbus telemetry (Apparent Power, Reactive Power) in real-time. This allows us to completely bypass the slow 60-second native status topic!

### 2. Energy Counters (`shelly-em/status/em1data:0`)
The ESP32 also subscribes to the Shelly's native `em1data:0` topic. This topic pushes the `total_act_energy` and `total_act_ret_energy` (Import/Export kWh) natively every 60 seconds. Because these are purely statistical metrics used by the Hoymiles Cloud dashboard to calculate "Lifetime Yield", a 60-second delay is perfectly acceptable. Offloading these heavy counters from the fast 500ms real-time payload keeps the pipeline incredibly lean.

**Note**: In your Shelly MQTT Settings, ensure `"Generic status update over MQTT"` is CHECKED so the energy counters are published. You can safely UNCHECK `"RPC status notifications over MQTT"` to save bandwidth.

## Hardware Setup
- **Board**: LILYGO T-CAN485 (ESP32) or Waveshare ESP32-S3 RS485
- **RS485 TX/RX**: Handled by internal hardware serial on Core 1
- **RS485 Enable Pin**: Automatic hardware flow control and failsafe power cut

## Native C++ Firmware (PlatformIO)

This repository includes a high-performance native C++ implementation designed to replace ESPHome when sub-millisecond Modbus RTU response times are required for zero-export PID loops.

### Key Benefits
* **Dual-Core Isolation**: Core 1 handles RS485 Modbus RTU polling with zero jitter; Core 0 handles Wi-Fi, MQTT, and OTA.
* **Low Footprint**: ~750KB Flash / ~15% RAM usage.
* **Instant Failsafe**: Drops RS485 bus power when Grid MQTT times out (>10s) or Wi-Fi drops, immediately forcing Hoymiles to safe idle.

### Configuration
1. Copy `include/secrets.h.example` to `include/secrets.h` if not already present.
2. Edit `include/secrets.h` with your Wi-Fi SSID, password, and MQTT broker IP.

### Build & Flash

Activate the virtual environment containing PlatformIO (or use your global `pio`):
```bash
source .venv/bin/activate
```

**Build firmware:**
```bash
# For LilyGO T-CAN485
pio run -e lilygo-t-can485

# For Waveshare ESP32-S3
pio run -e waveshare-esp32-s3
```

**Upload via USB:**
```bash
pio run -e lilygo-t-can485 -t upload
```

**Serial Monitor:**
```bash
pio device monitor -b 115200
```

**Over-The-Air (OTA) Flash:**
Once flashed with the C++ firmware, future updates can be pushed over Wi-Fi:
```bash
pio run -e lilygo-t-can485 -t upload --upload-port <ESP32_IP_ADDRESS>
```
