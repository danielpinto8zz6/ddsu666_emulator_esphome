#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include "config.h"

/**
 * @brief Plain struct holding runtime-configurable application parameters.
 */
struct SystemSettings {
  String wifi_ssid      = WIFI_SSID;
  String wifi_pass      = WIFI_PASSWORD;
  String chint_ip       = CHINT_XIAO_IP;
  String shelly_ip      = SHELLY_IP;
  String mqtt_broker    = MQTT_BROKER_IP;
  uint16_t mqtt_port    = MQTT_PORT;
  String mqtt_user      = MQTT_USER;
  String mqtt_pass      = MQTT_PASSWORD;
  String ota_password   = OTA_PASSWORD;
  uint8_t grid_priority = (uint8_t)DEFAULT_GRID_PRIORITY_MODE;
  uint8_t pv_priority   = (uint8_t)DEFAULT_PV_PRIORITY_MODE;
  String  filter_mode   = "damped"; // "damped" or "raw"
};

/**
 * @brief Single Responsibility: Central configuration management and NVS persistence.
 */
class AppConfig {
public:
  static AppConfig& getInstance();

  void begin();
  const SystemSettings& get() const { return _settings; }

  void setGridPriority(uint8_t mode);
  uint8_t getGridPriority() const { return _settings.grid_priority; }

  void setPvPriority(uint8_t mode);
  uint8_t getPvPriority() const { return _settings.pv_priority; }

  void setFilterMode(const String &mode);
  const String& getFilterMode() const { return _settings.filter_mode; }

  bool save(const SystemSettings &newSettings);
  void serialize(JsonDocument &doc) const;

private:
  AppConfig() = default;
  SystemSettings _settings;
  static constexpr const char *PREFS_NAMESPACE = "ddsu666";
};
