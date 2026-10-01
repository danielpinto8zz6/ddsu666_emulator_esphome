#pragma once

#include <Arduino.h>
#include <WebSocketsServer.h>
#include <ArduinoJson.h>
#include "interfaces.h"

/**
 * @brief High-frequency WebSocket Telemetry Streaming & RPC Manager.
 * Implements ITelemetryPublisher interface (LSP / DIP).
 * Emits clean JSON telemetry and emulates Shelly Pro EM RPC NotifyStatus.
 */
class WebSocketServerManager : public ITelemetryPublisher {
public:
  explicit WebSocketServerManager(IChintMeterReader &reader, uint16_t port = 81);
  ~WebSocketServerManager() override = default;

  void begin() override;
  void loop() override;

  // Broadcast fresh meter reading to all connected WebSocket clients
  void broadcastTelemetry(const ChintTelemetry &data, const ModbusMasterStats &stats) override;

  bool hasClients() const override;
  uint8_t getClientCount() const override;
  uint32_t getPacketsBroadcast() const override { return _packetsBroadcast; }
  std::vector<String> getClientIPs() const override;

private:
  IChintMeterReader &_reader;
  WebSocketsServer _ws;
  uint16_t _port;
  uint8_t _clientCount;
  uint32_t _packetsBroadcast;
  uint32_t _lastBroadcastTime;

  char _jsonBuffer[1536];

  void onEvent(uint8_t num, WStype_t type, uint8_t *payload, size_t length);
  void handleClientMessage(uint8_t num, const uint8_t *payload, size_t length);
  void sendWelcomePacket(uint8_t num);
  void sendShellyRpcResponse(uint8_t num, int reqId, const char *method);
};
