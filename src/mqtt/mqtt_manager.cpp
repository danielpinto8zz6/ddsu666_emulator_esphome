#include "mqtt_manager.h"

MqttManager& MqttManager::getInstance() {
  static MqttManager instance;
  return instance;
}

MqttManager::MqttManager()
  : _broker("10.0.0.1"), _port(1883), _user(""), _pass(""), _mqtt(_wifiClient) {
}

void MqttManager::begin(const String &broker, uint16_t port, const String &user, const String &pass) {
  _broker = broker;
  _port = port;
  _user = user;
  _pass = pass;

  _mqtt.setServer(_broker.c_str(), _port);
  _mqtt.setBufferSize(1280); // Ensure discovery and status JSON packets fit comfortably
  _mqtt.setKeepAlive(15);
  _mqtt.setCallback([this](char *topic, uint8_t *payload, unsigned int length) {
    this->onIncomingMessage(topic, payload, length);
  });
}

void MqttManager::setBroker(const String &broker, uint16_t port, const String &user, const String &pass) {
  _broker = broker;
  _port = port;
  _user = user;
  _pass = pass;
  _mqtt.setServer(_broker.c_str(), _port);
  if (_mqtt.connected()) {
    _mqtt.disconnect();
  }
}

void MqttManager::loop() {
  if (!_mqtt.connected()) {
    if (_wasConnected) {
      _wasConnected = false;
      Serial.println("[MQTT Manager] Connection lost");
      for (const auto &cb : _connCallbacks) {
        if (cb) cb(false);
      }
    }
    connect();
  } else {
    _mqtt.loop();
  }
}

bool MqttManager::isConnected() {
  return _mqtt.connected();
}

void MqttManager::connect() {
  if (WiFi.status() != WL_CONNECTED) return;
  if (_broker.length() == 0 || _broker == "0.0.0.0") return;

  uint32_t now = millis();
  if (now - _lastReconnectAttempt < 5000) return; // Retry every 5s
  _lastReconnectAttempt = now;

  Serial.printf("[MQTT Manager] Connecting to %s:%u...", _broker.c_str(), _port);

  // Set LWT (Last Will and Testament) so Home Assistant immediately detects offline state
  const char *clientId = "DDSU666_Emulator";
  const char *willTopic = "ddsu666_emulator/status";
  uint8_t willQos = 1;
  bool willRetain = true;
  const char *willMessage = "offline";

  bool ok = false;
  if (_user.length() > 0) {
    ok = _mqtt.connect(clientId, _user.c_str(), _pass.c_str(), willTopic, willQos, willRetain, willMessage);
  } else {
    ok = _mqtt.connect(clientId, willTopic, willQos, willRetain, willMessage);
  }

  if (ok) {
    Serial.println(" Connected!");
    _wasConnected = true;

    // Publish online status
    _mqtt.publish(willTopic, "online", true);

    // Resubscribe to all registered topics
    for (const auto &sub : _subscriptions) {
      if (sub.topic.length() > 0) {
        _mqtt.subscribe(sub.topic.c_str());
        Serial.printf("[MQTT Manager] Subscribed to %s\n", sub.topic.c_str());
      }
    }

    // Notify listeners (e.g. HaDiscovery to send configs)
    for (const auto &cb : _connCallbacks) {
      if (cb) cb(true);
    }
  } else {
    Serial.printf(" Failed, rc=%d. Retrying in 5s\n", _mqtt.state());
  }
}

bool MqttManager::publish(const char *topic, const char *payload, bool retained) {
  if (!_mqtt.connected()) return false;
  return _mqtt.publish(topic, payload, retained);
}

bool MqttManager::subscribe(const String &topic, MqttTopicCallback cb) {
  // Check if topic is already subscribed
  for (auto &sub : _subscriptions) {
    if (sub.topic == topic) {
      sub.callback = cb;
      return true;
    }
  }
  _subscriptions.push_back({topic, cb});
  if (_mqtt.connected()) {
    return _mqtt.subscribe(topic.c_str());
  }
  return true;
}

void MqttManager::onIncomingMessage(char *topic, uint8_t *payload, unsigned int length) {
  _lastMsgTime = millis();
  for (const auto &sub : _subscriptions) {
    if (sub.topic == topic && sub.callback) {
      sub.callback(topic, payload, length);
    }
  }
}
