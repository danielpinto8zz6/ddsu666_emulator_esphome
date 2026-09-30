#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <Preferences.h>
#include "telemetry_types.h"
#include "telemetry_source.h"
#include "sources/chint_ws_source.h"
#include "sources/shelly_ws_source.h"
#include "sources/opendtu_mqtt_source.h"
#include "sources/espnow_pv_source.h"
#include "grid_filter.h"
#include "modbus_rtu.h"
#include "meter_data.h"

/**
 * @brief Central Telemetry Manager & Ingestion Hub.
 *
 * Implements ITelemetrySink to receive normalized measurements from any source
 * (WebSocket, MQTT, ESP-NOW), coordinates priority and failover, applies the
 * Multi-Zone GridFilter, and updates the thread-safe Modbus MeterState.
 */
class TelemetryManager : public ITelemetrySink {
public:
  TelemetryManager(ModbusRTUServer &modbusServer);

  void begin();
  void loop();

  // ITelemetrySink implementation
  void reportGridMeasurement(const char *sourceId, const GridMeasurement &meas) override;
  void reportPvMeasurement(const char *sourceId, const PvMeasurement &meas) override;

  // Network & Priority Status
  bool isWiFiConnected() const { return WiFi.status() == WL_CONNECTED; }
  int8_t getWiFiRssi() const { return WiFi.RSSI(); }

  void setGridPriority(GridPriorityMode mode);
  GridPriorityMode getGridPriority() const { return _gridPriority; }
  const char* getGridPriorityDescription() const;
  const char* getActiveGridSource() const;

  // Source Accessors
  ChintWsSource& getChintSource() { return _chintSource; }
  ShellyWsSource& getShellySource() { return _shellySource; }
  OpenDtuMqttSource& getOpenDtuSource() { return _opendtuSource; }
  EspNowPvSource& getEspNowSource() { return _espnowSource; }

  // Freshness & Diagnostics
  uint32_t getLastGridMsgTime() const { return _lastGridMsgTime; }
  uint32_t getLastPvMsgTime() const { return _lastPvMsgTime; }
  bool isGridRecent(uint32_t timeoutMs = 4000) const {
    return (_lastGridMsgTime != 0) && (millis() - _lastGridMsgTime <= timeoutMs);
  }

  // Source transition history (for WebUI)
  uint8_t getSourceEventCount() const { return _sourceEventCount; }
  uint8_t getSourceEventHead() const { return _sourceEventHead; }
  const SourceEvent* getSourceEvents() const { return _sourceEvents; }

private:
  ModbusRTUServer &_modbusServer;

  // Concrete Technology Sources
  ChintWsSource _chintSource;
  ShellyWsSource _shellySource;
  OpenDtuMqttSource _opendtuSource;
  EspNowPvSource _espnowSource;

  // Multi-Zone Signal Conditioning Filter
  GridFilter _gridFilter;

  // Arbitration & State
  GridPriorityMode _gridPriority = DEFAULT_GRID_PRIORITY_MODE;
  uint32_t _lastGridMsgTime = 0;
  uint32_t _lastPvMsgTime = 0;
  bool _gridWatchdogTriggered = false;
  bool _pvWatchdogTriggered = false;

  // Transition History Ring Buffer
  SourceEvent _sourceEvents[SOURCE_EVENT_HISTORY];
  uint8_t _sourceEventHead = 0;
  uint8_t _sourceEventCount = 0;
  char _lastKnownSource[16];

  // Config settings
  String _wifiSsid;
  String _wifiPassword;

  void connectWiFi();
  void checkWatchdogs();
  void checkSourceTransition();
  bool canSourceUpdateGrid(const char *sourceId) const;
};
