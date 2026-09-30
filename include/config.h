#pragma once

#include <Arduino.h>

#if __has_include("secrets.h")
#include "secrets.h"
#endif

// ================================================================
// BOARD PINOUT DEFINITIONS
// ================================================================

#if defined(BOARD_WAVESHARE_ESP32S3)
  // Waveshare ESP32-S3 RS485/CAN
  constexpr const char* BOARD_NAME    = "Waveshare ESP32-S3 RS485";
  constexpr int PIN_RS485_TX          = 17;
  constexpr int PIN_RS485_RX          = 18;
  constexpr int PIN_RS485_DE_RE       = 21;
  #define HAS_RS485_POWER_PINS 0
#elif defined(BOARD_XIAO_ESP32S3)
  // Seeed Studio XIAO ESP32-S3 + XIAO RS485 Expansion Board
  // D4 = GPIO5 (TX), D5 = GPIO6 (RX), D2 = GPIO3 (DE/RE)
  constexpr const char* BOARD_NAME    = "Seeed Studio XIAO ESP32-S3 RS485";
  constexpr int PIN_RS485_TX          = 5;
  constexpr int PIN_RS485_RX          = 6;
  constexpr int PIN_RS485_DE_RE       = 3;
  #define HAS_RS485_POWER_PINS 0
#else
  // LILYGO T-CAN485 (ESP32) - DEFAULT TARGET
  constexpr const char* BOARD_NAME    = "LILYGO T-CAN485";
  constexpr int PIN_RS485_TX          = 22; // Hardware Serial TX (GPIO22)
  constexpr int PIN_RS485_RX          = 21; // Hardware Serial RX (GPIO21)
  constexpr int PIN_RS485_DE_RE       = 17; // Direction Control DE/!RE (GPIO17)
  constexpr int PIN_RS485_5V_EN       = 16; // 5V Boost Power Enable (GPIO16)
  constexpr int PIN_RS485_CHIP_EN     = 19; // Transceiver Enable (GPIO19)
  #define HAS_RS485_POWER_PINS 1
#endif

// RS485 Modbus UART Settings
constexpr uint32_t RS485_BAUD_RATE        = 9600;
#define RS485_CONFIG                      SERIAL_8N1

// Modbus RTU Slave IDs
constexpr uint8_t MODBUS_SLAVE_GRID       = 2;  // Slave ID 2: Main house Grid Meter (Hoymiles standard)
constexpr uint8_t MODBUS_SLAVE_PV         = 1;  // Slave ID 1: PV Inverter String Meter

// Watchdogs & Timings
constexpr uint32_t WATCHDOG_TIMEOUT_SEC   = 10; // Fail-safe timeout in seconds (Grid & PV)

// Chint DDSU666 Master (Seeed Studio XIAO ESP32-S3) Configuration
#ifndef CHINT_XIAO_IP
#define CHINT_XIAO_IP                     "10.0.0.197"
#endif
constexpr uint16_t CHINT_XIAO_WS_PORT     = 81;
constexpr const char* CHINT_XIAO_WS_PATH  = "/";
constexpr uint32_t CHINT_TIMEOUT_MS       = 4000;   // 4s fallback timeout to Shelly

// Shelly WebSocket Configuration
#ifndef SHELLY_IP
#define SHELLY_IP                         "10.0.0.187"
#endif
constexpr uint16_t SHELLY_WS_PORT         = 80;
constexpr const char* SHELLY_WS_PATH      = "/rpc";
constexpr uint32_t SHELLY_POLL_INTERVAL_MS= 500;   // 500ms stable polling for zero-export control
constexpr uint32_t SHELLY_ENERGY_INTERVAL_MS = 60000; // 60s energy counter interval

// PV Solar Generation Source: OpenDTU via MQTT
constexpr const char* TOPIC_OPENDTU_POWER = "solar/ac/power";
constexpr const char* TOPIC_OPENDTU_YIELD = "solar/ac/yieldtotal";

// Web Interface Options (Set to 0 for headless API-only mode)
#define ENABLE_WEB_DASHBOARD              1

// Grid Power Source Priority Modes:
enum GridPriorityMode : uint8_t {
  GRID_PRI_CHINT_SHELLY = 0, // Default Auto: Chint WS -> Shelly WS fallback
  GRID_PRI_SHELLY_CHINT = 1, // Auto: Shelly WS -> Chint WS fallback
  GRID_PRI_CHINT_ONLY   = 2, // Chint WS Only
  GRID_PRI_SHELLY_ONLY  = 3  // Shelly WS Only
};

#define DEFAULT_GRID_PRIORITY_MODE        GRID_PRI_CHINT_SHELLY

// Web OTA Authentication Password (empty = disabled / no password)
#ifndef OTA_PASSWORD
#define OTA_PASSWORD                      ""
#endif
