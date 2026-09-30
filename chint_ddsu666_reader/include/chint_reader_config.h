#pragma once

#include <Arduino.h>

#if __has_include("secrets.h")
#include "secrets.h"
#endif

// ================================================================
// HARDWARE PINOUT - Seeed Studio XIAO ESP32-S3 + RS485 Module
// ================================================================
// D4 = GPIO5 (UART1 TX)
// D5 = GPIO6 (UART1 RX)
// D2 = GPIO3 (RS485 Direction Control DE / !RE)
// D13 = GPIO21 (Onboard User LED, Active LOW)
#define PIN_RS485_TX          5
#define PIN_RS485_RX          6
#define PIN_RS485_DE_RE       3
#define STATUS_LED_PIN        21

// ================================================================
// CHINT DDSU666 MODBUS RTU CONFIGURATION
// ================================================================
#define DEFAULT_SLAVE_ID      2       // Chint DDSU666 Slave ID (Default 2 on this installation, auto-scans 1/2)
#define RS485_BAUD_RATE       9600    // Default Factory Baud Rate for Chint DDSU666
#define RS485_CONFIG          SERIAL_8N1

// Ultra-Fast Polling Timing:
// At 9600 baud, 1 char = 1.04ms. Request (8B) = 8.3ms, Response (37B) = 38.5ms.
// Total transaction = ~47ms. 10ms turnaround gives 15-17 Hz polling!
#define MODBUS_INTER_FRAME_DELAY_MS  8       // Turnaround silence between queries
#define MODBUS_TIMEOUT_MS            120     // Response timeout before retry
#define ENERGY_POLL_INTERVAL_MS      30000   // Poll energy accumulators (0x4000) every 30s

// ================================================================
// WEBSOCKET SERVER CONFIGURATION
// ================================================================
#define WS_SERVER_PORT        81      // Dedicated high-speed WebSocket Server Port
#define WS_EMIT_INTERVAL_MIN  0       // 0 = Emit immediately on every fresh Modbus reading

// ================================================================
// WI-FI CREDENTIALS & WEB OTA
// ================================================================
#ifndef WIFI_SSID
#define WIFI_SSID             "AccessPoint"
#endif

#ifndef WIFI_PASSWORD
#define WIFI_PASSWORD         "65809240"
#endif

#ifndef OTA_PASSWORD
#define OTA_PASSWORD          ""
#endif

// ================================================================
// MQTT CONFIGURATION (Home Assistant Auto-Discovery)
// ================================================================
#ifndef MQTT_BROKER_IP
#define MQTT_BROKER_IP        "10.0.0.1"
#endif

#ifndef MQTT_PORT
#define MQTT_PORT             1883
#endif

#ifndef MQTT_USER
#define MQTT_USER             ""
#endif

#ifndef MQTT_PASSWORD
#define MQTT_PASSWORD         ""
#endif

#define CHINT_MQTT_CLIENT_ID  "chint-ddsu666-master"
#define CHINT_MQTT_TOPIC      "chint_ddsu666/telemetry"

