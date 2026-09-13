#pragma once

#include <Arduino.h>
#include <esp_now.h>
#include "config.h"

// Standard DDSU666 Emulator ESP-NOW Packet
// Magic byte 0xD5 distinguishes emulator packets
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

// Diagnostic statistics
struct EspNowStats {
  bool initialized;
  uint32_t packet_count;
  uint32_t last_packet_time;
  uint8_t last_mac[6];
  char last_mac_str[18];
  float last_power_watts;
  uint8_t packet_type;
  int8_t rssi;
};

class ESPNowReceiver {
public:
  static void begin();
  static bool isRecent(uint32_t maxAgeMs = 5000);
  static EspNowStats getStats();
  static uint8_t getChannel();

private:
  static void onDataRecv(const uint8_t *mac_addr, const uint8_t *data, int data_len);
  static EspNowStats s_stats;
  static portMUX_TYPE s_spinlock;
};
