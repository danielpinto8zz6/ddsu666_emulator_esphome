#pragma once

#include <Arduino.h>
#include <driver/uart.h>
#include "config.h"
#include "meter_data.h"

class ModbusRTUServer {
public:
  explicit ModbusRTUServer(IMeterDataProvider *dataProvider = nullptr);

  void setDataProvider(IMeterDataProvider *dataProvider);
  void begin();
  void setEnabled(bool enabled);
  bool isEnabled() const { return _enabled; }
  void handle();

  uint32_t getQueryCount() const { return _queryCount; }
  uint32_t getQueryCountGrid() const { return _queryCountGrid; }
  uint32_t getQueryCountPv() const { return _queryCountPv; }
  uint32_t getCrcErrorCount() const { return _crcErrorCount; }
  uint32_t getLastQueryTime() const { return _lastQueryTime; }
  uint32_t getLastGridQueryTime() const { return _lastGridQueryTime; }
  uint32_t getLastPvQueryTime() const { return _lastPvQueryTime; }
  float getQueryRateHz();
  bool isInverterActive(uint32_t maxAgeMs = 2500) const {
    return (_lastQueryTime != 0) && (millis() - _lastQueryTime < maxAgeMs);
  }

  // Fast Table-Based CRC16 calculation for Modbus (polynomial 0xA001)
  static uint16_t calculateCRC(const uint8_t *buffer, size_t length);

  // Eastron SDM120 Gap Polling
  void setTelemetrySink(void *sink) { _telemetrySink = sink; }
  bool isEastronOnline() const { return _eastronOnline; }
  float getEastronWatts() const { return _eastronWatts; }
  uint8_t getEastronSlaveId() const { return _eastronSlaveId; }
  void setEastronSlaveId(uint8_t id) { _eastronSlaveId = id; }

private:
  IMeterDataProvider *_dataProvider;
  MeterStateDataProvider _defaultDataProvider;
  void *_telemetrySink = nullptr;

  QueueHandle_t _uartQueue;
  bool _enabled;
  uint8_t _rxBuffer[256];
  size_t _rxLen;

  uint32_t _queryCount;
  uint32_t _queryCountGrid;
  uint32_t _queryCountPv;
  uint32_t _crcErrorCount;
  uint32_t _lastQueryTime;
  uint32_t _lastGridQueryTime;
  uint32_t _lastPvQueryTime;
  uint32_t _lastRateCalcTime;
  uint32_t _queriesAtLastCalc;
  float _queryRateHz;

  // Eastron State (Legacy / Diagnostic)
  uint8_t _eastronSlaveId = 3;
  bool _eastronOnline = false;
  float _eastronWatts = 0.0f;

  void processFrame(const uint8_t *frame, size_t length);
  bool getRegisterValue(uint8_t slaveId, uint16_t regAddress, const MeterTelemetry &telemetry, uint16_t &value) const;
  void sendResponse(const uint8_t *response, size_t length);
  void sendException(uint8_t slaveId, uint8_t functionCode, uint8_t exceptionCode);
};
