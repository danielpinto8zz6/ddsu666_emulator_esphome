#pragma once

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/portmacro.h>
#include <driver/uart.h>
#include <Preferences.h>
#include "interfaces.h"

/**
 * @brief High-speed FreeRTOS Modbus RTU Master for Chint DDSU666 energy meters.
 * Implements IChintMeterReader interface (LSP / DIP).
 */
class DDSU666Master : public IChintMeterReader {
public:
  DDSU666Master();
  ~DDSU666Master() override = default;

  void begin(uint8_t slaveId = 1) override;
  void loop() override; // Runs on dedicated Core 1 FreeRTOS task

  // Thread-safe accessors
  ChintTelemetry getTelemetry() override;
  ModbusMasterStats getStats() override;

  void setSlaveId(uint8_t slaveId) override;
  uint8_t getSlaveId() const override { return _slaveId; }

  void onData(OnDataCallback cb) override { _dataCb = cb; }

private:
  uint8_t _slaveId;
  OnDataCallback _dataCb;
  Preferences _prefs;

  ChintTelemetry _telemetry;
  ModbusMasterStats _stats;
  portMUX_TYPE _spinlock;

  uint32_t _lastEnergyPoll;
  uint32_t _lastRateCalcTime;
  uint32_t _queriesAtLastCalc;

  // Modbus RTU Low-Level helpers
  bool queryRegisters(uint16_t startAddress, uint16_t count, uint8_t *responseBuffer, size_t &responseLen, float &latencyMs);
  uint16_t calculateCRC(const uint8_t *buffer, size_t length);
  float parseIEEE754(const uint8_t *p);
};
