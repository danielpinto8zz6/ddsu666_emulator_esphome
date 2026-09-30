#include "opendtu_mqtt_source.h"
#include <WiFi.h>
#include "config.h"

OpenDtuMqttSource::OpenDtuMqttSource(MqttManager &mqtt)
  : _mqtt(mqtt) {
}

void OpenDtuMqttSource::begin(ITelemetrySink *sink) {
  _sink = sink;
  _mqtt.subscribe(TOPIC_OPENDTU_POWER, [this](const char *topic, const uint8_t *payload, unsigned int length) {
    this->onMessage(topic, payload, length);
  });
  _mqtt.subscribe(TOPIC_OPENDTU_YIELD, [this](const char *topic, const uint8_t *payload, unsigned int length) {
    this->onMessage(topic, payload, length);
  });
}

void OpenDtuMqttSource::setBroker(const String &broker, uint16_t port, const String &user, const String &pass) {
  _mqtt.setBroker(broker, port, user, pass);
}

void OpenDtuMqttSource::loop() {
  // Handled centrally by MqttManager in networkTask
}

void OpenDtuMqttSource::onMessage(const char *topic, const uint8_t *payload, unsigned int length) {
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

void OpenDtuMqttSource::populateHealth(JsonObject &obj) const {
  obj["mqtt_connected"] = isConnected();
  float age = getAgeSeconds();
  obj["age_sec"] = (age >= 0.0f) ? std::round(age * 10.0f) / 10.0f : -1.0f;
}
