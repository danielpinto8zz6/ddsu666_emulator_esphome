#pragma once

#include <Arduino.h>
#include <WiFiClient.h>
#include <PubSubClient.h>
#include "../telemetry_source.h"

/**
 * @brief Telemetry source adapter for OpenDTU PV Solar Generation over MQTT.
 */
class OpenDtuMqttSource : public ITelemetrySource {
public:
  OpenDtuMqttSource(const String &broker = "10.0.0.1", uint16_t port = 1883,
                    const String &user = "", const String &pass = "");

  const char* getId() const override { return "OPENDTU_MQTT"; }
  const char* getName() const override { return "OpenDTU Solar (MQTT)"; }

  void begin(ITelemetrySink *sink) override;
  void loop() override;

  bool isConnected() const override { return const_cast<PubSubClient&>(_mqtt).connected(); }
  uint32_t getLastMessageTime() const override { return _lastMsgTime; }

  void setBroker(const String &broker, uint16_t port, const String &user, const String &pass);

private:
  String _broker;
  uint16_t _port;
  String _user;
  String _pass;

  WiFiClient _wifiClient;
  PubSubClient _mqtt;
  ITelemetrySink *_sink = nullptr;

  uint32_t _lastMsgTime = 0;
  uint32_t _lastReconnectAttempt = 0;

  void connect();
  void onMessage(char *topic, uint8_t *payload, unsigned int length);
};
