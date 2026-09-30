#pragma once

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/portmacro.h>
#include <driver/uart.h>

struct ChintTelemetry {
  float voltage;          // Volts (V)
  float current;          // Amps (A)
  float active_power;     // Watts (W) (+ for import, - for export)
  float reactive_power;   // Volt-Amps Reactive (VAR)
  float apparent_power;   // Volt-Amps (VA)
  float power_factor;     // -1.0 to +1.0
  float frequency;        // Hertz (Hz)
  float import_kwh;       // Total active import energy (kWh)
  float export_kwh;       // Total active export energy (kWh)
  uint32_t last_update;   // Millis timestamp of latest valid reading
};

struct ModbusMasterStats {
  uint32_t total_queries;
  uint32_t successful_queries;
  uint32_t timeout_errors;
  uint32_t crc_errors;
  float query_rate_hz;
  float response_latency_ms;
  bool meter_online;
  uint8_t slave_id;
};

class DDSU666Master {
public:
  DDSU666Master();

  void begin(uint8_t slaveId = 1);
  void loop(); // Call in dedicated FreeRTOS task or loop

  // Thread-safe accessors
  ChintTelemetry getTelemetry();
  ModbusMasterStats getStats();

  void setSlaveId(uint8_t slaveId);
  uint8_t getSlaveId() const { return _slaveId; }

  // Callback whenever fresh telemetry is parsed
  typedef void (*OnDataCallback)(const ChintTelemetry &data);
  void onData(OnDataCallback cb) { _dataCb = cb; }

private:
  uint8_t _slaveId;
  OnDataCallback _dataCb;

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
