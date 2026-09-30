#include "opendtu_mqtt_source.h"
#include <WiFi.h>
#include "config.h"

OpenDtuMqttSource::OpenDtuMqttSource(const String &broker, uint16_t port,
                                     const String &user, const String &pass)
  : _broker(broker), _port(port), _user(user), _pass(pass), _mqtt(_wifiClient) {
}

void OpenDtuMqttSource::begin(ITelemetrySink *sink) {
  _sink = sink;
  _mqtt.setServer(_broker.c_str(), _port);
  _mqtt.setBufferSize(768);
  _mqtt.setCallback([this](char *topic, uint8_t *payload, unsigned int length) {
    this->onMessage(topic, payload, length);
  });
}

void OpenDtuMqttSource::setBroker(const String &broker, uint16_t port, const String &user, const String &pass) {
  _broker = broker;
  _port = port;
  _user = user;
  _pass = pass;
  _mqtt.setServer(_broker.c_str(), _port);
  if (_mqtt.connected()) {
    _mqtt.disconnect();
  }
}

void OpenDtuMqttSource::loop() {
  if (!_mqtt.connected()) {
    connect();
  } else {
    _mqtt.loop();
  }
}

void OpenDtuMqttSource::connect() {
  if (WiFi.status() != WL_CONNECTED) return;

  uint32_t now = millis();
  if (now - _lastReconnectAttempt < 5000) return; // Retry every 5s
  _lastReconnectAttempt = now;

  Serial.print("[OpenDTU MQTT] Connecting to broker...");
  bool ok;
  if (_user.length() > 0) {
    ok = _mqtt.connect("DDSU666_OpenDTU_Bridge", _user.c_str(), _pass.c_str());
  } else {
    ok = _mqtt.connect("DDSU666_OpenDTU_Bridge");
  }

  if (ok) {
    Serial.println(" connected!");
    _mqtt.subscribe(TOPIC_OPENDTU_POWER);
    _mqtt.subscribe(TOPIC_OPENDTU_YIELD);
  } else {
    Serial.printf(" failed, rc=%d. Retrying in 5s\n", _mqtt.state());
  }
}

void OpenDtuMqttSource::onMessage(char *topic, uint8_t *payload, unsigned int length) {
  char msg[64];
  if (length >= sizeof(msg)) return;
  memcpy(msg, payload, length);
  msg[length] = '\0';

  PvMeasurement meas;

  if (strcmp(topic, TOPIC_OPENDTU_POWER) == 0) {
    float watts = atof(msg);
    if (!std::isnan(watts) && watts >= 0.0f && watts < 25000.0f) {
      meas.active_power_w = watts;
      _lastMsgTime = millis();
      if (_sink) {
        _sink->reportPvMeasurement(getId(), meas);
      }
    }
  } else if (strcmp(topic, TOPIC_OPENDTU_YIELD) == 0) {
    float yield_kwh = atof(msg);
    if (!std::isnan(yield_kwh) && yield_kwh >= 0.0f) {
      meas.yield_kwh = yield_kwh;
      _lastMsgTime = millis();
      if (_sink) {
        _sink->reportPvMeasurement(getId(), meas);
      }
    }
  }
}
