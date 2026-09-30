#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <PubSubClient.h>
#include <functional>
#include <vector>

typedef std::function<void(const char *topic, const uint8_t *payload, unsigned int length)> MqttTopicCallback;
typedef std::function<void(bool connected)> MqttConnectionCallback;

struct TopicSubscription {
  String topic;
  MqttTopicCallback callback;
};

/**
 * @brief Centralized MQTT Communication Service (SOLID: SRP & DIP).
 *
 * Manages the underlying TCP socket and PubSubClient instance for the emulator ecosystem,
 * avoiding duplicate socket consumption and reconnection logic across different modules.
 */
class MqttManager {
public:
  static MqttManager& getInstance();

  void begin(const String &broker = "10.0.0.1", uint16_t port = 1883,
             const String &user = "", const String &pass = "");
  void setBroker(const String &broker, uint16_t port, const String &user = "", const String &pass = "");
  void loop();

  bool isConnected();
  bool publish(const char *topic, const char *payload, bool retained = false);
  bool subscribe(const String &topic, MqttTopicCallback cb = nullptr);

  void onConnectionChange(MqttConnectionCallback cb) {
    _connCallbacks.push_back(cb);
  }

  uint32_t getLastMessageTime() const { return _lastMsgTime; }
  PubSubClient& getClient() { return _mqtt; }

private:
  MqttManager();
  ~MqttManager() = default;

  String _broker;
  uint16_t _port;
  String _user;
  String _pass;

  WiFiClient _wifiClient;
  PubSubClient _mqtt;

  uint32_t _lastReconnectAttempt = 0;
  uint32_t _lastMsgTime = 0;
  bool _wasConnected = false;

  std::vector<TopicSubscription> _subscriptions;
  std::vector<MqttConnectionCallback> _connCallbacks;

  void connect();
  void onIncomingMessage(char *topic, uint8_t *payload, unsigned int length);
};
