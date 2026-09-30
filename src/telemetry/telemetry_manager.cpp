#include "telemetry_manager.h"
#include <cmath>
#include <cstring>

TelemetryManager::TelemetryManager(ModbusRTUServer &modbusServer,
                                   IPowerFilter *filter,
                                   IGridArbitrator *arbitrator)
  : _modbusServer(modbusServer),
    _filter(filter ? filter : &_defaultFilter),
    _arbitrator(arbitrator ? arbitrator : &_defaultArbitrator) {
  strncpy(_lastKnownSource, "NONE", sizeof(_lastKnownSource));
}

void TelemetryManager::setFilter(IPowerFilter *filter) {
  _filter = filter ? filter : &_defaultFilter;
}

void TelemetryManager::setArbitrator(IGridArbitrator *arbitrator) {
  _arbitrator = arbitrator ? arbitrator : &_defaultArbitrator;
}

void TelemetryManager::registerSource(ITelemetrySource *source) {
  if (source) {
    _sources.push_back(source);
    Serial.printf("[Telemetry] Registered source: %s (%s)\n", source->getId(), source->getName());
  }
}

ITelemetrySource* TelemetryManager::getSource(const char *id) const {
  for (auto *src : _sources) {
    if (src && strcmp(src->getId(), id) == 0) {
      return src;
    }
  }
  return nullptr;
}

void TelemetryManager::begin() {
  AppConfig &cfg = AppConfig::getInstance();
  cfg.begin();

  _arbitrator->setMode((GridPriorityMode)cfg.getGridPriority());

  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.setSleep(false);
  WiFi.setTxPower(WIFI_POWER_19_5dBm);

  connectWiFi();

  // Initialize all registered sources with this hub as the destination sink
  for (auto *src : _sources) {
    src->begin(this);
  }

  _lastGridMsgTime = millis();
  _lastPvMsgTime = millis();
}

void TelemetryManager::connectWiFi() {
  if (WiFi.status() == WL_CONNECTED) return;
  const SystemSettings &s = AppConfig::getInstance().get();
  Serial.printf("[WiFi] Connecting to %s", s.wifi_ssid.c_str());
  WiFi.begin(s.wifi_ssid.c_str(), s.wifi_pass.c_str());
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 10000) {
    delay(200);
    Serial.print(".");
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("\n[WiFi] Connected! IP: %s | RSSI: %d dBm\n",
                  WiFi.localIP().toString().c_str(), WiFi.RSSI());
  } else {
    Serial.println("\n[WiFi] Connection timed out; continuing offline.");
  }
}

void TelemetryManager::loop() {
  if (WiFi.status() != WL_CONNECTED) {
    static uint32_t lastCheck = 0;
    if (millis() - lastCheck > 10000) {
      lastCheck = millis();
      connectWiFi();
    }
  }

  // Generic loop over all registered telemetry sources (OCP)
  for (auto *src : _sources) {
    src->loop();
  }

  checkWatchdogs();
  checkSourceTransition();
}

void TelemetryManager::setGridPriority(GridPriorityMode mode) {
  _arbitrator->setMode(mode);
  AppConfig::getInstance().setGridPriority((uint8_t)mode);
  Serial.printf("[Priority] Grid Priority Mode set to %u: %s\n", (unsigned)mode, getGridPriorityDescription());
}

GridPriorityMode TelemetryManager::getGridPriority() const {
  return _arbitrator->getMode();
}

const char* TelemetryManager::getGridPriorityDescription() const {
  return _arbitrator->getDescription();
}

const char* TelemetryManager::getActiveGridSource() const {
  return _arbitrator->getActiveSource(_sources);
}

bool TelemetryManager::canSourceUpdateGrid(const char *sourceId) const {
  return _arbitrator->canUpdate(sourceId, _sources);
}

void TelemetryManager::reportGridMeasurement(const char *sourceId, const GridMeasurement &meas) {
  // 1. Sync energy accumulators if present
  if (meas.import_kwh >= 0.0f || meas.export_kwh >= 0.0f) {
    bool isChint = (strcmp(sourceId, "CHINT_WS") == 0);
    float imp = meas.import_kwh;
    float exp = meas.export_kwh;

    // Guard: divergence check after Chint reconnect
    ITelemetrySource *chint = getSource("CHINT_WS");
    if (isChint && chint && !chint->isRecent(CHINT_TIMEOUT_MS + 500)) {
      MeterTelemetry cur = MeterState::getGrid();
      float impDelta = (imp >= 0.0f) ? std::fabs(imp - cur.import_kwh) : 0.0f;
      float expDelta = (exp >= 0.0f) ? std::fabs(exp - cur.export_kwh) : 0.0f;
      if (impDelta > 1.0f || expDelta > 1.0f) {
        Serial.printf("[Chint Energy] Divergence detected after reconnect: stored=%.2f/%.2f chint=%.2f/%.2f\n",
                      cur.import_kwh, cur.export_kwh, imp, exp);
        imp = -1.0f;
        exp = -1.0f;
      }
    }

    if (imp >= 0.0f || exp >= 0.0f) {
      MeterState::updateGridEnergy(imp, exp);
    }
  }

  // 2. Check arbitration priority
  if (!canSourceUpdateGrid(sourceId)) {
    return;
  }

  // Must have valid AC voltage and active power
  if (std::isnan(meas.voltage) || meas.voltage < 150.0f || std::isnan(meas.active_power_w)) {
    return;
  }

  _lastGridMsgTime = millis();

  // Restore RS485 if watchdog or failsafe was active
  if (_gridWatchdogTriggered || !_modbusServer.isEnabled()) {
    Serial.printf("[%s] Valid grid telemetry received, restoring RS485.\n", sourceId);
    _modbusServer.setEnabled(true);
    _gridWatchdogTriggered = false;
  }

  // 3. Apply Multi-Zone Signal Conditioning Filter
  float filtered_w = _filter->apply(meas.active_power_w);

  // 4. Invert sign for Hoymiles zero-export: import is negative kW, export is positive kW
  float p_kw = (filtered_w / 1000.0f) * -1.0f;

  // 5. Calculate apparent & reactive power if not provided
  float s_kw = 0.0f;
  float q_kvar = 0.0f;
  float a = !std::isnan(meas.current) && meas.current >= 0.0f ? meas.current : 0.0f;

  if (!std::isnan(meas.apparent_power_va) && meas.apparent_power_va >= 0.0f) {
    s_kw = meas.apparent_power_va / 1000.0f;
  } else if (meas.voltage > 0.0f && a >= 0.0f && a < 250.0f) {
    s_kw = (meas.voltage * a) / 1000.0f;
  }

  float p_abs = std::fabs(p_kw);
  if (!std::isnan(meas.reactive_power_var)) {
    q_kvar = meas.reactive_power_var / 1000.0f;
  } else if (s_kw > p_abs) {
    q_kvar = std::sqrt(s_kw * s_kw - p_abs * p_abs);
  }

  float pf = (!std::isnan(meas.power_factor) && meas.power_factor >= -1.0f && meas.power_factor <= 1.0f)
             ? meas.power_factor : 1.0f;
  float f  = (!std::isnan(meas.frequency) && meas.frequency > 40.0f && meas.frequency < 65.0f)
             ? meas.frequency : 50.0f;

  MeterState::updateGridRealtime(meas.voltage, a, p_kw, s_kw, q_kvar, pf, f);
}

void TelemetryManager::reportPvMeasurement(const char *sourceId, const PvMeasurement &meas) {
  if (meas.isValid()) {
    if (!std::isnan(meas.active_power_w)) {
      MeterState::updatePvPower(meas.active_power_w);
    }
    if (meas.yield_kwh >= 0.0f) {
      MeterState::updatePvYield(meas.yield_kwh);
    }
    _lastPvMsgTime = millis();
    _pvWatchdogTriggered = false;
  }
}

void TelemetryManager::checkWatchdogs() {
  uint32_t now = millis();

  // Grid Watchdog (10s timeout)
  if (!_gridWatchdogTriggered && (now - _lastGridMsgTime > (WATCHDOG_TIMEOUT_SEC * 1000))) {
    Serial.println("[FAILSAFE] Grid data timeout (all sources offline)! Disabling RS485 for Hoymiles safe state.");
    _modbusServer.setEnabled(false);
    _gridWatchdogTriggered = true;
  }

  // PV Watchdog (10s timeout)
  if (!_pvWatchdogTriggered && (now - _lastPvMsgTime > (WATCHDOG_TIMEOUT_SEC * 1000))) {
    Serial.println("[FAILSAFE] PV data timeout! Zeroing PV power.");
    MeterState::zeroPvPower();
    _pvWatchdogTriggered = true;
  }
}

void TelemetryManager::checkSourceTransition() {
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
    _filter->reset();
  }
}
