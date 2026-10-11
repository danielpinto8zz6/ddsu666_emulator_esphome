#pragma once

#include <Arduino.h>
#include <WebSocketsClient.h>
#include <ArduinoJson.h>
#include "../telemetry_source.h"

/**
 * @brief Telemetry source adapter for Seeed Studio XIAO ESP32-S3 Chint Master over WebSocket.
 */
class ChintWsSource : public ITelemetrySource {
public:
  ChintWsSource(const String &ip = "10.0.0.110", uint16_t port = 81, const char *path = "/");

  const char* getId() const override { return "CHINT_WS"; }
  const char* getName() const override { return "Chint DDSU666 Master (WebSocket)"; }

  void begin(ITelemetrySink *sink) override;
  void loop() override;

  bool isConnected() const override { return _connected; }
  uint32_t getLastMessageTime() const override { return _lastMsgTime; }

  const String& getIp() const { return _ip; }
  void setIp(const String &ip);

  bool isGridSource() const override { return true; }
  bool isPvSource() const override { return true; }
  bool isPvRecent(uint32_t timeoutMs = 5000) const override {
    return (_lastPvMsgTime != 0) && (millis() - _lastPvMsgTime <= timeoutMs);
  }
  uint32_t getLastPvMessageTime() const override { return _lastPvMsgTime; }

  const char* getHealthKey() const override { return "chint"; }
  void populateHealth(JsonObject &obj) const override;

private:
  String _ip;
  uint16_t _port;
  const char *_path;
  WebSocketsClient _ws;
  ITelemetrySink *_sink = nullptr;
  bool _connected = false;
  uint32_t _lastMsgTime = 0;
  uint32_t _lastPvMsgTime = 0;

  void onWsEvent(WStype_t type, uint8_t *payload, size_t length);
  void handleMessage(const uint8_t *payload, size_t length);
};
