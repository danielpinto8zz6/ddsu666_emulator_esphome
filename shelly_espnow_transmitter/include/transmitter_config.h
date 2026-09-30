#pragma once

#include <Arduino.h>

#if __has_include("secrets.h")
#include "secrets.h"
#endif

// Hardware Pinout Definitions
#ifndef STATUS_LED_PIN
#if defined(BOARD_XIAO_ESP32S3)
#define STATUS_LED_PIN        21      // Seeed Studio XIAO ESP32-S3 onboard user LED
#elif defined(BOARD_XIAO_ESP32C3)
#define STATUS_LED_PIN        10      // Seeed Studio XIAO ESP32-C3
#else
#define STATUS_LED_PIN        8       // ESP32-C3 SuperMini onboard blue LED (Active LOW)
#endif
#endif

// Wi-Fi Credentials
// IMPORTANT: The ESP32-C3 MUST connect to the same Wi-Fi network as the Lilygo
// so both ESP32 radios automatically lock onto the exact same Wi-Fi channel!
#ifndef WIFI_SSID
#define WIFI_SSID             "AccessPoint"
#endif
#ifndef WIFI_PASSWORD
#define WIFI_PASSWORD         "65809240"
#endif

// Shelly Pro EM Configuration
#ifndef SHELLY_IP
#define SHELLY_IP             "10.0.0.187"
#endif
#define SHELLY_WS_PORT        80
#define SHELLY_WS_PATH        "/rpc"
#define SHELLY_POLL_MS        200     // 500ms ultra-fast polling for zero-export control
#define SHELLY_ENERGY_POLL_MS 60000   // 60s polling for energy accumulators

// Target Receiver XIAO ESP32-S3 ESP-NOW Configuration
// Set USE_BROADCAST to false for direct unicast to receiver MAC
#define USE_BROADCAST         false
static const uint8_t LILYGO_MAC[6] = { 0xE0, 0x72, 0xA1, 0xF9, 0x89, 0xD8 };



// Burst transmission redundancy for broadcast mode (sends 2 consecutive frames spaced 2.5ms)
// Guarantees zero packet loss over distance even with RF interference
#ifndef ESPNOW_BURST_REDUNDANCY
#define ESPNOW_BURST_REDUNDANCY true
#endif


// Web OTA Authentication Password (empty = disabled / no password)
#ifndef OTA_PASSWORD
#define OTA_PASSWORD          ""
#endif
