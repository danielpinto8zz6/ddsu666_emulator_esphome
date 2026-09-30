#include "app_config.h"

AppConfig& AppConfig::getInstance() {
  static AppConfig instance;
  return instance;
}

void AppConfig::begin() {
  Preferences prefs;
  if (prefs.begin(PREFS_NAMESPACE, true)) { // read-only
    _settings.wifi_ssid     = prefs.getString("wifi_ssid",  WIFI_SSID);
    _settings.wifi_pass     = prefs.getString("wifi_pass",  WIFI_PASSWORD);
    _settings.chint_ip      = prefs.getString("chint_ip",   CHINT_XIAO_IP);
    _settings.shelly_ip     = prefs.getString("shelly_ip",  SHELLY_IP);
    _settings.mqtt_broker   = prefs.getString("mqtt_broker",MQTT_BROKER_IP);
    _settings.mqtt_port     = prefs.getUShort("mqtt_port",  MQTT_PORT);
    _settings.mqtt_user     = prefs.getString("mqtt_user",  MQTT_USER);
    _settings.mqtt_pass     = prefs.getString("mqtt_pass",  MQTT_PASSWORD);
    _settings.ota_password  = prefs.getString("ota_pass",   OTA_PASSWORD);
    _settings.grid_priority = prefs.getUChar("grid_pri",    (uint8_t)DEFAULT_GRID_PRIORITY_MODE);
    prefs.end();
  }
}

void AppConfig::setGridPriority(uint8_t mode) {
  _settings.grid_priority = mode;
  Preferences prefs;
  if (prefs.begin(PREFS_NAMESPACE, false)) {
    prefs.putUChar("grid_pri", mode);
    prefs.end();
  }
}

bool AppConfig::save(const SystemSettings &newSettings) {
  Preferences prefs;
  if (!prefs.begin(PREFS_NAMESPACE, false)) {
    return false;
  }

  bool changed = false;

  if (newSettings.wifi_ssid.length() > 0 && newSettings.wifi_ssid != _settings.wifi_ssid) {
    prefs.putString("wifi_ssid", newSettings.wifi_ssid);
    _settings.wifi_ssid = newSettings.wifi_ssid;
    changed = true;
  }

  if (newSettings.wifi_pass != _settings.wifi_pass) {
    prefs.putString("wifi_pass", newSettings.wifi_pass);
    _settings.wifi_pass = newSettings.wifi_pass;
    changed = true;
  }

  if (newSettings.chint_ip.length() >= 7 && newSettings.chint_ip != _settings.chint_ip) {
    prefs.putString("chint_ip", newSettings.chint_ip);
    _settings.chint_ip = newSettings.chint_ip;
    changed = true;
  }

  if (newSettings.shelly_ip.length() >= 7 && newSettings.shelly_ip != _settings.shelly_ip) {
    prefs.putString("shelly_ip", newSettings.shelly_ip);
    _settings.shelly_ip = newSettings.shelly_ip;
    changed = true;
  }

  if (newSettings.mqtt_broker.length() >= 7 && newSettings.mqtt_broker != _settings.mqtt_broker) {
    prefs.putString("mqtt_broker", newSettings.mqtt_broker);
    _settings.mqtt_broker = newSettings.mqtt_broker;
    changed = true;
  }

  if (newSettings.mqtt_port > 0 && newSettings.mqtt_port != _settings.mqtt_port) {
    prefs.putUShort("mqtt_port", newSettings.mqtt_port);
    _settings.mqtt_port = newSettings.mqtt_port;
    changed = true;
  }

  if (newSettings.mqtt_user != _settings.mqtt_user) {
    prefs.putString("mqtt_user", newSettings.mqtt_user);
    _settings.mqtt_user = newSettings.mqtt_user;
    changed = true;
  }

  if (newSettings.mqtt_pass != _settings.mqtt_pass) {
    prefs.putString("mqtt_pass", newSettings.mqtt_pass);
    _settings.mqtt_pass = newSettings.mqtt_pass;
    changed = true;
  }

  if (newSettings.ota_password != _settings.ota_password) {
    prefs.putString("ota_pass", newSettings.ota_password);
    _settings.ota_password = newSettings.ota_password;
    changed = true;
  }

  prefs.end();
  return changed;
}

void AppConfig::serialize(JsonDocument &doc) const {
  doc["wifi_ssid"]     = _settings.wifi_ssid;
  doc["wifi_pass_set"] = (_settings.wifi_pass.length() > 0);
  doc["chint_ip"]      = _settings.chint_ip;
  doc["shelly_ip"]     = _settings.shelly_ip;
  doc["mqtt_broker"]   = _settings.mqtt_broker;
  doc["mqtt_port"]     = _settings.mqtt_port;
  doc["mqtt_user"]     = _settings.mqtt_user;
  doc["mqtt_pass_set"] = (_settings.mqtt_pass.length() > 0);
  doc["ota_password"]  = _settings.ota_password;
}
