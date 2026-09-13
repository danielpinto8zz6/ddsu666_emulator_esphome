#pragma once

#include <Arduino.h>

// Wi-Fi Credentials
// IMPORTANT: The Lolin C3 Mini MUST connect to the same Wi-Fi network as the Lilygo
// so both ESP32 radios automatically lock onto the exact same Wi-Fi channel!
#define WIFI_SSID             "AccessPoint"
#define WIFI_PASSWORD         "65809240"

// Shelly Pro EM Configuration
#define SHELLY_IP             "10.0.0.187"
#define SHELLY_WS_PORT        80
#define SHELLY_WS_PATH        "/rpc"
#define SHELLY_POLL_MS        200     // 200ms polling for ultra-fast real-time telemetry (zero-export)
#define SHELLY_ENERGY_POLL_MS 60000   // 60s polling for energy accumulators

// Target Lilygo T-CAN485 ESP-NOW Configuration
// Set USE_BROADCAST to true to broadcast without pairing (recommended: 100% transmission success)
// Set USE_BROADCAST to false to unicast with hardware 802.11 ACK
#define USE_BROADCAST         true
static const uint8_t LILYGO_MAC[6] = { 0x80, 0xF3, 0xDA, 0xD8, 0x41, 0xD0 };
