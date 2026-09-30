#pragma once

#include <Arduino.h>
#include <WebSocketsServer.h>
#include <ArduinoJson.h>
#include "ddsu666_master.h"

class WebSocketServerManager {
public:
  WebSocketServerManager(uint16_t port = 81);

  void begin();
  void loop();

  // Broadcast fresh meter reading to all connected WebSocket clients
  void broadcastTelemetry(const ChintTelemetry &data, const ModbusMasterStats &stats);

  uint8_t getClientCount() const { return _clientCount; }
  uint32_t getPacketsBroadcast() const { return _packetsBroadcast; }

private:
  WebSocketsServer _ws;
  uint16_t _port;
  uint8_t _clientCount;
  uint32_t _packetsBroadcast;
  uint32_t _lastBroadcastTime;

  char _jsonBuffer[1024];

  void onEvent(uint8_t num, WStype_t type, uint8_t *payload, size_t length);
  void handleClientMessage(uint8_t num, const uint8_t *payload, size_t length);
  void sendShellyRpcResponse(uint8_t num, int reqId, const char *method);
};
