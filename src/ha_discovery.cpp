#include "ha_discovery.h"
#include <ArduinoJson.h>
#include <cmath>

static inline float round1(float v) { return std::round(v * 10.0f) / 10.0f; }
static inline float round2(float v) { return std::round(v * 100.0f) / 100.0f; }

struct HaSensorDef {
  const char *id;
  const char *name;
  const char *val_key;
  const char *unit;
  const char *device_class;
  const char *state_class;
  const char *icon;
};

static const HaSensorDef HA_SENSORS[] = {
  {"grid_power",        "Grid Active Power",          "grid_power",        "W",   "power",        "measurement",      "mdi:transmission-tower"},
  {"pv_power",          "PV Active Power",            "pv_power",          "W",   "power",        "measurement",      "mdi:solar-power"},
  {"grid_voltage",      "Grid Voltage",               "grid_voltage",      "V",   "voltage",      "measurement",      nullptr},
  {"grid_current",      "Grid Current",               "grid_current",      "A",   "current",      "measurement",      nullptr},
  {"grid_frequency",    "Grid Frequency",             "grid_frequency",    "Hz",  "frequency",    "measurement",      nullptr},
  {"grid_power_factor", "Grid Power Factor",          "grid_power_factor", nullptr, "power_factor", "measurement",   nullptr},
  {"import_energy",     "Grid Import Energy",         "import_energy",     "kWh", "energy",       "total_increasing", nullptr},
  {"export_energy",     "Grid Export Energy",         "export_energy",     "kWh", "energy",       "total_increasing", nullptr},
  {"pv_yield",          "PV Total Yield",             "pv_yield",          "kWh", "energy",       "total_increasing", nullptr},
  {"modbus_query_rate", "Inverter Modbus Query Rate", "modbus_query_rate", "Hz",  nullptr,        "measurement",      "mdi:speedometer"},
  {"modbus_crc_errors", "Modbus CRC Errors",          "modbus_crc_errors", nullptr, nullptr,      "total_increasing", "mdi:alert-circle-outline"},
  {"active_grid_source","Active Grid Source",         "active_grid_source",nullptr, nullptr,      nullptr,            "mdi:swap-horizontal"},
  {"active_pv_source",  "Active PV Source",           "active_pv_source",  nullptr, nullptr,      nullptr,            "mdi:solar-panel"},
  {"filter_mode",       "Power Filter Mode",          "filter_mode",       nullptr, nullptr,      nullptr,            "mdi:filter-outline"}
};

HaDiscovery::HaDiscovery(MqttManager &mqtt)
  : _mqtt(mqtt) {
}

void HaDiscovery::begin(ModbusRTUServer *modbus, TelemetryManager *telemetry) {
  _modbus = modbus;
  _telemetry = telemetry;

  _mqtt.onConnectionChange([this](bool connected) {
    if (connected) {
      Serial.println("[HA Discovery] MQTT connected -> announcing entity configs");
      this->publishDiscovery();
    }
  });

  if (_mqtt.isConnected()) {
    publishDiscovery();
  }
}

void HaDiscovery::publishDiscovery() {
  const size_t sensorCount = sizeof(HA_SENSORS) / sizeof(HA_SENSORS[0]);

  for (size_t i = 0; i < sensorCount; i++) {
    const auto &s = HA_SENSORS[i];
    StaticJsonDocument<768> doc;

    char topic[128];
    snprintf(topic, sizeof(topic), "homeassistant/sensor/ddsu666_emulator/%s/config", s.id);

    char uniqueId[64];
    snprintf(uniqueId, sizeof(uniqueId), "ddsu666_emulator_%s", s.id);

    char valTpl[64];
    snprintf(valTpl, sizeof(valTpl), "{{ value_json.%s }}", s.val_key);

    doc["name"] = s.name;
    doc["stat_t"] = "ddsu666_emulator/state";
    doc["val_tpl"] = valTpl;
    doc["uniq_id"] = uniqueId;
    doc["avty_t"] = "ddsu666_emulator/status";

    if (s.unit) doc["unit_of_meas"] = s.unit;
    if (s.device_class) doc["dev_cla"] = s.device_class;
    if (s.state_class) doc["stat_cla"] = s.state_class;
    if (s.icon) doc["icon"] = s.icon;

    JsonObject dev = doc.createNestedObject("dev");
    JsonArray ids = dev.createNestedArray("ids");
    ids.add("ddsu666_emulator_xiao");
    dev["name"] = "DDSU666 Modbus Emulator";
    dev["mf"]   = "Chint / Hoymiles Zero-Export Bridge";
    dev["mdl"]  = "Seeed XIAO ESP32-S3 RS485";
    dev["sw"]   = "v2.0.0-solid";
    dev["cu"]   = "http://ddsu666.local";

    char payload[640];
    serializeJson(doc, payload, sizeof(payload));
    _mqtt.publish(topic, payload, true);
    vTaskDelay(pdMS_TO_TICKS(10)); // Paced publish to prevent MQTT queue overflow
  }

  _discoveryPublished = true;
  Serial.printf("[HA Discovery] Published %u entity discovery configs\n", (unsigned int)sensorCount);
}

void HaDiscovery::publishState() {
  if (!_mqtt.isConnected()) return;

  MeterTelemetry grid = MeterState::getGrid();
  MeterTelemetry pv   = MeterState::getPv();

  StaticJsonDocument<512> doc;
  doc["grid_power"]        = round1(MeterState::getGridWatts());
  doc["pv_power"]          = round1(MeterState::getPvWatts());
  doc["grid_voltage"]      = round1(grid.voltage);
  doc["grid_current"]      = round2(grid.current);
  doc["grid_frequency"]    = round1(grid.frequency);
  doc["grid_power_factor"] = round2(grid.power_factor);
  doc["import_energy"]     = round2(grid.import_kwh);
  doc["export_energy"]     = round2(grid.export_kwh);
  doc["pv_yield"]          = round2(pv.import_kwh);

  if (_modbus) {
    doc["modbus_query_rate"] = round1(_modbus->getQueryRateHz());
    doc["modbus_crc_errors"] = _modbus->getCrcErrorCount();
  }

  if (_telemetry) {
    doc["active_grid_source"] = _telemetry->getActiveGridSource();
    doc["active_pv_source"]   = _telemetry->getActivePvSource();
    doc["filter_mode"]        = _telemetry->getFilterMode();
  }

  char payload[512];
  serializeJson(doc, payload, sizeof(payload));
  _mqtt.publish("ddsu666_emulator/state", payload, false);
}

void HaDiscovery::loop() {
  uint32_t now = millis();
  if (now - _lastStatePublish >= _stateIntervalMs) {
    _lastStatePublish = now;
    publishState();
  }
}
