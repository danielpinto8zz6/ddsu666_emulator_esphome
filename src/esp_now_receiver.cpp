#include "esp_now_receiver.h"
#include "meter_data.h"
#include <WiFi.h>
#include <esp_wifi.h>
#include <cmath>

EspNowStats ESPNowReceiver::s_stats = { false, 0, 0, {0}, "None", 0.0f, 0, 0 };
portMUX_TYPE ESPNowReceiver::s_spinlock = portMUX_INITIALIZER_UNLOCKED;

static volatile int8_t s_captured_rssi = 0;
static volatile uint8_t s_tracked_mac[6] = {0};

static void IRAM_ATTR promis_sniffer_cb(void *buf, wifi_promiscuous_pkt_type_t type) {
  if (type != WIFI_PKT_MGMT) return;
  const wifi_promiscuous_pkt_t *pkt = (const wifi_promiscuous_pkt_t *)buf;
  if (pkt->rx_ctrl.sig_len < 24) return;
  const uint8_t *payload = pkt->payload;

  // Match transmitter MAC dynamically once learned from valid packet
  if (s_tracked_mac[0] != 0 || s_tracked_mac[1] != 0) {
    if (payload[10] == s_tracked_mac[0] && payload[11] == s_tracked_mac[1] &&
        payload[12] == s_tracked_mac[2] && payload[13] == s_tracked_mac[3] &&
        payload[14] == s_tracked_mac[4] && payload[15] == s_tracked_mac[5]) {
      s_captured_rssi = pkt->rx_ctrl.rssi;
    }
  } else {
    // Fallback before first packet: capture 802.11 action frames (0xD0)
    if (payload[0] == 0xD0) {
      s_captured_rssi = pkt->rx_ctrl.rssi;
    }
  }
}

void ESPNowReceiver::begin() {
  if (s_stats.initialized) return;

  // Wi-Fi STA mode must be initialized prior to esp_now_init()
  if (esp_now_init() != ESP_OK) {
    Serial.println("[ESP-NOW] Error: Initialization failed!");
    return;
  }

  esp_now_register_recv_cb(onDataRecv);

  // Enable promiscuous sniffer for direct RF RSSI on management/action frames
  wifi_promiscuous_filter_t filter = { .filter_mask = WIFI_PROMIS_FILTER_MASK_MGMT };
  esp_wifi_set_promiscuous_filter(&filter);
  esp_wifi_set_promiscuous_rx_cb(promis_sniffer_cb);
  esp_wifi_set_promiscuous(true);

  portENTER_CRITICAL(&s_spinlock);
  s_stats.initialized = true;
  portEXIT_CRITICAL(&s_spinlock);

  Serial.printf("[ESP-NOW] Receiver ready! Board MAC: %s on Wi-Fi Channel %d\n",
                WiFi.macAddress().c_str(), WiFi.channel());
}

bool ESPNowReceiver::isRecent(uint32_t maxAgeMs) {
  portENTER_CRITICAL(&s_spinlock);
  bool recent = (s_stats.last_packet_time != 0) && (millis() - s_stats.last_packet_time < maxAgeMs);
  portEXIT_CRITICAL(&s_spinlock);
  return recent;
}

EspNowStats ESPNowReceiver::getStats() {
  EspNowStats copy;
  portENTER_CRITICAL(&s_spinlock);
  copy = s_stats;
  portEXIT_CRITICAL(&s_spinlock);
  return copy;
}

uint8_t ESPNowReceiver::getChannel() {
  return WiFi.channel();
}

void ESPNowReceiver::onDataRecv(const uint8_t *mac_addr, const uint8_t *data, int data_len) {
  if (data == nullptr || data_len <= 0) return;

  if (mac_addr) {
    s_tracked_mac[0] = mac_addr[0];
    s_tracked_mac[1] = mac_addr[1];
    s_tracked_mac[2] = mac_addr[2];
    s_tracked_mac[3] = mac_addr[3];
    s_tracked_mac[4] = mac_addr[4];
    s_tracked_mac[5] = mac_addr[5];

    if (!esp_now_is_peer_exist(mac_addr)) {
      esp_now_peer_info_t peer = {};
      memcpy(peer.peer_addr, mac_addr, 6);
      peer.channel = 0;
      peer.ifidx = WIFI_IF_STA;
      peer.encrypt = false;
      esp_now_add_peer(&peer);
    }
  }

  float grid_watts = 0.0f;
  float pv_watts = 0.0f;
  float voltage = 230.0f;
  float current = 0.0f;
  float pf = 1.0f;
  float freq = 50.0f;
  float energy_kwh = -1.0f;
  uint8_t type = ESPNOW_TYPE_GRID;

  if (data_len == sizeof(EspNowPowerPacket)) {
    const EspNowPowerPacket *pkt = reinterpret_cast<const EspNowPowerPacket *>(data);
    if (pkt->magic != ESPNOW_MAGIC_BYTE) return;

    type = pkt->packet_type;
    if (type == ESPNOW_TYPE_GRID) {
      grid_watts = pkt->active_power;
      if (pkt->voltage >= 180.0f && pkt->voltage <= 280.0f) voltage = pkt->voltage;
      if (pkt->power_factor >= -1.0f && pkt->power_factor <= 1.0f && pkt->power_factor != 0.0f) pf = pkt->power_factor;
      if (pkt->frequency >= 45.0f && pkt->frequency <= 55.0f) freq = pkt->frequency;
      current = (pkt->current > 0.0f) ? pkt->current : (std::fabs(grid_watts) / voltage);
      energy_kwh = pkt->energy_kwh;
    } else if (type == ESPNOW_TYPE_PV) {
      pv_watts = pkt->active_power;
      energy_kwh = pkt->energy_kwh;
    }
  } else if (data_len == 4) {
    // Ultra-simple 4-byte raw float: Grid Watts
    memcpy(&grid_watts, data, 4);
    current = std::fabs(grid_watts) / voltage;
  } else if (data_len == 8) {
    // 8-byte dual raw float: [0] = Grid Watts, [1] = PV Watts
    memcpy(&grid_watts, data, 4);
    memcpy(&pv_watts, data + 4, 4);
    current = std::fabs(grid_watts) / voltage;
  } else {
    // Unrecognized format
    return;
  }

  // Update DDSU666 telemetry registers
#if (GRID_SOURCE_PRIORITY != 0)
  if (type == ESPNOW_TYPE_GRID || data_len == 4 || data_len == 8) {
    // Invert sign for Hoymiles: Grid import is negative kW, export is positive kW
    float p_kw = (grid_watts / 1000.0f) * -1.0f;
    float s_kw = std::fabs(p_kw);
    MeterState::updateGridRealtime(voltage, current, p_kw, s_kw, 0.0f, pf, freq);
    if (energy_kwh >= 0.0f) {
      MeterState::updateGridEnergy(energy_kwh, -1.0f);
    }
  }
#endif

  if (type == ESPNOW_TYPE_PV || data_len == 8) {
    MeterState::updatePvPower(pv_watts);
    if (energy_kwh >= 0.0f) {
      MeterState::updatePvYield(energy_kwh);
    }
  }

  // Update diagnostic stats
  portENTER_CRITICAL(&s_spinlock);
  s_stats.packet_count++;
  s_stats.last_packet_time = millis();
  s_stats.packet_type = type;
  s_stats.last_power_watts = (type == ESPNOW_TYPE_PV) ? pv_watts : grid_watts;
  if (s_captured_rssi != 0) {
    if (s_stats.rssi == 0) {
      s_stats.rssi = s_captured_rssi;
    } else {
      // Exponential Moving Average filter: 80% previous + 20% new
      s_stats.rssi = (int8_t)std::round(0.8f * s_stats.rssi + 0.2f * s_captured_rssi);
    }
  }
  if (mac_addr) {
    memcpy(s_stats.last_mac, mac_addr, 6);
    snprintf(s_stats.last_mac_str, sizeof(s_stats.last_mac_str),
             "%02X:%02X:%02X:%02X:%02X:%02X",
             mac_addr[0], mac_addr[1], mac_addr[2],
             mac_addr[3], mac_addr[4], mac_addr[5]);
  }
  portEXIT_CRITICAL(&s_spinlock);
}
