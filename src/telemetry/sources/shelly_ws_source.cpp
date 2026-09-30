#include "shelly_ws_source.h"

ShellyWsSource::ShellyWsSource(const String &ip, uint16_t port, const char *path)
  : _ip(ip), _port(port), _path(path) {
}

void ShellyWsSource::begin(ITelemetrySink *sink) {
  _sink = sink;
  Serial.printf("[Shelly WS] Initializing connection to ws://%s:%d%s\n", _ip.c_str(), _port, _path);
  _ws.begin(_ip.c_str(), _port, _path);
  _ws.onEvent([this](WStype_t type, uint8_t *payload, size_t length) {
    this->onWsEvent(type, payload, length);
  });
  _ws.setReconnectInterval(2000);
  _ws.enableHeartbeat(10000, 3000, 2);
}

void ShellyWsSource::setIp(const String &ip) {
  if (_ip != ip) {
    _ip = ip;
    _connected = false;
    _ws.disconnect();
    _ws.begin(_ip.c_str(), _port, _path);
  }
}

void ShellyWsSource::loop() {
  _ws.loop();

  uint32_t now = millis();

  // Polling Strategy:
  // Shelly pushes updates automatically via WebSocket NotifyStatus.
  // We only send a poll fallback if Shelly has been quiet for > 2000ms.
  if (_connected) {
    if (now - _lastPollTime >= 2500 && (now - _lastMsgTime >= 2000)) {
      _lastPollTime = now;
      _ws.sendTXT("{\"id\":1,\"src\":\"esp32\",\"method\":\"EM1.GetStatus\",\"params\":{\"id\":0}}");
    }

    // 60-second polling for energy accumulators from Shelly
    if (_energyPollingEnabled && (now - _lastEnergyPollTime >= 60000)) {
      _lastEnergyPollTime = now;
      _ws.sendTXT("{\"id\":2,\"src\":\"esp32\",\"method\":\"EM1Data.GetStatus\",\"params\":{\"id\":0}}");
    }
  }
}

void ShellyWsSource::onWsEvent(WStype_t type, uint8_t *payload, size_t length) {
  switch (type) {
    case WStype_DISCONNECTED:
      _connected = false;
      Serial.println("[Shelly WS] Disconnected");
      break;

    case WStype_CONNECTED:
      _connected = true;
      Serial.printf("[Shelly WS] Connected to ws://%s:%d%s\n", _ip.c_str(), _port, _path);
      // Immediately request status on connect
      _ws.sendTXT("{\"id\":1,\"src\":\"esp32\",\"method\":\"EM1.GetStatus\",\"params\":{\"id\":0}}");
      _ws.sendTXT("{\"id\":2,\"src\":\"esp32\",\"method\":\"EM1Data.GetStatus\",\"params\":{\"id\":0}}");
      break;

    case WStype_TEXT:
      handleMessage(payload, length);
      break;

    case WStype_ERROR:
      Serial.println("[Shelly WS] Communication error");
      break;

    default:
      break;
  }
}

void ShellyWsSource::handleMessage(const uint8_t *payload, size_t length) {
  StaticJsonDocument<1024> doc;
  DeserializationError err = deserializeJson(doc, payload, length);
  if (err) return;

  JsonObject em1;
  JsonObject em1data;

  if (doc.containsKey("result")) {
    JsonObject res = doc["result"];
    if (res.containsKey("voltage")) {
      em1 = res;
    } else if (res.containsKey("em1:0")) {
      em1 = res["em1:0"];
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

  GridMeasurement meas;
  bool hasTelemetry = false;

  // Process Real-Time Telemetry
  if (!em1.isNull() && em1.containsKey("voltage")) {
    float v = em1["voltage"].as<float>();
    if (!std::isnan(v) && v >= 180.0f && v <= 280.0f) {
      meas.voltage = v;
      meas.current = em1.containsKey("current") ? em1["current"].as<float>() : 0.0f;
      meas.active_power_w = em1.containsKey("act_power") ? em1["act_power"].as<float>() : 0.0f;
      meas.apparent_power_va = em1.containsKey("aprt_power") ? em1["aprt_power"].as<float>() : NAN;
      meas.power_factor = em1.containsKey("pf") ? em1["pf"].as<float>() : 1.0f;
      meas.frequency = em1.containsKey("freq") ? em1["freq"].as<float>() : 50.0f;
      hasTelemetry = true;
    }
  }

  // Process Energy Accumulators
  if (!em1data.isNull()) {
    if (em1data.containsKey("total_act_energy")) {
      meas.import_kwh = em1data["total_act_energy"].as<float>() / 1000.0f;
    }
    if (em1data.containsKey("total_act_ret_energy")) {
      meas.export_kwh = em1data["total_act_ret_energy"].as<float>() / 1000.0f;
    }
    if (meas.import_kwh >= 0.0f || meas.export_kwh >= 0.0f) {
      hasTelemetry = true;
    }
  }

  if (hasTelemetry) {
    _lastMsgTime = millis();
    if (_sink) {
      _sink->reportGridMeasurement(getId(), meas);
    }
  }
}

void ShellyWsSource::populateHealth(JsonObject &obj) const {
  obj["ws_connected"] = _connected;
  obj["ip"] = _ip;
  float age = getAgeSeconds();
  obj["age_sec"] = (age >= 0.0f) ? std::round(age * 10.0f) / 10.0f : -1.0f;
}

