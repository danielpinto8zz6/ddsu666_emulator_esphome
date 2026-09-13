/*
 * DDSU666 Emulator - ESP-NOW Test Transmitter Example
 * 
 * Flash this sketch onto any spare ESP32 or ESP8266 to simulate
 * a remote CT clamp, energy meter, or power sensor sending live grid watts
 * directly to the DDSU666 Emulator over ESP-NOW.
 */

#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>

// 1. SET THE TARGET WI-FI CHANNEL
// Must match the channel your home Wi-Fi router uses (check http://10.0.0.110/health)
#define ESPNOW_CHANNEL 6

// 2. CHOOSE TARGET MAC ADDRESS
// Use {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF} for Broadcast (no pairing needed!)
// Or replace with the emulator's exact MAC address
uint8_t broadcastAddress[] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };

// Structured Packet Format (Matches emulator's include/esp_now_receiver.h)
#define ESPNOW_MAGIC_BYTE 0xD5

struct __attribute__((packed)) EspNowPowerPacket {
  uint8_t magic;         // 0xD5
  uint8_t packet_type;   // 0x01 = Grid Power
  float active_power;    // Watts (+ = importing from grid, - = exporting to grid)
  float voltage;         // Volts (e.g. 230.0)
  float current;         // Amps
  float power_factor;    // -1.0 to 1.0 (default 1.0)
  float frequency;       // Hz (default 50.0)
  float energy_kwh;      // kWh (optional, -1.0 if not measured)
};

EspNowPowerPacket packet;
esp_now_peer_info_t peerInfo;

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n[ESP-NOW Transmitter] Starting...");

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  // Lock to the same Wi-Fi channel as the emulator / router
  // On ESP32, this sets the radio channel in STA mode:
  #if defined(ESP32)
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_channel(ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE);
  esp_wifi_set_promiscuous(false);
  #endif

  if (esp_now_init() != ESP_OK) {
    Serial.println("[ESP-NOW] Init failed!");
    return;
  }

  // Register broadcast peer
  memcpy(peerInfo.peer_addr, broadcastAddress, 6);
  peerInfo.channel = ESPNOW_CHANNEL;
  peerInfo.encrypt = false;
  
  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("[ESP-NOW] Failed to add peer!");
    return;
  }

  Serial.println("[ESP-NOW] Ready! Sending test grid power packets...");
}

void loop() {
  // Simulate grid power varying between -200W (export) and +800W (import)
  static float testWatts = 250.0f;
  testWatts += 25.0f;
  if (testWatts > 900.0f) testWatts = -300.0f;

  packet.magic = ESPNOW_MAGIC_BYTE;
  packet.packet_type = 0x01; // Grid
  packet.active_power = testWatts;
  packet.voltage = 232.5f;
  packet.current = abs(testWatts) / 232.5f;
  packet.power_factor = 0.98f;
  packet.frequency = 50.0f;
  packet.energy_kwh = -1.0f;

  esp_err_t result = esp_now_send(broadcastAddress, (uint8_t *)&packet, sizeof(packet));

  if (result == ESP_OK) {
    Serial.printf("Sent Grid Power: %.1f W\n", testWatts);
  } else {
    Serial.println("Send Error!");
  }

  delay(500); // Transmit every 500ms
}
