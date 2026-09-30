#pragma once

#include <Arduino.h>
#include "../telemetry_source.h"
#include "../../mqtt/mqtt_manager.h"

/**
 * @brief Telemetry source adapter for OpenDTU PV Solar Generation over MQTT.
 */
class OpenDtuMqttSource : public ITelemetrySource {
public:
  OpenDtuMqttSource(MqttManager &mqtt = MqttManager::getInstance());

  const char* getId() const override { return "OPENDTU_MQTT"; }
  const char* getName() const override { return "OpenDTU Solar (MQTT)"; }

  void begin(ITelemetrySink *sink) override;
  void loop() override;

  bool isConnected() const override { return _mqtt.isConnected(); }
  uint32_t getLastMessageTime() const override { return _lastMsgTime; }

  void setBroker(const String &broker, uint16_t port, const String &user, const String &pass);

  bool isPvSource() const override { return true; }

  const char* getHealthKey() const override { return "opendtu"; }
  void populateHealth(JsonObject &obj) const override;

private:
  MqttManager &_mqtt;
  ITelemetrySink *_sink = nullptr;

  uint32_t _lastMsgTime = 0;

  void onMessage(const char *topic, const uint8_t *payload, unsigned int length);
};
