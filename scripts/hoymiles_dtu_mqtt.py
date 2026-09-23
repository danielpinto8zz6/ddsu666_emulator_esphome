#!/usr/bin/env python3
"""
Hoymiles DTU-Lite-S / DTU-WLite to MQTT Bridge
Polls Hoymiles DTU-Lite-S at local IP and publishes solar telemetry to MQTT
for DDSU666 Modbus RTU emulator, ESPHome, Home Assistant, etc.
"""

import argparse
import asyncio
import json
import logging
import os
import sys
import time

try:
    import paho.mqtt.client as mqtt
except ImportError:
    print("Error: paho-mqtt is required. Run: pip install paho-mqtt")
    sys.exit(1)

try:
    from hoymiles_wifi.dtu import DTU
except ImportError:
    print("Error: hoymiles-wifi is required. Run: pip install hoymiles-wifi")
    sys.exit(1)

logging.basicConfig(
    format="%(asctime)s [%(levelname)s] %(message)s",
    level=logging.INFO,
)
logger = logging.getLogger("hoymiles_dtu_mqtt")

async def poll_and_publish(args):
    logger.info(f"Connecting to MQTT broker at {args.mqtt_host}:{args.mqtt_port}...")
    client = mqtt.Client(client_id=args.mqtt_client_id)
    if args.mqtt_user:
        client.username_pw_set(args.mqtt_user, args.mqtt_pass)

    client.connect(args.mqtt_host, args.mqtt_port, 60)
    client.loop_start()

    logger.info(f"Initializing DTU client for {args.dtu_host}...")
    dtu = DTU(args.dtu_host)

    last_power = None
    last_yield = None

    while True:
        try:
            res = await dtu.get_real_data_new()
            if res:
                # Extract active power (W) and total yield (kWh)
                power_w = 0.0
                yield_kwh = 0.0

                if hasattr(res, "dtu_power") and res.dtu_power is not None:
                    power_w = float(res.dtu_power) * 0.1
                elif hasattr(res, "sgs_data") and res.sgs_data:
                    power_w = sum(float(s.active_power) * 0.1 for s in res.sgs_data if hasattr(s, "active_power") and s.active_power is not None)

                if hasattr(res, "pv_data") and res.pv_data:
                    total_wh = sum(float(p.energy_total) for p in res.pv_data if hasattr(p, "energy_total") and p.energy_total is not None)
                    yield_kwh = total_wh / 1000.0

                logger.info(f"[DTU] Power: {power_w:.1f} W | Total Yield: {yield_kwh:.3f} kWh")

                # Publish to OpenDTU compatible topics
                client.publish(args.topic_power, f"{power_w:.1f}", qos=0, retain=False)
                if yield_kwh > 0:
                    client.publish(args.topic_yield, f"{yield_kwh:.3f}", qos=0, retain=True)

                # Telemetry JSON payload
                telemetry = {
                    "power_w": round(power_w, 1),
                    "yield_kwh": round(yield_kwh, 3),
                    "timestamp": int(time.time()),
                }
                client.publish(args.topic_telemetry, json.dumps(telemetry), qos=0, retain=False)

        except Exception as e:
            logger.warning(f"Error fetching data from DTU: {e}")

        await asyncio.sleep(args.interval)

def main():
    parser = argparse.ArgumentParser(description="Hoymiles DTU-Lite-S to MQTT Bridge")
    parser.add_argument("--dtu-host", default=os.getenv("HOYMILES_DTU_IP", "10.0.0.164"), help="Hoymiles DTU IP address")
    parser.add_argument("--mqtt-host", default=os.getenv("MQTT_BROKER_IP", "10.0.0.187"), help="MQTT broker IP")
    parser.add_argument("--mqtt-port", type=int, default=int(os.getenv("MQTT_PORT", "1883")), help="MQTT broker port")
    parser.add_argument("--mqtt-user", default=os.getenv("MQTT_USER", ""), help="MQTT username")
    parser.add_argument("--mqtt-pass", default=os.getenv("MQTT_PASSWORD", ""), help="MQTT password")
    parser.add_argument("--mqtt-client-id", default="hoymiles_dtu_bridge", help="MQTT client ID")
    parser.add_argument("--topic-power", default="solar/ac/power", help="MQTT topic for active power in W")
    parser.add_argument("--topic-yield", default="solar/ac/yieldtotal", help="MQTT topic for total yield in kWh")
    parser.add_argument("--topic-telemetry", default="hoymiles/telemetry", help="MQTT topic for JSON telemetry")
    parser.add_argument("--interval", type=int, default=10, help="Polling interval in seconds")

    args = parser.parse_args()

    try:
        asyncio.run(poll_and_publish(args))
    except KeyboardInterrupt:
        logger.info("Exiting...")

if __name__ == "__main__":
    main()
