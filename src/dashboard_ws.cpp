#include "dashboard_ws.h"
#include "telemetry/telemetry_manager.h"

DashboardWsServer::DashboardWsServer(uint16_t port)
  : _port(port), _ws(port) {}

void DashboardWsServer::begin(TelemetryManager &telemetry) {
  _telemetry = &telemetry;
  _ws.begin();
  _ws.onEvent([this](uint8_t num, WStype_t type, uint8_t *payload, size_t length) {
    this->onEvent(num, type, payload, length);
  });
  Serial.printf("[Dashboard WS] Server listening on port %u\n", _port);
}

void DashboardWsServer::loop() {
  _ws.loop();
}

void DashboardWsServer::broadcast(const char *json) {
  if (_clientCount > 0 && json) {
    _ws.broadcastTXT(json);
  }
}

void DashboardWsServer::onEvent(uint8_t num, WStype_t type, uint8_t *payload, size_t length) {
  switch (type) {
    case WStype_CONNECTED: {
      _clientCount++;
      IPAddress ip = _ws.remoteIP(num);
      Serial.printf("[Dashboard WS] Client #%u connected from %s (Total: %u)\n",
                    num, ip.toString().c_str(), _clientCount);
      break;
    }

    case WStype_DISCONNECTED: {
      if (_clientCount > 0) _clientCount--;
      Serial.printf("[Dashboard WS] Client #%u disconnected (Total: %u)\n", num, _clientCount);
      break;
    }

    case WStype_TEXT: {
      handleCommand(num, payload, length);
      break;
    }

    default:
      break;
  }
}

void DashboardWsServer::handleCommand(uint8_t num, const uint8_t *payload, size_t length) {
  if (!_telemetry || length == 0) return;

  StaticJsonDocument<256> doc;
  DeserializationError err = deserializeJson(doc, payload, length);
  if (err) return;

  const char *action = doc["action"] | "";

  if (strcmp(action, "set_priority") == 0) {
    uint8_t mode = doc["mode"] | 0;
    if (mode <= 3) {
      _telemetry->setGridPriority((GridPriorityMode)mode);
      _ws.sendTXT(num, "{\"status\":\"OK\",\"action\":\"set_priority\"}");
    }
  } else if (strcmp(action, "set_pv_priority") == 0) {
    uint8_t mode = doc["mode"] | 0;
    if (mode <= 3) {
      _telemetry->setPvPriority((PvPriorityMode)mode);
      _ws.sendTXT(num, "{\"status\":\"OK\",\"action\":\"set_pv_priority\"}");
    }
  } else if (strcmp(action, "set_filter") == 0) {
    const char *mode = doc["mode"] | "damped";
    _telemetry->setFilterMode(mode);
    _ws.sendTXT(num, "{\"status\":\"OK\",\"action\":\"set_filter\"}");
  } else if (strcmp(action, "reboot") == 0) {
    _ws.sendTXT(num, "{\"status\":\"OK\",\"action\":\"reboot\",\"msg\":\"Rebooting...\"}");
    delay(200);
    ESP.restart();
  }
}
