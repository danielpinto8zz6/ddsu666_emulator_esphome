#include "chint_ws_source.h"
#include <WiFi.h>

ChintWsSource::ChintWsSource(const String &ip, uint16_t port, const char *path)
  : _configuredHost(ip), _activeIp(""), _port(port), _path(path) {
}

bool ChintWsSource::resolveHost(const String &host, String &outIp) {
  String target = host;
  if (target.isEmpty()) target = "chint-reader";

  // If host is already an IPv4 address, use it directly
  IPAddress parsedIp;
  if (parsedIp.fromString(target)) {
    outIp = target;
    return true;
  }

  if (WiFi.status() != WL_CONNECTED) {
    return false;
  }

  // Handle mDNS names (.local or plain hostname)
  String hostname = target;
  if (hostname.endsWith(".local")) {
    hostname = hostname.substring(0, hostname.length() - 6);
  }

  IPAddress resolved = MDNS.queryHost(hostname.c_str(), 1000);
  if (resolved != INADDR_NONE && resolved != IPAddress(0, 0, 0, 0)) {
    outIp = resolved.toString();
    return true;
  }
  return false;
}

void ChintWsSource::connect() {
  String ipToUse;
  if (resolveHost(_configuredHost, ipToUse)) {
    _activeIp = ipToUse;
  } else {
    IPAddress testIp;
    if (testIp.fromString(_configuredHost)) {
      _activeIp = _configuredHost;
    } else {
      _activeIp = "10.0.0.110"; // Fallback to default transmitter IP while mDNS resolves
    }
  }

  Serial.printf("[Chint WS] Connecting to ws://%s:%d%s (configured: %s)\n",
                _activeIp.c_str(), _port, _path, _configuredHost.c_str());
  _ws.begin(_activeIp.c_str(), _port, _path);
}

void ChintWsSource::begin(ITelemetrySink *sink) {
  _sink = sink;
  _ws.onEvent([this](WStype_t type, uint8_t *payload, size_t length) {
    this->onWsEvent(type, payload, length);
  });
  _ws.setReconnectInterval(2000);
  _ws.enableHeartbeat(5000, 2000, 2);

  connect();
}

void ChintWsSource::setIp(const String &ip) {
  if (_configuredHost != ip) {
    _configuredHost = ip;
    _connected = false;
    _ws.disconnect();
    connect();
  }
}

void ChintWsSource::loop() {
  _ws.loop();

  // If disconnected, periodically attempt mDNS discovery every 5 seconds
  uint32_t now = millis();
  if (!_connected && (now - _lastMdnsAttempt >= 5000)) {
    _lastMdnsAttempt = now;

    // Check if configured host is an mDNS name or if disconnected from static IP,
    // probe for chint-reader on the network
    String target = _configuredHost;
    if (target.endsWith(".local") || target == "chint-reader" || target.isEmpty()) {
      String resolved;
      if (resolveHost(target, resolved) && resolved != _activeIp) {
        Serial.printf("[Chint WS] Discovered chint-reader via mDNS at %s (was %s)\n",
                      resolved.c_str(), _activeIp.c_str());
        _activeIp = resolved;
        _ws.disconnect();
        _ws.begin(_activeIp.c_str(), _port, _path);
      }
    } else {
      // If configured with an IP that is disconnected, also check if chint-reader.local is available
      String resolved;
      if (resolveHost("chint-reader", resolved) && resolved != _activeIp) {
        Serial.printf("[Chint WS] Transmitter relocated via mDNS: %s -> %s\n",
                      _activeIp.c_str(), resolved.c_str());
        _activeIp = resolved;
        _ws.disconnect();
        _ws.begin(_activeIp.c_str(), _port, _path);
      }
    }
  }
}

void ChintWsSource::onWsEvent(WStype_t type, uint8_t *payload, size_t length) {
  switch (type) {
    case WStype_DISCONNECTED:
      _connected = false;
      Serial.println("[Chint WS] Disconnected");
      break;

    case WStype_CONNECTED:
      _connected = true;
      Serial.printf("[Chint WS] Connected to ws://%s:%d%s\n", _activeIp.c_str(), _port, _path);
      break;

    case WStype_TEXT:
      handleMessage(payload, length);
      break;

    case WStype_ERROR:
      Serial.println("[Chint WS] Communication error");
      break;

    default:
      break;
  }
}

void ChintWsSource::handleMessage(const uint8_t *payload, size_t length) {
  StaticJsonDocument<768> doc;
  DeserializationError err = deserializeJson(doc, payload, length);
  if (err) return;

  GridMeasurement meas;
  bool valid = false;

  // Case 1: Clean stream JSON or welcome packet from Chint Master
  if (doc.containsKey("type")) {
    const char *type = doc["type"] | "";
    if (strcmp(type, "telemetry") == 0 || strcmp(type, "welcome") == 0) {
      // 1. Grid Telemetry (Chint DDSU666)
      bool meterOnline = doc["meter_online"] | true;
      if (meterOnline && doc.containsKey("volts") && doc.containsKey("watts")) {
        meas.voltage = doc["volts"].as<float>();
        meas.active_power_w = doc["watts"].as<float>();
        meas.current = doc["amps"] | 0.0f;
        meas.power_factor = doc["pf"] | 1.0f;
        meas.frequency = doc["freq"] | 50.0f;
        meas.import_kwh = doc["import_kwh"] | -1.0f;
        meas.export_kwh = doc["export_kwh"] | -1.0f;
        valid = (!std::isnan(meas.voltage) && meas.voltage >= 180.0f && meas.voltage <= 280.0f);
      }

      // 2. Solar PV Telemetry (Eastron SDM120 multi-drop meter)
      if (doc.containsKey("pv")) {
        JsonObject pvObj = doc["pv"];
        bool pvOnline = pvObj["online"] | (pvObj["meter_online"] | false);
        if (pvOnline && pvObj.containsKey("watts")) {
          PvMeasurement pvMeas;
          float w = pvObj["watts"].as<float>();
          pvMeas.active_power_w = std::fabs(w);
          if (pvMeas.active_power_w < 0.5f) {
            pvMeas.active_power_w = 0.0f;
          }
          pvMeas.yield_kwh = pvObj["yield_kwh"] | -1.0f;
          if (!std::isnan(pvMeas.active_power_w)) {
            _lastPvMsgTime = millis();
            if (_sink) {
              _sink->reportPvMeasurement("EASTRON_WS", pvMeas);
            }
          }
        }
      }
    }
  }
  // Case 2: Shelly RPC NotifyStatus format emitted by Chint Master
  else if (doc.containsKey("params")) {
    JsonObject par = doc["params"];
    if (par.containsKey("em1:0")) {
      JsonObject em1 = par["em1:0"];
      meas.voltage = em1["voltage"] | 0.0f;
      meas.active_power_w = em1["act_power"] | 0.0f;
      meas.current = em1["current"] | 0.0f;
      meas.power_factor = em1["pf"] | 1.0f;
      meas.frequency = em1["freq"] | 50.0f;
      valid = (!std::isnan(meas.voltage) && meas.voltage >= 180.0f && meas.voltage <= 280.0f);
    }
    if (par.containsKey("em1data:0")) {
      JsonObject d = par["em1data:0"];
      if (d.containsKey("total_act_energy")) meas.import_kwh = d["total_act_energy"].as<float>() / 1000.0f;
      if (d.containsKey("total_act_ret_energy")) meas.export_kwh = d["total_act_ret_energy"].as<float>() / 1000.0f;
    }
  }

  if (valid) {
    _lastMsgTime = millis();
    if (_sink) {
      _sink->reportGridMeasurement(getId(), meas);
    }
  }
}

void ChintWsSource::populateHealth(JsonObject &obj) const {
  obj["ws_connected"] = _connected;
  obj["ip"] = getIp();
  obj["configured_host"] = _configuredHost;
  float age = getAgeSeconds();
  obj["age_sec"] = (age >= 0.0f) ? std::round(age * 10.0f) / 10.0f : -1.0f;
  float pvAge = (_lastPvMsgTime != 0) ? (float)(millis() - _lastPvMsgTime) / 1000.0f : -1.0f;
  obj["pv_age_sec"] = (pvAge >= 0.0f) ? std::round(pvAge * 10.0f) / 10.0f : -1.0f;
  obj["pv_recent"] = isPvRecent(5000);
}
