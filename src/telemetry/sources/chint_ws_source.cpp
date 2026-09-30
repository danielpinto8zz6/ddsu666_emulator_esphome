#include "chint_ws_source.h"

ChintWsSource::ChintWsSource(const String &ip, uint16_t port, const char *path)
  : _ip(ip), _port(port), _path(path) {
}

void ChintWsSource::begin(ITelemetrySink *sink) {
  _sink = sink;
  Serial.printf("[Chint WS] Initializing connection to ws://%s:%d%s\n", _ip.c_str(), _port, _path);
  _ws.begin(_ip.c_str(), _port, _path);
  _ws.onEvent([this](WStype_t type, uint8_t *payload, size_t length) {
    this->onWsEvent(type, payload, length);
  });
  _ws.setReconnectInterval(2000);
  _ws.enableHeartbeat(5000, 2000, 2);
}

void ChintWsSource::setIp(const String &ip) {
  if (_ip != ip) {
    _ip = ip;
    _connected = false;
    _ws.disconnect();
    _ws.begin(_ip.c_str(), _port, _path);
  }
}

void ChintWsSource::loop() {
  _ws.loop();
}

void ChintWsSource::onWsEvent(WStype_t type, uint8_t *payload, size_t length) {
  switch (type) {
    case WStype_DISCONNECTED:
      _connected = false;
      Serial.println("[Chint WS] Disconnected");
      break;

    case WStype_CONNECTED:
      _connected = true;
      Serial.printf("[Chint WS] Connected to ws://%s:%d%s\n", _ip.c_str(), _port, _path);
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
      bool meterOnline = doc["meter_online"] | true;
      if (!meterOnline) {
        // Physical Chint meter not responding to Xiao; ignore to let fallback take over
        return;
      }
      if (doc.containsKey("volts") && doc.containsKey("watts")) {
        meas.voltage = doc["volts"].as<float>();
        meas.active_power_w = doc["watts"].as<float>();
        meas.current = doc["amps"] | 0.0f;
        meas.power_factor = doc["pf"] | 1.0f;
        meas.frequency = doc["freq"] | 50.0f;
        meas.import_kwh = doc["import_kwh"] | -1.0f;
        meas.export_kwh = doc["export_kwh"] | -1.0f;
        valid = (!std::isnan(meas.voltage) && meas.voltage >= 180.0f && meas.voltage <= 280.0f);
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

  if (!valid) return;

  _lastMsgTime = millis();

  if (_sink) {
    _sink->reportGridMeasurement(getId(), meas);
  }
}
