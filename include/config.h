#pragma once

#include <Arduino.h>

// ================================================================
// BOARD PINOUT DEFINITIONS
// ================================================================

#if defined(BOARD_WAVESHARE_ESP32S3)
  // Waveshare ESP32-S3 RS485/CAN
  #define BOARD_NAME          "Waveshare ESP32-S3 RS485"
  #define PIN_RS485_TX        17
  #define PIN_RS485_RX        18
  #define PIN_RS485_DE_RE     21
  #define HAS_RS485_POWER_PINS 0
#else
  // LILYGO T-CAN485 (ESP32) - DEFAULT TARGET
  #define BOARD_NAME          "LILYGO T-CAN485"
  #define PIN_RS485_TX        22  // Hardware Serial TX (GPIO22)
  #define PIN_RS485_RX        21  // Hardware Serial RX (GPIO21)
  #define PIN_RS485_DE_RE     17  // Direction Control DE/!RE (GPIO17)
  #define PIN_RS485_5V_EN     16  // 5V Boost Power Enable (GPIO16)
  #define PIN_RS485_CHIP_EN   19  // Transceiver Enable (GPIO19)
  #define HAS_RS485_POWER_PINS 1
#endif

// RS485 Modbus UART Settings
#define RS485_BAUD_RATE      9600
#define RS485_CONFIG         SERIAL_8N1

// Modbus RTU Slave IDs
#define MODBUS_SLAVE_GRID    2  // Slave ID 2: Main house Grid Meter (Hoymiles standard)
#define MODBUS_SLAVE_PV      1  // Slave ID 1: PV Inverter String Meter

// Watchdogs & Timings
#define WATCHDOG_TIMEOUT_SEC 10   // Fail-safe timeout in seconds (Grid & PV)

// Shelly WebSocket Configuration
#define SHELLY_IP                 "10.0.0.187"
#define SHELLY_WS_PORT            80
#define SHELLY_WS_PATH            "/rpc"
#define SHELLY_POLL_INTERVAL_MS   200   // 200ms ultra-fast polling for zero-export control
#define SHELLY_ENERGY_INTERVAL_MS 60000 // 60s energy counter interval

// OpenDTU MQTT Topics
#define TOPIC_OPENDTU_POWER       "solar/ac/power"
#define TOPIC_OPENDTU_YIELD       "solar/ac/yieldtotal"

// Web Interface Options (Set to 0 for headless API-only mode)
#define ENABLE_WEB_DASHBOARD      1

// ESP-NOW Receiver Options
#define ENABLE_ESPNOW             1   // 1 = Enable concurrent ESP-NOW receiver, 0 = Disable
// Grid Power Source Priority:
// 0 = Shelly WebSocket Only
// 1 = ESP-NOW Only (Lolin C3 Bridge)
// 2 = Auto (ESP-NOW takes priority if packets received in last 5s, else Shelly WS fallback)
#define GRID_SOURCE_PRIORITY      2

// Wi-Fi Channel Selection (0 = Auto-scan, 1-13 = Force channel to match ESP-NOW peer)
#define WIFI_FORCE_CHANNEL        1



