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
  if (len == 9 && data && data[0] == 0xD6) {
    float watts = 0.0f;
    float yield_kwh = 0.0f;
    memcpy(&watts, data + 1, 4);
    memcpy(&yield_kwh, data + 5, 4);

    PvMeasurement meas;
    meas.active_power_w = watts;
    meas.yield_kwh = yield_kwh;

    _lastMsgTime = millis();
    if (_sink && meas.isValid()) {
      _sink->reportPvMeasurement(getId(), meas);
    }
  }
}
