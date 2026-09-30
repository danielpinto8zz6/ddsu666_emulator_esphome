#pragma once

#include <Arduino.h>
#include <WebSocketsClient.h>
#include <ArduinoJson.h>
#include "../telemetry_source.h"

/**
 * @brief Telemetry source adapter for Shelly Pro EM over WebSocket.
 */
class ShellyWsSource : public ITelemetrySource {
public:
  ShellyWsSource(const String &ip = "10.0.0.187", uint16_t port = 80, const char *path = "/rpc");

  const char* getId() const override { return "SHELLY_WS"; }
  const char* getName() const override { return "Shelly Pro EM (WebSocket)"; }

  void begin(ITelemetrySink *sink) override;
  void loop() override;

  bool isConnected() const override { return _connected; }
  uint32_t getLastMessageTime() const override { return _lastMsgTime; }

  const String& getIp() const { return _ip; }
  void setIp(const String &ip);

  bool isGridSource() const override { return true; }

  /**
   * @brief Enable periodic polling for energy counters (used during fallback when Chint is offline).
   */
  void setEnergyPollingEnabled(bool enabled) { _energyPollingEnabled = enabled; }

private:
  String _ip;
  uint16_t _port;
  const char *_path;
  WebSocketsClient _ws;
  ITelemetrySink *_sink = nullptr;
  bool _connected = false;
  uint32_t _lastMsgTime = 0;
  uint32_t _lastPollTime = 0;
  uint32_t _lastEnergyPollTime = 0;
  bool _energyPollingEnabled = true;

  void onWsEvent(WStype_t type, uint8_t *payload, size_t length);
  void handleMessage(const uint8_t *payload, size_t length);
};
