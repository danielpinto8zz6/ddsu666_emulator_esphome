#pragma once

#include <Arduino.h>
#include <driver/uart.h>
#include "config.h"
#include "meter_data.h"

class ModbusRTUServer {
public:
  ModbusRTUServer();

  void begin();
  void setEnabled(bool enabled);
  bool isEnabled() const { return _enabled; }
  void handle();

  uint32_t getQueryCount() const { return _queryCount; }
  uint32_t getCrcErrorCount() const { return _crcErrorCount; }
  uint32_t getLastQueryTime() const { return _lastQueryTime; }
  float getQueryRateHz();
  bool isInverterActive(uint32_t maxAgeMs = 2500) const {
    return (_lastQueryTime != 0) && (millis() - _lastQueryTime < maxAgeMs);
  }

  // Fast Table-Based CRC16 calculation for Modbus (polynomial 0xA001)
  static uint16_t calculateCRC(const uint8_t *buffer, size_t length);

private:
  QueueHandle_t _uartQueue;
  bool _enabled;
  uint8_t _rxBuffer[256];
  size_t _rxLen;

  uint32_t _queryCount;
  uint32_t _crcErrorCount;
  uint32_t _lastQueryTime;
  uint32_t _lastRateCalcTime;
  uint32_t _queriesAtLastCalc;
  float _queryRateHz;

  void processFrame(const uint8_t *frame, size_t length);
  bool getRegisterValue(uint8_t slaveId, uint16_t regAddress, const MeterTelemetry &telemetry, uint16_t &value) const;
  void sendResponse(const uint8_t *response, size_t length);
  void sendException(uint8_t slaveId, uint8_t functionCode, uint8_t exceptionCode);
};
