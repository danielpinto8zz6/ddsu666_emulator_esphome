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

class MQTTManager {
public:
  MQTTManager(ModbusRTUServer &modbusServer);

  void begin();
  void loop();

  bool isWiFiConnected() const { return WiFi.status() == WL_CONNECTED; }
  bool isMQTTConnected() { return _mqttClient.connected(); }
  bool isShellyWsConnected() const { return _shellyWsConnected; }
  bool isRS485Enabled() const { return _modbusServer.isEnabled(); }
  uint32_t getLastGridMsgTime() const { return _lastGridMsgTime; }
  uint32_t getLastPvMsgTime() const { return _lastPvMsgTime; }

private:
  ModbusRTUServer &_modbusServer;
  WiFiClient _wifiClient;
  PubSubClient _mqttClient;
  WebSocketsClient _wsClient;

  uint32_t _lastGridMsgTime;
  uint32_t _lastPvMsgTime;
  uint32_t _lastReconnectAttempt;
  uint32_t _lastWiFiConnectAttempt;
  uint32_t _lastShellyPollTime;
  uint32_t _lastShellyEnergyPollTime;
  uint32_t _lastTelemetryPublishTime;
  uint32_t _lastFastPublishTime;

  float _lastPublishedGridWatts;
  float _lastPublishedPvWatts;

  bool _gridWatchdogTriggered;
  bool _pvWatchdogTriggered;
  bool _shellyWsConnected;
  bool _haDiscoveryPublished;

  void connectWiFi();
  void connectMQTT();
  void connectShellyWS();
  void checkWatchdogs();
  void publishHADiscovery();
  void publishTelemetry();

  void onMqttMessage(char *topic, uint8_t *payload, unsigned int length);
  void onWsEvent(WStype_t type, uint8_t *payload, size_t length);

  void handleShellyMessage(const uint8_t *payload, size_t length);
  void handleOpenDtuPower(const uint8_t *payload, unsigned int length);
  void handleOpenDtuYield(const uint8_t *payload, unsigned int length);
};
