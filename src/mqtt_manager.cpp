#include "mqtt_manager.h"
#include <cmath>
#include <esp_now.h>
#include <esp_wifi.h>

MQTTManager* MQTTManager::s_instance = nullptr;

MQTTManager::MQTTManager(ModbusRTUServer &modbusServer)
  : _modbusServer(modbusServer),
    _mqttClient(_wifiClient),
    _gridPriority(DEFAULT_GRID_PRIORITY_MODE),
    _lastGridMsgTime(0),
    _lastChintMsgTime(0),
    _lastShellyMsgTime(0),
    _lastPvMsgTime(0),
    _lastReconnectAttempt(0),
    _lastWiFiConnectAttempt(0),
    _lastShellyPollTime(0),
    _lastShellyEnergyPollTime(0),
    _gridWatchdogTriggered(false),
    _pvWatchdogTriggered(false),
    _shellyWsConnected(false),
    _chintWsConnected(false),
    _sourceEventHead(0),
    _sourceEventCount(0) {
  s_instance = this;
  strncpy(_lastKnownSource, "NONE", sizeof(_lastKnownSource));
  memset(_sourceEvents, 0, sizeof(_sourceEvents));
}

void MQTTManager::begin() {
  s_instance = this;

  Preferences prefs;
  if (prefs.begin("ddsu666", false)) {
    _gridPriority = (GridPriorityMode)prefs.getUChar("grid_pri", (uint8_t)DEFAULT_GRID_PRIORITY_MODE);
    // Runtime-configurable network settings (overrides compile-time defaults)
    _chintIp     = prefs.getString("chint_ip",    CHINT_XIAO_IP);
    _shellyIp    = prefs.getString("shelly_ip",   SHELLY_IP);
    _mqttBroker  = prefs.getString("mqtt_broker", MQTT_BROKER_IP);
    _mqttPort    = prefs.getUShort("mqtt_port",   MQTT_PORT);
    _mqttUser    = prefs.getString("mqtt_user",   MQTT_USER);
    _mqttPass    = prefs.getString("mqtt_pass",   MQTT_PASSWORD);
    _wifiSsid    = prefs.getString("wifi_ssid",   WIFI_SSID);
    _wifiPassword= prefs.getString("wifi_pass",   WIFI_PASSWORD);
    prefs.end();
  } else {
    _chintIp     = CHINT_XIAO_IP;
    _shellyIp    = SHELLY_IP;
    _mqttBroker  = MQTT_BROKER_IP;
    _mqttPort    = MQTT_PORT;
    _mqttUser    = MQTT_USER;
    _mqttPass    = MQTT_PASSWORD;
    _wifiSsid    = WIFI_SSID;
    _wifiPassword= WIFI_PASSWORD;
  }

  Serial.printf("[Config] Chint: %s | Shelly: %s | MQTT: %s:%u\n",
                _chintIp.c_str(), _shellyIp.c_str(), _mqttBroker.c_str(), _mqttPort);

  _mqttClient.setServer(_mqttBroker.c_str(), _mqttPort);
  _mqttClient.setBufferSize(768);
  _mqttClient.setCallback([this](char *topic, uint8_t *payload, unsigned int length) {
    this->onMqttMessage(topic, payload, length);
  });

  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.setSleep(false);
  WiFi.setTxPower(WIFI_POWER_19_5dBm);

  connectWiFi();
  connectChintWS();
  connectShellyWS();

  // Initialize ESP-NOW receiver for blackout-proof PV streaming from Sonoff POWCT
  if (esp_now_init() == ESP_OK) {
    esp_now_register_recv_cb([](const uint8_t *mac_addr, const uint8_t *data, int data_len) {
      if (data_len == 9 && data && data[0] == 0xD6) {
        float watts = 0.0f;
        float yield_kwh = 0.0f;
        memcpy(&watts, data + 1, 4);
        memcpy(&yield_kwh, data + 5, 4);
        if (s_instance) {
          s_instance->handleEspNowPv(watts, yield_kwh);
        }
      }
    });
    Serial.println("[ESP-NOW] Initialized PV receiver (listening for 0xD6 packets from Sonoff POWCT)");
  } else {
    Serial.println("[ESP-NOW] Init failed");
  }

  _lastGridMsgTime = millis();
  _lastChintMsgTime = 0;
  _lastShellyMsgTime = 0;
  _lastPvMsgTime = millis();
  _lastShellyPollTime = millis();
  _lastShellyEnergyPollTime = millis();
}

void MQTTManager::setGridPriority(GridPriorityMode mode) {
  _gridPriority = mode;
  Preferences prefs;
  if (prefs.begin("ddsu666", false)) {
    prefs.putUChar("grid_pri", (uint8_t)mode);
    prefs.end();
  }
  Serial.printf("[Priority] Grid Priority Mode set to %u: %s\n", (unsigned)mode, getGridPriorityDescription());
}

const char* MQTTManager::getGridPriorityDescription() const {
  switch (_gridPriority) {
    case GRID_PRI_CHINT_SHELLY: return "Auto: Chint WS -> Shelly WS";
    case GRID_PRI_SHELLY_CHINT: return "Auto: Shelly WS -> Chint WS";
    case GRID_PRI_CHINT_ONLY:   return "Chint WS Only";
    case GRID_PRI_SHELLY_ONLY:  return "Shelly WS Only";
    default:                    return "Unknown";
  }
}

bool MQTTManager::canChintUpdateGrid() const {
  switch (_gridPriority) {
    case GRID_PRI_CHINT_SHELLY:
    case GRID_PRI_CHINT_ONLY:
      return true;
    case GRID_PRI_SHELLY_CHINT:
      return !isShellyRecent(5000);
    case GRID_PRI_SHELLY_ONLY:
    default:
      return false;
  }
}

bool MQTTManager::canShellyUpdateGrid() const {
  switch (_gridPriority) {
    case GRID_PRI_CHINT_SHELLY:
      return !isChintRecent(CHINT_TIMEOUT_MS);
    case GRID_PRI_SHELLY_CHINT:
    case GRID_PRI_SHELLY_ONLY:
      return true;
    case GRID_PRI_CHINT_ONLY:
    default:
      return false;
  }
}

void MQTTManager::connectWiFi() {
  if (WiFi.status() == WL_CONNECTED) return;

  uint32_t now = millis();
  if (_lastWiFiConnectAttempt != 0 && (now - _lastWiFiConnectAttempt < 8000)) {
    return; // Wait for ongoing connection attempt
  }
  _lastWiFiConnectAttempt = now;

  String ssid = (_wifiSsid.length() > 0) ? _wifiSsid : String(WIFI_SSID);
  String pass = (_wifiPassword.length() > 0) ? _wifiPassword : String(WIFI_PASSWORD);

#if WIFI_FORCE_CHANNEL > 0
  Serial.printf("[WiFi] Connecting to %s on Channel %d...\n", ssid.c_str(), WIFI_FORCE_CHANNEL);
  WiFi.disconnect();
  delay(100);
  WiFi.begin(ssid.c_str(), pass.c_str(), WIFI_FORCE_CHANNEL);
#else
  Serial.printf("[WiFi] Connecting to %s...\n", ssid.c_str());
  WiFi.disconnect();
  delay(100);
  WiFi.begin(ssid.c_str(), pass.c_str());
#endif
}

void MQTTManager::connectChintWS() {
  Serial.printf("[Chint WS] Initializing connection to ws://%s:%d%s\n",
                _chintIp.c_str(), CHINT_XIAO_WS_PORT, CHINT_XIAO_WS_PATH);
  _wsChint.begin(_chintIp.c_str(), CHINT_XIAO_WS_PORT, CHINT_XIAO_WS_PATH);
  _wsChint.onEvent([this](WStype_t type, uint8_t *payload, size_t length) {
    this->onChintWsEvent(type, payload, length);
  });
  _wsChint.setReconnectInterval(2000);
  _wsChint.enableHeartbeat(5000, 2000, 2);
}

void MQTTManager::connectShellyWS() {
  Serial.printf("[Shelly WS] Initializing connection to ws://%s:%d%s\n",
                _shellyIp.c_str(), SHELLY_WS_PORT, SHELLY_WS_PATH);
  _wsShelly.begin(_shellyIp.c_str(), SHELLY_WS_PORT, SHELLY_WS_PATH);
  _wsShelly.onEvent([this](WStype_t type, uint8_t *payload, size_t length) {
    this->onShellyWsEvent(type, payload, length);
  });
  _wsShelly.setReconnectInterval(2000);
  _wsShelly.enableHeartbeat(10000, 3000, 2);
}

void MQTTManager::connectMQTT() {
  if (_mqttClient.connected()) return;
  if (WiFi.status() != WL_CONNECTED) return;

  uint32_t now = millis();
  if (now - _lastReconnectAttempt < 5000) return; // Retry every 5s
  _lastReconnectAttempt = now;

  Serial.print("[OpenDTU MQTT] Connecting to broker...");
  bool ok;
  if (_mqttUser.length() > 0) {
    ok = _mqttClient.connect(MQTT_CLIENT_ID, _mqttUser.c_str(), _mqttPass.c_str());
  } else {
    ok = _mqttClient.connect(MQTT_CLIENT_ID);
  }

  if (ok) {
    Serial.println(" connected!");
    _mqttClient.subscribe(TOPIC_OPENDTU_POWER);
    _mqttClient.subscribe(TOPIC_OPENDTU_YIELD);
  } else {
    Serial.printf(" failed, rc=%d. Retrying in 5s\n", _mqttClient.state());
  }
}

void MQTTManager::loop() {
  static bool s_wasConnected = false;
  static uint32_t s_wifiConnectedTime = 0;

  if (WiFi.status() != WL_CONNECTED) {
    if (s_wasConnected) {
      Serial.println("[WiFi] Lost connection to AP!");
      if (_mqttClient.connected()) {
        _mqttClient.disconnect();
      }
      _wsChint.disconnect();
      _wsShelly.disconnect();
      MeterState::zeroPvPower();
      _pvWatchdogTriggered = true;
      s_wasConnected = false;
      if (_modbusServer.isEnabled()) _modbusServer.setEnabled(false);
      _gridWatchdogTriggered = true;
    }
    _shellyWsConnected = false;
    _chintWsConnected = false;
    connectWiFi();
    return;
  }

  if (!s_wasConnected) {
    Serial.printf("[WiFi] Connected! IP: %s (RSSI: %d dBm)\n",
                  WiFi.localIP().toString().c_str(), WiFi.RSSI());
    s_wasConnected = true;
    s_wifiConnectedTime = millis();
  }

  // 1. Service WebSockets (Chint Master & Shelly)
  _wsChint.loop();
  _wsShelly.loop();

  uint32_t now = millis();

  // Auto-recovery for Chint WS:
  // If telemetry has stopped or never arrived for > 8s, force reset the connection.
  // Re-calling begin() tears down any hung socket and starts a fresh handshake.
  static uint32_t s_lastChintResetAttempt = 0;
  uint32_t chintSilence = (_lastChintMsgTime != 0) ? (now - _lastChintMsgTime) : (now - s_wifiConnectedTime);
  if (s_wasConnected && chintSilence > 8000 && (now - s_lastChintResetAttempt > 8000)) {
    s_lastChintResetAttempt = now;
    Serial.println("[Chint WS] Stale or missing telemetry, resetting connection...");
    _chintWsConnected = false;
    _wsChint.disconnect();
    _wsChint.begin(_chintIp.c_str(), CHINT_XIAO_WS_PORT, CHINT_XIAO_WS_PATH);
  }

  bool chintFresh = isChintRecent(CHINT_TIMEOUT_MS);

  // Shelly Polling Strategy:
  // Shelly pushes updates automatically via WebSocket NotifyStatus.
  // We only send a poll fallback if Shelly has been quiet for > 2000ms.
  if (_shellyWsConnected && canShellyUpdateGrid()) {
    if (now - _lastShellyPollTime >= 2500 && (now - _lastShellyMsgTime >= 2000)) {
      _lastShellyPollTime = now;
      _wsShelly.sendTXT("{\"id\":1,\"src\":\"esp32\",\"method\":\"EM1.GetStatus\",\"params\":{\"id\":0}}");
    }
  }

  // 60-second polling for energy accumulators from Shelly (only during fallback when Chint is offline)
  if (_shellyWsConnected && !chintFresh && (now - _lastShellyEnergyPollTime >= SHELLY_ENERGY_INTERVAL_MS)) {
    _lastShellyEnergyPollTime = now;
    _wsShelly.sendTXT("{\"id\":2,\"src\":\"esp32\",\"method\":\"EM1Data.GetStatus\",\"params\":{\"id\":0}}");
  }

  // 2. Service OpenDTU MQTT Client
  if (!_mqttClient.connected()) {
    connectMQTT();
  } else {
    _mqttClient.loop();
  }

  checkWatchdogs();
  checkSourceTransition();
}

const char* MQTTManager::getActiveGridSource() const {
  bool chintFresh = isChintRecent(CHINT_TIMEOUT_MS);
  bool shellyFresh = isShellyRecent(5000);

  switch (_gridPriority) {
    case GRID_PRI_CHINT_SHELLY:
      if (chintFresh) return "CHINT_WS";
      if (shellyFresh) return "SHELLY_WS";
      break;

    case GRID_PRI_SHELLY_CHINT:
      if (shellyFresh) return "SHELLY_WS";
      if (chintFresh) return "CHINT_WS";
      break;

    case GRID_PRI_CHINT_ONLY:
      if (chintFresh) return "CHINT_WS";
      break;

    case GRID_PRI_SHELLY_ONLY:
      if (shellyFresh) return "SHELLY_WS";
      break;
  }
  return "NONE";
}

void MQTTManager::onChintWsEvent(WStype_t type, uint8_t *payload, size_t length) {
  switch (type) {
    case WStype_DISCONNECTED:
      _chintWsConnected = false;
      Serial.println("[Chint WS] Disconnected");
      break;

    case WStype_CONNECTED:
      _chintWsConnected = true;
      Serial.printf("[Chint WS] Connected to ws://%s:%d%s\n", _chintIp.c_str(), CHINT_XIAO_WS_PORT, CHINT_XIAO_WS_PATH);
      break;

    case WStype_TEXT:
      handleChintMessage(payload, length);
      break;

    case WStype_ERROR:
      Serial.println("[Chint WS] Communication error");
      break;

    default:
      break;
  }
}

void MQTTManager::handleChintMessage(const uint8_t *payload, size_t length) {
  StaticJsonDocument<768> doc;
  DeserializationError err = deserializeJson(doc, payload, length);
  if (err) return;

  float v = 0.0f;
  float a = 0.0f;
  float raw_w = 0.0f;
  float pf = 1.0f;
  float f = 50.0f;
  float imp_kwh = -1.0f;
  float exp_kwh = -1.0f;
  bool valid = false;

  // Case 1: Clean stream JSON or welcome packet from Chint Master
  if (doc.containsKey("type")) {
    const char *type = doc["type"] | "";
    if (strcmp(type, "telemetry") == 0 || strcmp(type, "welcome") == 0) {
      bool meterOnline = doc["meter_online"] | true;
      if (!meterOnline) {
        // Physical Chint meter not responding to Xiao; ignore to let fallback take over
        return;
      }
      if (doc.containsKey("volts") && doc.containsKey("watts")) {
        v = doc["volts"].as<float>();
        raw_w = doc["watts"].as<float>();
        a = doc["amps"] | 0.0f;
        pf = doc["pf"] | 1.0f;
        f = doc["freq"] | 50.0f;
        imp_kwh = doc["import_kwh"] | -1.0f;
        exp_kwh = doc["export_kwh"] | -1.0f;
        valid = (!std::isnan(v) && v >= 180.0f && v <= 280.0f);
      }
    }
  }
  // Case 2: Shelly RPC NotifyStatus format emitted by Chint Master
  else if (doc.containsKey("params")) {
    JsonObject par = doc["params"];
    if (par.containsKey("em1:0")) {
      JsonObject em1 = par["em1:0"];
      v = em1["voltage"] | 0.0f;
      raw_w = em1["act_power"] | 0.0f;
      a = em1["current"] | 0.0f;
      pf = em1["pf"] | 1.0f;
      f = em1["freq"] | 50.0f;
      valid = (!std::isnan(v) && v >= 180.0f && v <= 280.0f);
    }
    if (par.containsKey("em1data:0")) {
      JsonObject d = par["em1data:0"];
      if (d.containsKey("total_act_energy")) imp_kwh = d["total_act_energy"].as<float>() / 1000.0f;
      if (d.containsKey("total_act_ret_energy")) exp_kwh = d["total_act_ret_energy"].as<float>() / 1000.0f;
    }
  }

  if (!valid) return;

  uint32_t now = millis();
  _lastChintMsgTime = now;

  // Sync energy accumulators if available.
  // Guard: if Chint was offline and Shelly was tracking energy, only accept
  // Chint's reading if it is within 1 kWh of the current stored value to
  // prevent large jumps from corrupting the accumulator (#7).
  if (imp_kwh >= 0.0f || exp_kwh >= 0.0f) {
    MeterTelemetry cur = MeterState::getGrid();
    bool wasOffline = !isChintRecent(CHINT_TIMEOUT_MS + 500); // just reconnected?
    if (wasOffline) {
      float impDelta = (imp_kwh >= 0.0f) ? std::fabs(imp_kwh - cur.import_kwh) : 0.0f;
      float expDelta = (exp_kwh >= 0.0f) ? std::fabs(exp_kwh - cur.export_kwh) : 0.0f;
      if (impDelta > 1.0f || expDelta > 1.0f) {
        Serial.printf("[Chint Energy] Divergence detected after reconnect: "
                      "stored=%.2f/%.2f chint=%.2f/%.2f – ignoring this reading.\n",
                      cur.import_kwh, cur.export_kwh, imp_kwh, exp_kwh);
        // Accept power data but skip energy update this cycle
        imp_kwh = -1.0f;
        exp_kwh = -1.0f;
      }
    }
    if (imp_kwh >= 0.0f || exp_kwh >= 0.0f) {
      MeterState::updateGridEnergy(imp_kwh, exp_kwh);
    }
  }

  if (!canChintUpdateGrid()) {
    return;
  }

  _lastGridMsgTime = now;

  if (_gridWatchdogTriggered || !_modbusServer.isEnabled()) {
    Serial.println("[Chint WS Fallback] Valid telemetry received, restoring RS485.");
    _modbusServer.setEnabled(true);
    _gridWatchdogTriggered = false;
  }

  // Invert sign for Hoymiles zero-export control loop:
  // Grid import is negative kW, export is positive kW (consistent with Shelly & MeterState)
  float p_kw = 0.0f;
  if (!std::isnan(raw_w) && raw_w > -15000.0f && raw_w < 15000.0f) {
    float filtered_w = _gridFilter.apply(raw_w);
    p_kw = (filtered_w / 1000.0f) * -1.0f;
  }

  float s_kw = 0.0f;
  float q_kvar = 0.0f;
  if (v > 0.0f && a >= 0.0f && a < 250.0f) {
    s_kw = (v * a) / 1000.0f;
    float p_abs = std::fabs(p_kw);
    if (s_kw > p_abs) {
      q_kvar = std::sqrt(s_kw * s_kw - p_abs * p_abs);
    }
  }

  if (std::isnan(pf) || pf < -1.0f || pf > 1.0f) pf = 1.0f;
  if (std::isnan(f) || f < 45.0f || f > 55.0f) f = 50.0f;

  MeterState::updateGridRealtime(v, a, p_kw, s_kw, q_kvar, pf, f);
}

void MQTTManager::onShellyWsEvent(WStype_t type, uint8_t *payload, size_t length) {
  switch (type) {
    case WStype_DISCONNECTED:
      _shellyWsConnected = false;
      Serial.println("[Shelly WS] Disconnected");
      break;

    case WStype_CONNECTED:
      _shellyWsConnected = true;
      Serial.printf("[Shelly WS] Connected to ws://%s:%d%s\n", SHELLY_IP, SHELLY_WS_PORT, SHELLY_WS_PATH);
      // Immediately request full status
      _wsShelly.sendTXT("{\"id\":10,\"src\":\"esp32\",\"method\":\"Shelly.GetStatus\"}");
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
      _lastShellyMsgTime = millis();
      if (canShellyUpdateGrid()) {
        _lastGridMsgTime = millis();
        if (_gridWatchdogTriggered || !_modbusServer.isEnabled()) {
          Serial.println("[Shelly WS Fallback] Valid voltage received, restoring RS485.");
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
            float filtered_w = _gridFilter.apply(raw_p);
            // Invert sign for Hoymiles: Grid import is negative, export is positive
            p_kw = (filtered_w / 1000.0f) * -1.0f;
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

        MeterState::updateGridRealtime(v, c, p_kw, s_kw, q_kvar, pf, f);
      }
    }
  }

  // 2. Process Energy Accumulators
  if (!em1data.isNull()) {
    bool chintActive = isChintRecent(CHINT_TIMEOUT_MS);
    if (!chintActive) {
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
    _lastPvMsgTime = millis();
    _pvWatchdogTriggered = false;
    MeterState::updatePvYield(y);
  }
}

void MQTTManager::checkWatchdogs() {
  uint32_t now = millis();


  // 1. Grid watchdog (10 seconds timeout without ANY valid source)
  if (!_gridWatchdogTriggered && (now - _lastGridMsgTime > (WATCHDOG_TIMEOUT_SEC * 1000))) {
    Serial.println("[FAILSAFE] Grid data timeout (both Chint and Shelly unavailable)! Disabling RS485 to force Hoymiles safe state.");
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

void MQTTManager::checkSourceTransition() {
  const char *current = getActiveGridSource();
  if (strcmp(current, _lastKnownSource) != 0) {
    Serial.printf("[Source] Grid source changed: %s -> %s\n", _lastKnownSource, current);
    SourceEvent &ev = _sourceEvents[_sourceEventHead];
    ev.timestamp_s = millis() / 1000;
    strncpy(ev.from, _lastKnownSource, sizeof(ev.from) - 1);
    ev.from[sizeof(ev.from) - 1] = '\0';
    strncpy(ev.to, current, sizeof(ev.to) - 1);
    ev.to[sizeof(ev.to) - 1] = '\0';
    _sourceEventHead = (_sourceEventHead + 1) % SOURCE_EVENT_HISTORY;
    if (_sourceEventCount < SOURCE_EVENT_HISTORY) _sourceEventCount++;
    strncpy(_lastKnownSource, current, sizeof(_lastKnownSource) - 1);
    _lastKnownSource[sizeof(_lastKnownSource) - 1] = '\0';

    // Reset filter on source handoff so new source starts fresh immediately
    _gridFilter.reset();
  }
}

void MQTTManager::handleEspNowPv(float watts, float yield_kwh) {
  if (std::isnan(watts) || watts < 0.0f || watts > 25000.0f) return;
  if (std::isnan(yield_kwh) || yield_kwh < 0.0f) return;

  MeterState::updatePvPower(watts);
  MeterState::updatePvYield(yield_kwh);
  _lastPvMsgTime = millis();
  _pvWatchdogTriggered = false;
}

