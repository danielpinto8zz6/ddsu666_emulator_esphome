#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <WebSocketsClient.h>
#include <ArduinoJson.h>
#include "config.h"
#include "secrets.h"
#include "meter_data.h"
#include "modbus_rtu.h"
#include "grid_filter.h"
#include <Preferences.h>

// Ring buffer of recent grid-source transition events (shown on dashboard)
struct SourceEvent {
  uint32_t timestamp_s;  // millis()/1000 at time of transition
  char from[12];         // Source name before transition
  char to[12];           // Source name after transition
};
constexpr uint8_t SOURCE_EVENT_HISTORY = 5;

class MQTTManager {
public:
  MQTTManager(ModbusRTUServer &modbusServer);

  void begin();
  void loop();

  bool isWiFiConnected() const { return WiFi.status() == WL_CONNECTED; }
  bool isMQTTConnected() { return _mqttClient.connected(); }
  bool isShellyWsConnected() const { return _shellyWsConnected; }
  bool isChintWsConnected() const { return _chintWsConnected; }
  bool isChintRecent(uint32_t maxAgeMs = CHINT_TIMEOUT_MS) const {
    return (_lastChintMsgTime != 0) && (millis() - _lastChintMsgTime < maxAgeMs);
  }
  bool isRS485Enabled() const { return _modbusServer.isEnabled(); }
  uint32_t getLastGridMsgTime() const { return _lastGridMsgTime; }
  uint32_t getLastChintMsgTime() const { return _lastChintMsgTime; }
  uint32_t getLastShellyMsgTime() const { return _lastShellyMsgTime; }
  uint32_t getLastPvMsgTime() const { return _lastPvMsgTime; }
  bool isShellyRecent(uint32_t maxAgeMs = 5000) const {
    return (_lastShellyMsgTime != 0) && (millis() - _lastShellyMsgTime < maxAgeMs);
  }
  const char* getActiveGridSource() const;

  // Runtime config accessors (for /health reporting)
  const String& getChintIp() const { return _chintIp; }
  const String& getShellyIp() const { return _shellyIp; }
  const String& getMqttBroker() const { return _mqttBroker; }
  uint16_t getMqttPort() const { return _mqttPort; }

  // Source-transition event history (ring buffer, oldest->newest)
  const SourceEvent* getSourceEvents() const { return _sourceEvents; }
  uint8_t getSourceEventCount() const { return _sourceEventCount; }
  uint8_t getSourceEventHead() const { return _sourceEventHead; }

  GridPriorityMode getGridPriority() const { return _gridPriority; }
  void setGridPriority(GridPriorityMode mode);
  const char* getGridPriorityDescription() const;
  bool canChintUpdateGrid() const;
  bool canShellyUpdateGrid() const;

private:
  static MQTTManager* s_instance;

  ModbusRTUServer &_modbusServer;
  WiFiClient _wifiClient;
  PubSubClient _mqttClient;
  WebSocketsClient _wsShelly;
  WebSocketsClient _wsChint;

  GridPriorityMode _gridPriority;

  uint32_t _lastGridMsgTime;
  uint32_t _lastChintMsgTime;
  uint32_t _lastShellyMsgTime;
  uint32_t _lastPvMsgTime;
  uint32_t _lastReconnectAttempt;
  uint32_t _lastWiFiConnectAttempt;
  uint32_t _lastShellyPollTime;
  uint32_t _lastShellyEnergyPollTime;

  bool _gridWatchdogTriggered;
  bool _pvWatchdogTriggered;
  bool _shellyWsConnected;
  bool _chintWsConnected;

  // Multi-Zone Precision Damping Filter for Zero-Export
  GridFilter _gridFilter;

  // Source-transition ring buffer (#5)
  SourceEvent _sourceEvents[SOURCE_EVENT_HISTORY];
  uint8_t _sourceEventHead  = 0;
  uint8_t _sourceEventCount = 0;
  char _lastKnownSource[12];

  // Runtime-configurable settings (loaded from NVS at begin(), saved via /api/config)
  String _chintIp;
  String _shellyIp;
  String _mqttBroker;
  uint16_t _mqttPort = MQTT_PORT;
  String _mqttUser;
  String _mqttPass;
  String _wifiSsid;
  String _wifiPassword;

  void connectWiFi();
  void connectMQTT();
  void connectShellyWS();
  void connectChintWS();
  void checkWatchdogs();
  void checkSourceTransition();

  void onMqttMessage(char *topic, uint8_t *payload, unsigned int length);
  void onShellyWsEvent(WStype_t type, uint8_t *payload, size_t length);
  void onChintWsEvent(WStype_t type, uint8_t *payload, size_t length);

  void handleShellyMessage(const uint8_t *payload, size_t length);
  void handleChintMessage(const uint8_t *payload, size_t length);
  void handleOpenDtuPower(const uint8_t *payload, unsigned int length);
  void handleOpenDtuYield(const uint8_t *payload, unsigned int length);
  void handleEspNowPv(float watts, float yield_kwh);
};
