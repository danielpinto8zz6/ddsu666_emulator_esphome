#include "websocket_server_mgr.h"
#include <WiFi.h>

extern DDSU666Master g_ddsu666; // Global master reference for handling RPC queries

WebSocketServerManager::WebSocketServerManager(uint16_t port)
  : _ws(port),
    _port(port),
    _clientCount(0),
    _packetsBroadcast(0),
    _lastBroadcastTime(0) {}

void WebSocketServerManager::begin() {
  _ws.begin();
  _ws.onEvent([this](uint8_t num, WStype_t type, uint8_t *payload, size_t length) {
    this->onEvent(num, type, payload, length);
  });
  Serial.printf("[WebSocket Server] Listening on ws://0.0.0.0:%d/\n", _port);
}

void WebSocketServerManager::loop() {
  _ws.loop();
}

void WebSocketServerManager::onEvent(uint8_t num, WStype_t type, uint8_t *payload, size_t length) {
  switch (type) {
    case WStype_DISCONNECTED:
      if (_clientCount > 0) _clientCount--;
      Serial.printf("[WebSocket Server] Client #%u disconnected. Active clients: %u\n", num, _clientCount);
      break;

    case WStype_CONNECTED: {
      _clientCount++;
      IPAddress ip = _ws.remoteIP(num);
      Serial.printf("[WebSocket Server] Client #%u connected from %s. Active clients: %u\n",
                    num, ip.toString().c_str(), _clientCount);

      // Immediately send welcome packet with current telemetry
      ChintTelemetry data = g_ddsu666.getTelemetry();
      ModbusMasterStats stats = g_ddsu666.getStats();

      snprintf(_jsonBuffer, sizeof(_jsonBuffer),
        "{"
          "\"type\":\"welcome\","
          "\"device\":\"Chint DDSU666 Modbus Master\","
          "\"watts\":%.1f,"
          "\"volts\":%.1f,"
          "\"amps\":%.2f,"
          "\"pf\":%.2f,"
          "\"freq\":%.1f,"
          "\"import_kwh\":%.2f,"
          "\"export_kwh\":%.2f,"
          "\"poll_hz\":%.1f,"
          "\"latency_ms\":%.1f,"
          "\"meter_online\":%s,"
          "\"slave_id\":%u"
        "}",
        data.active_power,
        data.voltage,
        data.current,
        data.power_factor,
        data.frequency,
        data.import_kwh,
        data.export_kwh,
        stats.query_rate_hz,
        stats.response_latency_ms,
        stats.meter_online ? "true" : "false",
        stats.slave_id
      );
      _ws.sendTXT(num, _jsonBuffer);
      break;
    }

    case WStype_TEXT:
      handleClientMessage(num, payload, length);
      break;

    case WStype_BIN:
    case WStype_ERROR:
    default:
      break;
  }
}

void WebSocketServerManager::handleClientMessage(uint8_t num, const uint8_t *payload, size_t length) {
  StaticJsonDocument<256> doc;
  DeserializationError err = deserializeJson(doc, payload, length);
  if (err) return;

  int id = doc["id"] | 0;
  const char *method = doc["method"] | "";

  if (strlen(method) > 0) {
    sendShellyRpcResponse(num, id, method);
  }
}

void WebSocketServerManager::sendShellyRpcResponse(uint8_t num, int reqId, const char *method) {
  ChintTelemetry data = g_ddsu666.getTelemetry();

  if (strcmp(method, "Shelly.GetStatus") == 0 || strcmp(method, "EM1.GetStatus") == 0) {
    // Exact Shelly Pro EM RPC response format
    snprintf(_jsonBuffer, sizeof(_jsonBuffer),
      "{"
        "\"id\":%d,"
        "\"src\":\"chint-ddsu666-master\","
        "\"dst\":\"*\","
        "\"result\":{"
          "\"em1:0\":{"
            "\"id\":0,"
            "\"voltage\":%.1f,"
            "\"current\":%.2f,"
            "\"act_power\":%.1f,"
            "\"aprt_power\":%.1f,"
            "\"pf\":%.2f,"
            "\"freq\":%.1f"
          "},"
          "\"em1data:0\":{"
            "\"id\":0,"
            "\"total_act_energy\":%.1f,"
            "\"total_act_ret_energy\":%.1f"
          "}"
        "}"
      "}",
      reqId,
      data.voltage,
      data.current,
      data.active_power,
      data.apparent_power,
      data.power_factor,
      data.frequency,
      data.import_kwh * 1000.0f, // Shelly reports Wh
      data.export_kwh * 1000.0f
    );
    _ws.sendTXT(num, _jsonBuffer);
  } else if (strcmp(method, "EM1Data.GetStatus") == 0) {
    snprintf(_jsonBuffer, sizeof(_jsonBuffer),
      "{"
        "\"id\":%d,"
        "\"src\":\"chint-ddsu666-master\","
        "\"dst\":\"*\","
        "\"result\":{"
          "\"id\":0,"
          "\"total_act_energy\":%.1f,"
          "\"total_act_ret_energy\":%.1f"
        "}"
      "}",
      reqId,
      data.import_kwh * 1000.0f,
      data.export_kwh * 1000.0f
    );
    _ws.sendTXT(num, _jsonBuffer);
  }
}

void WebSocketServerManager::broadcastTelemetry(const ChintTelemetry &data, const ModbusMasterStats &stats) {
  if (_clientCount == 0) return;

  uint32_t now = millis();
  _lastBroadcastTime = now;

  // 1. Broadcast Clean Stream JSON
  snprintf(_jsonBuffer, sizeof(_jsonBuffer),
    "{"
      "\"type\":\"telemetry\","
      "\"ts\":%lu,"
      "\"watts\":%.1f,"
      "\"volts\":%.1f,"
      "\"amps\":%.2f,"
      "\"pf\":%.2f,"
      "\"freq\":%.1f,"
      "\"import_kwh\":%.2f,"
      "\"export_kwh\":%.2f,"
      "\"poll_hz\":%.1f,"
      "\"latency_ms\":%.1f,"
      "\"meter_online\":%s,"
      "\"slave_id\":%u"
    "}",
    (unsigned long)now,
    data.active_power,
    data.voltage,
    data.current,
    data.power_factor,
    data.frequency,
    data.import_kwh,
    data.export_kwh,
    stats.query_rate_hz,
    stats.response_latency_ms,
    stats.meter_online ? "true" : "false",
    stats.slave_id
  );
  _ws.broadcastTXT(_jsonBuffer);

  // 2. Also Broadcast Shelly Pro EM Compatible NotifyStatus Event
  snprintf(_jsonBuffer, sizeof(_jsonBuffer),
    "{"
      "\"src\":\"chint-ddsu666-master\","
      "\"dst\":\"*\","
      "\"method\":\"NotifyStatus\","
      "\"params\":{"
        "\"ts\":%lu,"
        "\"em1:0\":{"
          "\"id\":0,"
          "\"voltage\":%.1f,"
          "\"current\":%.2f,"
          "\"act_power\":%.1f,"
          "\"aprt_power\":%.1f,"
          "\"pf\":%.2f,"
          "\"freq\":%.1f"
        "},"
        "\"em1data:0\":{"
          "\"id\":0,"
          "\"total_act_energy\":%.1f,"
          "\"total_act_ret_energy\":%.1f"
        "}"
      "}"
    "}",
    (unsigned long)(now / 1000),
    data.voltage,
    data.current,
    data.active_power,
    data.apparent_power,
    data.power_factor,
    data.frequency,
    data.import_kwh * 1000.0f,
    data.export_kwh * 1000.0f
  );
  _ws.broadcastTXT(_jsonBuffer);

  _packetsBroadcast++;
}
