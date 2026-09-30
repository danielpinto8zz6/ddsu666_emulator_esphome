#pragma once

#include <Arduino.h>
#include <WebSocketsServer.h>
#include <ArduinoJson.h>

class TelemetryManager;

/**
 * @brief Real-time WebSocket Dashboard Streaming Server.
 * Streams live telemetry to connected SPA browser clients at high frequency (5-10 Hz)
 * and accepts real-time control commands (priority, filter, reboot).
 */
class DashboardWsServer {
public:
  explicit DashboardWsServer(uint16_t port = 81);

  void begin(TelemetryManager &telemetry);
  void loop();

  /**
   * @brief Broadcast telemetry JSON string to all active browser clients.
   */
  void broadcast(const char *json);

  /**
   * @brief Check whether any browser clients are actively connected.
   */
  bool hasClients() const { return _clientCount > 0; }
  uint8_t getClientCount() const { return _clientCount; }

private:
  uint16_t _port;
  WebSocketsServer _ws;
  TelemetryManager *_telemetry = nullptr;
  uint8_t _clientCount = 0;

  void onEvent(uint8_t num, WStype_t type, uint8_t *payload, size_t length);
  void handleCommand(uint8_t num, const uint8_t *payload, size_t length);
};
