#include "espnow_pv_source.h"

EspNowPvSource *EspNowPvSource::s_instance = nullptr;

EspNowPvSource::EspNowPvSource() {
  s_instance = this;
}

void EspNowPvSource::onDataRecv(const uint8_t *mac_addr, const uint8_t *data, int data_len) {
  if (s_instance) {
    s_instance->handlePacket(data, data_len);
  }
}

void EspNowPvSource::begin(ITelemetrySink *sink) {
  _sink = sink;
  if (esp_now_init() == ESP_OK) {
    esp_now_register_recv_cb(onDataRecv);
    Serial.println("[ESP-NOW] Initialized PV receiver (listening for 0xD6 packets from Sonoff POWCT)");
  } else {
    Serial.println("[ESP-NOW] Init failed");
  }
}

void EspNowPvSource::handlePacket(const uint8_t *data, int len) {
  if (len == (int)sizeof(PowCtPacket) && data && data[0] == 0xD6) {
    const PowCtPacket *pkt = reinterpret_cast<const PowCtPacket*>(data);

    PvMeasurement meas;
    meas.active_power_w = pkt->active_power_w;
    meas.yield_kwh      = pkt->yield_kwh;

    _lastMsgTime = millis();
    _packetsReceived++;

    if (_sink && meas.isValid()) {
      _sink->reportPvMeasurement(getId(), meas);
    }
  }
}

void EspNowPvSource::populateHealth(JsonObject &obj) const {
  obj["connected"] = isConnected();
  float age = getAgeSeconds();
  obj["age_sec"] = (age >= 0.0f) ? std::round(age * 10.0f) / 10.0f : -1.0f;
  obj["packets_rx"] = (uint32_t)_packetsReceived;
}

