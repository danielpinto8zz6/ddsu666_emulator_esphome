#pragma once

#include <Arduino.h>

/**
 * @brief Telemetry measurements read from Chint DDSU666 Modbus registers.
 */
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

/**
 * @brief Real-time Modbus RTU communication statistics & health metrics.
 */
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
