#include "mqtt_manager.h"
#include <cmath>
#if ENABLE_ESPNOW
#include "esp_now_receiver.h"
#endif

MQTTManager::MQTTManager(ModbusRTUServer &modbusServer)
  : _modbusServer(modbusServer),
    _mqttClient(_wifiClient),
    _lastGridMsgTime(0),
    _lastPvMsgTime(0),
    _lastReconnectAttempt(0),
    _lastWiFiConnectAttempt(0),
    _lastShellyPollTime(0),
    _lastShellyEnergyPollTime(0),
    _lastTelemetryPublishTime(0),
    _lastFastPublishTime(0),
    _lastPublishedGridWatts(0.0f),
    _lastPublishedPvWatts(0.0f),
    _gridWatchdogTriggered(false),
    _pvWatchdogTriggered(false),
    _shellyWsConnected(false),
    _haDiscoveryPublished(false) {
}

void MQTTManager::begin() {
  _mqttClient.setServer(MQTT_BROKER_IP, MQTT_PORT);
  _mqttClient.setBufferSize(768);
  _mqttClient.setCallback([this](char *topic, uint8_t *payload, unsigned int length) {
    this->onMqttMessage(topic, payload, length);
  });

  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.setSleep(false);
  WiFi.setTxPower(WIFI_POWER_19_5dBm);

#if ENABLE_ESPNOW
  ESPNowReceiver::begin();
#endif

  connectWiFi();
  connectShellyWS();

  _lastGridMsgTime = millis();
  _lastPvMsgTime = millis();
  _lastShellyPollTime = millis();
  _lastShellyEnergyPollTime = millis();
}

void MQTTManager::connectWiFi() {
  if (WiFi.status() == WL_CONNECTED) return;

  uint32_t now = millis();
  if (_lastWiFiConnectAttempt != 0 && (now - _lastWiFiConnectAttempt < 8000)) {
    return; // Wait for ongoing connection attempt
  }
  _lastWiFiConnectAttempt = now;

#if WIFI_FORCE_CHANNEL > 0
  Serial.printf("[WiFi] Connecting to %s on Channel %d...\n", WIFI_SSID, WIFI_FORCE_CHANNEL);
  WiFi.disconnect();
  delay(100);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD, WIFI_FORCE_CHANNEL);
#else
  Serial.printf("[WiFi] Connecting to %s...\n", WIFI_SSID);
  WiFi.disconnect();
  delay(100);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
#endif
}

void MQTTManager::connectShellyWS() {
#if ENABLE_ESPNOW && (GRID_SOURCE_PRIORITY == 1)
  Serial.println("[Shelly WS] Disabled on Lilygo (Dedicated ESP-NOW Mode from Lolin C3)");
  return;
#else
  Serial.printf("[Shelly WS] Initializing connection to ws://%s:%d%s\n",
                SHELLY_IP, SHELLY_WS_PORT, SHELLY_WS_PATH);
  _wsClient.begin(SHELLY_IP, SHELLY_WS_PORT, SHELLY_WS_PATH);
  _wsClient.onEvent([this](WStype_t type, uint8_t *payload, size_t length) {
    this->onWsEvent(type, payload, length);
  });
  _wsClient.setReconnectInterval(2000);
#endif
}

void MQTTManager::connectMQTT() {
  if (_mqttClient.connected()) return;
  if (WiFi.status() != WL_CONNECTED) return;

  uint32_t now = millis();
  if (now - _lastReconnectAttempt < 5000) return; // Retry every 5s
  _lastReconnectAttempt = now;

  Serial.print("[OpenDTU MQTT] Connecting to broker...");
  bool ok;
  if (strlen(MQTT_USER) > 0) {
    ok = _mqttClient.connect(MQTT_CLIENT_ID, MQTT_USER, MQTT_PASSWORD);
  } else {
    ok = _mqttClient.connect(MQTT_CLIENT_ID);
  }

  if (ok) {
    Serial.println(" connected!");
    _mqttClient.subscribe(TOPIC_OPENDTU_POWER);
    _mqttClient.subscribe(TOPIC_OPENDTU_YIELD);
    publishHADiscovery();
  } else {
    Serial.printf(" failed, rc=%d. Retrying in 5s\n", _mqttClient.state());
  }
}

void MQTTManager::loop() {
  static bool s_wasConnected = false;

  if (WiFi.status() != WL_CONNECTED) {
    if (s_wasConnected) {
      Serial.println("[WiFi] Lost connection to AP!");
      if (_mqttClient.connected()) {
        _mqttClient.disconnect();
      }
      _haDiscoveryPublished = false;
      MeterState::zeroPvPower();
      _pvWatchdogTriggered = true;
      s_wasConnected = false;
#if ENABLE_ESPNOW && (GRID_SOURCE_PRIORITY != 0)
      if (!ESPNowReceiver::isRecent(3000)) {
        if (_modbusServer.isEnabled()) _modbusServer.setEnabled(false);
        _gridWatchdogTriggered = true;
      }
#else
      if (_modbusServer.isEnabled()) _modbusServer.setEnabled(false);
      _gridWatchdogTriggered = true;
#endif
    }
    _shellyWsConnected = false;
    connectWiFi();
    return;
  }

  if (!s_wasConnected) {
    Serial.printf("[WiFi] Connected! IP: %s (RSSI: %d dBm)\n",
                  WiFi.localIP().toString().c_str(), WiFi.RSSI());
#if ENABLE_ESPNOW
    Serial.printf("[ESP-NOW] Operating on Wi-Fi Channel: %d (Receiver MAC: %s)\n",
                  WiFi.channel(), WiFi.macAddress().c_str());
#endif
    s_wasConnected = true;
  }

  // 1. Service Shelly WebSocket Client
  _wsClient.loop();

  uint32_t now = millis();

  // Adaptive polling: Only poll Shelly at 500ms when ESP-NOW is inactive
#if ENABLE_ESPNOW && (GRID_SOURCE_PRIORITY == 2)
  bool espNowActive = ESPNowReceiver::isRecent(3000);
  uint32_t pollInterval = espNowActive ? 10000 : SHELLY_POLL_INTERVAL_MS; // 10s keepalive when ESP-NOW is healthy, 500ms fallback
#elif ENABLE_ESPNOW && (GRID_SOURCE_PRIORITY == 1)
  uint32_t pollInterval = 60000;
#else
  uint32_t pollInterval = SHELLY_POLL_INTERVAL_MS;
#endif

  // Fast or adaptive polling for real-time telemetry
  if (_shellyWsConnected && (now - _lastShellyPollTime >= pollInterval)) {
    _lastShellyPollTime = now;
    _wsClient.sendTXT("{\"id\":1,\"src\":\"esp32\",\"method\":\"EM1.GetStatus\",\"params\":{\"id\":0}}");
  }

  // 60-second polling for energy accumulators
  if (_shellyWsConnected && (now - _lastShellyEnergyPollTime >= SHELLY_ENERGY_INTERVAL_MS)) {
    _lastShellyEnergyPollTime = now;
    _wsClient.sendTXT("{\"id\":2,\"src\":\"esp32\",\"method\":\"EM1Data.GetStatus\",\"params\":{\"id\":0}}");
  }

  // 2. Service OpenDTU MQTT Client & Telemetry
  if (!_mqttClient.connected()) {
    connectMQTT();
  } else {
    _mqttClient.loop();

    float curGridWatts = MeterState::getGridWatts();
    float curPvWatts   = MeterState::getPvWatts();

    bool deltaTrigger = (std::fabs(curGridWatts - _lastPublishedGridWatts) >= 25.0f) ||
                        (std::fabs(curPvWatts - _lastPublishedPvWatts) >= 25.0f);
    bool heartbeat   = (now - _lastTelemetryPublishTime >= 3000);
    bool throttleOk  = (now - _lastFastPublishTime >= 250);

    if ((deltaTrigger && throttleOk) || heartbeat) {
      _lastTelemetryPublishTime = now;
      _lastFastPublishTime = now;
      _lastPublishedGridWatts = curGridWatts;
      _lastPublishedPvWatts = curPvWatts;
      publishTelemetry();
    }
  }

  checkWatchdogs();
}

void MQTTManager::onWsEvent(WStype_t type, uint8_t *payload, size_t length) {
  switch (type) {
    case WStype_DISCONNECTED:
      _shellyWsConnected = false;
      Serial.println("[Shelly WS] Disconnected");
      break;

    case WStype_CONNECTED:
      _shellyWsConnected = true;
      Serial.printf("[Shelly WS] Connected to ws://%s:%d%s\n", SHELLY_IP, SHELLY_WS_PORT, SHELLY_WS_PATH);
      // Immediately request full status (both real-time and energy totals)
      _wsClient.sendTXT("{\"id\":10,\"src\":\"esp32\",\"method\":\"Shelly.GetStatus\"}");
      break;

    case WStype_TEXT:
      handleShellyMessage(payload, length);
      break;

    case WStype_ERROR:
      Serial.println("[Shelly WS] Communication error");
      break;

    default:
      break;
  }
}

void MQTTManager::handleShellyMessage(const uint8_t *payload, size_t length) {
  static StaticJsonDocument<256> filter;
  static bool s_filterInit = false;
  if (!s_filterInit) {
    filter["result"]["em1:0"] = true;
    filter["result"]["em1data:0"] = true;
    filter["result"]["voltage"] = true;
    filter["result"]["current"] = true;
    filter["result"]["act_power"] = true;
    filter["result"]["aprt_power"] = true;
    filter["result"]["pf"] = true;
    filter["result"]["freq"] = true;
    filter["result"]["total_act_energy"] = true;
    filter["result"]["total_act_ret_energy"] = true;
    filter["params"]["em1:0"] = true;
    filter["params"]["em1data:0"] = true;
    s_filterInit = true;
  }

  StaticJsonDocument<512> doc;
  DeserializationError err = deserializeJson(doc, payload, length, DeserializationOption::Filter(filter));
  if (err) return;

  JsonObject em1;
  JsonObject em1data;

  // Extract nested component data from RPC responses or async notifications
  if (doc.containsKey("result")) {
    JsonObject res = doc["result"];
    if (res.containsKey("em1:0")) {
      em1 = res["em1:0"];
    } else if (res.containsKey("voltage")) {
      em1 = res;
    }

    if (res.containsKey("em1data:0")) {
      em1data = res["em1data:0"];
    } else if (res.containsKey("total_act_energy")) {
      em1data = res;
    }
  } else if (doc.containsKey("params")) {
    JsonObject par = doc["params"];
    if (par.containsKey("em1:0")) {
      em1 = par["em1:0"];
    }
    if (par.containsKey("em1data:0")) {
      em1data = par["em1data:0"];
    }
  }

  // 1. Process Real-Time Telemetry
  if (!em1.isNull() && em1.containsKey("voltage")) {
    float v = em1["voltage"].as<float>();
    if (!std::isnan(v) && v >= 180.0f && v <= 280.0f) {
      _lastGridMsgTime = millis();
      if (_gridWatchdogTriggered || !_modbusServer.isEnabled()) {
        Serial.println("[Shelly WS] Valid voltage received, restoring RS485.");
        _modbusServer.setEnabled(true);
        _gridWatchdogTriggered = false;
      }

      float c = 0.0f;
      if (em1.containsKey("current")) {
        float raw_c = em1["current"].as<float>();
        if (!std::isnan(raw_c) && raw_c >= 0.0f && raw_c < 250.0f) c = raw_c;
      }

      float p_kw = 0.0f;
      if (em1.containsKey("act_power")) {
        float raw_p = em1["act_power"].as<float>();
        if (!std::isnan(raw_p) && raw_p > -15000.0f && raw_p < 15000.0f) {
          // Invert sign for Hoymiles: Grid import is negative, export is positive
          p_kw = (raw_p / 1000.0f) * -1.0f;
        }
      }

      float s_kw = 0.0f;
      float q_kvar = 0.0f;
      if (em1.containsKey("aprt_power")) {
        float raw_s = em1["aprt_power"].as<float>();
        if (!std::isnan(raw_s) && raw_s >= 0.0f && raw_s < 15000.0f) {
          s_kw = raw_s / 1000.0f;
          float p_abs = std::fabs(p_kw);
          if (s_kw > p_abs) {
            q_kvar = std::sqrt(s_kw * s_kw - p_abs * p_abs);
          }
        }
      }

      float pf = 1.0f;
      if (em1.containsKey("pf")) {
        float raw_pf = em1["pf"].as<float>();
        if (!std::isnan(raw_pf) && raw_pf >= -1.0f && raw_pf <= 1.0f) pf = raw_pf;
      }

      float f = 50.0f;
      if (em1.containsKey("freq")) {
        float raw_f = em1["freq"].as<float>();
        if (!std::isnan(raw_f) && raw_f > 45.0f && raw_f < 55.0f) f = raw_f;
      }

#if ENABLE_ESPNOW
      bool useShellyGrid = (GRID_SOURCE_PRIORITY == 0) || 
                           (GRID_SOURCE_PRIORITY == 2 && !ESPNowReceiver::isRecent());
#else
      bool useShellyGrid = true;
#endif
      if (useShellyGrid) {
        MeterState::updateGridRealtime(v, c, p_kw, s_kw, q_kvar, pf, f);
      }
    }
  }

  // 2. Process Energy Accumulators
  if (!em1data.isNull()) {
    float import_kwh = -1.0f;
    float export_kwh = -1.0f;

    if (em1data.containsKey("total_act_energy")) {
      import_kwh = em1data["total_act_energy"].as<float>() / 1000.0f;
    }
    if (em1data.containsKey("total_act_ret_energy")) {
      export_kwh = em1data["total_act_ret_energy"].as<float>() / 1000.0f;
    }

    if (import_kwh >= 0.0f || export_kwh >= 0.0f) {
      MeterState::updateGridEnergy(import_kwh, export_kwh);
    }
  }
}

void MQTTManager::onMqttMessage(char *topic, uint8_t *payload, unsigned int length) {
  if (strcmp(topic, TOPIC_OPENDTU_POWER) == 0) {
    handleOpenDtuPower(payload, length);
  } else if (strcmp(topic, TOPIC_OPENDTU_YIELD) == 0) {
    handleOpenDtuYield(payload, length);
  }
}

void MQTTManager::handleOpenDtuPower(const uint8_t *payload, unsigned int length) {
  char buf[32];
  size_t len = (length < sizeof(buf) - 1) ? length : sizeof(buf) - 1;
  memcpy(buf, payload, len);
  buf[len] = '\0';

  float p = atof(buf);
  if (std::isnan(p) || p < 0.0f || p > 10000.0f) {
    Serial.printf("[PV] Rejected bad power: %.1f\n", p);
    return;
  }

  _lastPvMsgTime = millis();
  _pvWatchdogTriggered = false;
  MeterState::updatePvPower(p);
}

void MQTTManager::handleOpenDtuYield(const uint8_t *payload, unsigned int length) {
  char buf[32];
  size_t len = (length < sizeof(buf) - 1) ? length : sizeof(buf) - 1;
  memcpy(buf, payload, len);
  buf[len] = '\0';

  float y = atof(buf);
  if (!std::isnan(y) && y >= 0.0f) {
    MeterState::updatePvYield(y);
  }
}

void MQTTManager::checkWatchdogs() {
  uint32_t now = millis();

#if ENABLE_ESPNOW
  if (ESPNowReceiver::isRecent(WATCHDOG_TIMEOUT_SEC * 1000)) {
    _lastGridMsgTime = now;
    if (_gridWatchdogTriggered || !_modbusServer.isEnabled()) {
      Serial.println("[ESP-NOW] Valid packet received, restoring RS485.");
      _modbusServer.setEnabled(true);
      _gridWatchdogTriggered = false;
    }
  }
#endif

  // 1. Grid watchdog (10 seconds timeout)
  if (!_gridWatchdogTriggered && (now - _lastGridMsgTime > (WATCHDOG_TIMEOUT_SEC * 1000))) {
    Serial.println("[FAILSAFE] Grid Shelly WS timeout! Disabling RS485 to force Hoymiles safe state.");
    _modbusServer.setEnabled(false);
    _gridWatchdogTriggered = true;
  }

  // 2. PV watchdog (10 seconds timeout)
  if (!_pvWatchdogTriggered && (now - _lastPvMsgTime > (WATCHDOG_TIMEOUT_SEC * 1000))) {
    Serial.println("[FAILSAFE] PV OpenDTU MQTT timeout! Zeroing PV power.");
    MeterState::zeroPvPower();
    _pvWatchdogTriggered = true;
  }
}

void MQTTManager::publishHADiscovery() {
  if (_haDiscoveryPublished) return;

  const char *dev = "\"dev\":{\"ids\":[\"ddsu666_emulator\"],\"name\":\"Hoymiles DDSU666 Emulator\",\"mf\":\"LILYGO\",\"mdl\":\"T-CAN485\"}";

  struct SensorDef {
    const char *id;
    const char *name;
    const char *unit;
    const char *dev_cla;
    const char *stat_cla;
    const char *val_key;
    const char *cat;
  };

  SensorDef sensors[] = {
    { "grid_power", "Grid Active Power", "W", "power", "measurement", "grid_w", nullptr },
    { "grid_voltage", "Grid Voltage", "V", "voltage", "measurement", "grid_v", nullptr },
    { "grid_current", "Grid Current", "A", "current", "measurement", "grid_a", nullptr },
    { "grid_frequency", "Grid Frequency", "Hz", "frequency", "measurement", "grid_hz", nullptr },
    { "grid_pf", "Grid Power Factor", nullptr, "power_factor", "measurement", "grid_pf", nullptr },
    { "grid_import_kwh", "Grid Import Energy", "kWh", "energy", "total_increasing", "grid_imp_kwh", nullptr },
    { "grid_export_kwh", "Grid Export Energy", "kWh", "energy", "total_increasing", "grid_exp_kwh", nullptr },
    { "pv_power", "PV Active Power", "W", "power", "measurement", "pv_w", nullptr },
    { "pv_yield_kwh", "PV Total Yield", "kWh", "energy", "total_increasing", "pv_kwh", nullptr },
    { "inv_rate_hz", "Inverter Modbus Rate", "Hz", nullptr, "measurement", "inv_hz", "diagnostic" },
    { "wifi_rssi", "WiFi Signal", "dBm", "signal_strength", "measurement", "rssi", "diagnostic" },
    { "espnow_rssi", "ESP-NOW Signal", "dBm", "signal_strength", "measurement", "espnow_rssi", "diagnostic" },
    { "espnow_packets", "ESP-NOW Packets", nullptr, nullptr, "total_increasing", "espnow_pkts", "diagnostic" }
  };

  char topic[80];
  char payload[384];

  for (const auto &s : sensors) {
    snprintf(topic, sizeof(topic), "homeassistant/sensor/ddsu666/%s/config", s.id);

    int offset = snprintf(payload, sizeof(payload),
      "{\"name\":\"%s\",\"stat_t\":\"ddsu666/telemetry\",\"val_tpl\":\"{{ value_json.%s }}\",\"uniq_id\":\"ddsu666_%s\",%s",
      s.name, s.val_key, s.id, dev);

    if (s.unit) {
      offset += snprintf(payload + offset, sizeof(payload) - offset, ",\"unit_of_meas\":\"%s\"", s.unit);
    }
    if (s.dev_cla) {
      offset += snprintf(payload + offset, sizeof(payload) - offset, ",\"dev_cla\":\"%s\"", s.dev_cla);
    }
    if (s.stat_cla) {
      offset += snprintf(payload + offset, sizeof(payload) - offset, ",\"stat_cla\":\"%s\"", s.stat_cla);
    }
    if (s.cat) {
      offset += snprintf(payload + offset, sizeof(payload) - offset, ",\"ent_cat\":\"%s\"", s.cat);
    }
    snprintf(payload + offset, sizeof(payload) - offset, "}");

    _mqttClient.publish(topic, payload, true); // Retain true for HA discovery
  }

  _haDiscoveryPublished = true;
  Serial.println("[HA Discovery] Published 13 sensor configs to MQTT broker.");
}

void MQTTManager::publishTelemetry() {
  MeterTelemetry g = MeterState::getGrid();
  MeterTelemetry p = MeterState::getPv();

#if ENABLE_ESPNOW
  EspNowStats es = ESPNowReceiver::getStats();
  int espnowRssi = (int)es.rssi;
  uint32_t espnowPkts = es.packet_count;
#else
  int espnowRssi = 0;
  uint32_t espnowPkts = 0;
#endif

  char payload[384];
  snprintf(payload, sizeof(payload),
    "{"
      "\"grid_w\":%.1f,"
      "\"grid_v\":%.1f,"
      "\"grid_a\":%.2f,"
      "\"grid_hz\":%.1f,"
      "\"grid_pf\":%.2f,"
      "\"grid_imp_kwh\":%.2f,"
      "\"grid_exp_kwh\":%.2f,"
      "\"pv_w\":%.1f,"
      "\"pv_kwh\":%.2f,"
      "\"inv_hz\":%.1f,"
      "\"rssi\":%d,"
      "\"espnow_rssi\":%d,"
      "\"espnow_pkts\":%lu"
    "}",
    (double)(g.active_power * -1000.0f), // Invert to physical convention (+ = import, - = export)
    (double)g.voltage,
    (double)g.current,
    (double)g.frequency,
    (double)g.power_factor,
    (double)g.import_kwh,
    (double)g.export_kwh,
    (double)(p.active_power * 1000.0f),
    (double)p.import_kwh,
    (double)_modbusServer.getQueryRateHz(),
    WiFi.RSSI(),
    espnowRssi,
    (unsigned long)espnowPkts
  );

  _mqttClient.publish("ddsu666/telemetry", payload, false);
}

