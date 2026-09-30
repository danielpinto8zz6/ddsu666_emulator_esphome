#pragma once

#include <Arduino.h>
#include "meter_data.h"
#include "modbus_rtu.h"
#include "telemetry/telemetry_manager.h"
#include "mqtt/mqtt_manager.h"

/**
 * @brief Home Assistant MQTT Auto-Discovery & Telemetry Publisher (SOLID: SRP & DIP).
 *
 * Automatically announces DDSU666 Modbus RTU emulator sensors to Home Assistant via
 * MQTT Discovery standard topics, and publishes periodic state JSON telemetry payloads.
 */
class HaDiscovery {
public:
  HaDiscovery(MqttManager &mqtt = MqttManager::getInstance());

  void begin(ModbusRTUServer *modbus, TelemetryManager *telemetry);
  void loop();

  void publishDiscovery();
  void publishState();

  void setStateInterval(uint32_t ms) { _stateIntervalMs = ms; }

private:
  MqttManager &_mqtt;
  ModbusRTUServer *_modbus = nullptr;
  TelemetryManager *_telemetry = nullptr;

  uint32_t _lastStatePublish = 0;
  uint32_t _stateIntervalMs = 2000; // 2-second telemetry cadence
  bool _discoveryPublished = false;
};
