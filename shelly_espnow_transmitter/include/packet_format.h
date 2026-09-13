#pragma once

#include <Arduino.h>

// Matches include/esp_now_receiver.h on the Lilygo DDSU666 Emulator
#define ESPNOW_MAGIC_BYTE 0xD5

enum EspNowPacketType : uint8_t {
  ESPNOW_TYPE_GRID = 0x01,  // Grid power & electricals
  ESPNOW_TYPE_PV   = 0x02   // PV solar inverter yield & power
};

struct __attribute__((packed)) EspNowPowerPacket {
  uint8_t magic;         // 0xD5
  uint8_t packet_type;   // 0x01 = Grid, 0x02 = PV
  float active_power;    // Watts (+ for import, - for export on grid; + for PV)
  float voltage;         // Volts (e.g. 230.0f)
  float current;         // Amps
  float power_factor;    // -1.0 to 1.0 (default 1.0f)
  float frequency;       // Hz (default 50.0f)
  float energy_kwh;      // kWh (optional, set to -1.0f if not provided)
};
