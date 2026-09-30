#pragma once

#include <Arduino.h>
#include <esp_now.h>
#include "../telemetry_source.h"

/**
 * @brief Telemetry source adapter for Sonoff POWCT PV stream over ESP-NOW.
 */
class EspNowPvSource : public ITelemetrySource {
public:
  EspNowPvSource();

  const char* getId() const override { return "ESPNOW_PV"; }
  const char* getName() const override { return "Sonoff POWCT (ESP-NOW)"; }

  void begin(ITelemetrySink *sink) override;
  void loop() override {}

  bool isConnected() const override { return isRecent(10000); }
  uint32_t getLastMessageTime() const override { return _lastMsgTime; }

  void handlePacket(const uint8_t *data, int len);

  bool isPvSource() const override { return true; }

private:
  ITelemetrySink *_sink = nullptr;
  uint32_t _lastMsgTime = 0;
  static EspNowPvSource *s_instance;
  static void onDataRecv(const uint8_t *mac_addr, const uint8_t *data, int data_len);
};
