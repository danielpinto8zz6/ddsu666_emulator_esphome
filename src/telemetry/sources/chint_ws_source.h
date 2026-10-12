#pragma once

#include <Arduino.h>
#include <WebSocketsClient.h>
#include <ArduinoJson.h>
#include <ESPmDNS.h>
#include "../telemetry_source.h"

/**
 * @brief Telemetry source adapter for Chint Master transmitter over WebSocket.
 * Supports static IP and dynamic mDNS discovery (chint-reader.local).
 */
class ChintWsSource : public ITelemetrySource {
public:
  ChintWsSource(const String &ip = "chint-reader.local", uint16_t port = 81, const char *path = "/");

  const char* getId() const override { return "CHINT_WS"; }
  const char* getName() const override { return "Chint DDSU666 Master (WebSocket)"; }

  void begin(ITelemetrySink *sink) override;
  void loop() override;

  bool isConnected() const override { return _connected; }
  uint32_t getLastMessageTime() const override { return _lastMsgTime; }

  const String& getIp() const { return _activeIp.isEmpty() ? _configuredHost : _activeIp; }
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
  String _configuredHost;   // Configured IP or mDNS hostname (e.g. "10.0.0.110" or "chint-reader.local")
  String _activeIp;         // Resolved / Active IP currently connected
  uint16_t _port;
  const char *_path;
  WebSocketsClient _ws;
  ITelemetrySink *_sink = nullptr;
  bool _connected = false;
  uint32_t _lastMsgTime = 0;
  uint32_t _lastPvMsgTime = 0;
  uint32_t _lastMdnsAttempt = 0;

  void connect();
  bool resolveHost(const String &host, String &outIp);
  void onWsEvent(WStype_t type, uint8_t *payload, size_t length);
  void handleMessage(const uint8_t *payload, size_t length);
};
