#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <vector>
#include "telemetry_types.h"
#include "telemetry_source.h"
#include "grid_filter.h"
#include "grid_arbitrator.h"
#include "pv_arbitrator.h"
#include "modbus_rtu.h"
#include "meter_data.h"
#include "../config/app_config.h"

/**
 * @brief Central Telemetry Manager & Ingestion Hub (SOLID: Open/Closed & Dependency Inversion).
 *
 * Implements ITelemetrySink to receive normalized measurements from any registered
 * ITelemetrySource provider, coordinates priority arbitration via IGridArbitrator & IPvArbitrator,
 * applies power smoothing via IPowerFilter, and updates the thread-safe Modbus MeterState.
 */
class TelemetryManager : public ITelemetrySink {
public:
  TelemetryManager(ModbusRTUServer &modbusServer,
                   IPowerFilter *filter = nullptr,
                   IGridArbitrator *gridArbitrator = nullptr,
                   IPvArbitrator *pvArbitrator = nullptr);

  // Strategy Injection (DIP / OCP)
  void setFilter(IPowerFilter *filter);
  void setFilterMode(const String &mode);
  const String& getFilterMode() const { return _currentFilterMode; }
  IPowerFilter& getFilter() { return *_filter; }

  void setGridArbitrator(IGridArbitrator *arbitrator);
  IGridArbitrator& getGridArbitrator() { return *_gridArbitrator; }

  void setPvArbitrator(IPvArbitrator *arbitrator);
  IPvArbitrator& getPvArbitrator() { return *_pvArbitrator; }

  // Source Registration (Dependency Injection / OCP)
  void registerSource(ITelemetrySource *source);
  ITelemetrySource* getSource(const char *id) const;
  const std::vector<ITelemetrySource*>& getSources() const { return _sources; }

  void begin();
  void loop();

  // ITelemetrySink implementation
  void reportGridMeasurement(const char *sourceId, const GridMeasurement &meas) override;
  void reportPvMeasurement(const char *sourceId, const PvMeasurement &meas) override;

  // Network & Priority Status
  bool isWiFiConnected() const { return WiFi.status() == WL_CONNECTED; }
  int8_t getWiFiRssi() const { return WiFi.RSSI(); }

  void setGridPriority(GridPriorityMode mode);
  GridPriorityMode getGridPriority() const;
  const char* getGridPriorityDescription() const;
  const char* getActiveGridSource() const;

  void setPvPriority(PvPriorityMode mode);
  PvPriorityMode getPvPriority() const;
  const char* getPvPriorityDescription() const;
  const char* getActivePvSource() const;

  // Freshness & Diagnostics
  uint32_t getLastGridMsgTime() const { return _lastGridMsgTime; }
  uint32_t getLastPvMsgTime() const { return _lastPvMsgTime; }
  bool isGridRecent(uint32_t timeoutMs = 4000) const {
    return (_lastGridMsgTime != 0) && (millis() - _lastGridMsgTime <= timeoutMs);
  }
  bool isPvRecent(uint32_t timeoutMs = 15000) const {
    if (_pvArbitrator && _pvArbitrator->getMode() == PV_PRI_FAKE_50W) return true;
    return (_lastPvMsgTime != 0) && (millis() - _lastPvMsgTime <= timeoutMs);
  }

  // Source transition history (for WebUI)
  uint8_t getSourceEventCount() const { return _sourceEventCount; }
  uint8_t getSourceEventHead() const { return _sourceEventHead; }
  const SourceEvent* getSourceEvents() const { return _sourceEvents; }

private:
  ModbusRTUServer &_modbusServer;
  std::vector<ITelemetrySource*> _sources;

  // Signal Conditioning Filter Strategy
  IPowerFilter *_filter;
  GridFilter _dampedFilter;
  PassThroughFilter _passThroughFilter;
  String _currentFilterMode = "damped";

  // Priority Arbitration Strategies
  IGridArbitrator *_gridArbitrator;
  DefaultGridArbitrator _defaultGridArbitrator;

  IPvArbitrator *_pvArbitrator;
  DefaultPvArbitrator _defaultPvArbitrator;

  // State Timers & Failsafes
  uint32_t _lastGridMsgTime = 0;
  uint32_t _lastPvMsgTime = 0;
  bool _gridWatchdogTriggered = false;
  bool _pvWatchdogTriggered = false;

  // Transition History Ring Buffer
  SourceEvent _sourceEvents[SOURCE_EVENT_HISTORY];
  uint8_t _sourceEventHead = 0;
  uint8_t _sourceEventCount = 0;
  char _lastKnownSource[16];
  char _lastKnownPvSource[16];

  void connectWiFi();
  void checkWatchdogs();
  void checkSourceTransition();
  bool canSourceUpdateGrid(const char *sourceId) const;
  bool canSourceUpdatePv(const char *sourceId) const;
};
