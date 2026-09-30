#pragma once

#include <Arduino.h>
#include <cmath>
#include "config.h"

/**
 * @brief Normalized Grid Measurement from any physical or virtual meter.
 */
struct GridMeasurement {
  float voltage = NAN;            // Line-to-Neutral Voltage (V)
  float current = NAN;            // Current (A)
  float active_power_w = NAN;     // Active Power in Watts (positive = import from grid, negative = export)
  float apparent_power_va = NAN;  // Apparent Power (VA), NAN if unmeasured
  float reactive_power_var = NAN; // Reactive Power (var), NAN if unmeasured
  float power_factor = NAN;       // Power Factor (-1.0 to 1.0)
  float frequency = 50.0f;        // Grid Frequency (Hz)
  float import_kwh = -1.0f;       // Cumulative import energy (kWh), -1 if not provided in this sample
  float export_kwh = -1.0f;       // Cumulative export energy (kWh), -1 if not provided in this sample

  bool isValid() const {
    return !std::isnan(voltage) && voltage >= 150.0f && voltage <= 300.0f &&
           !std::isnan(active_power_w) && active_power_w > -30000.0f && active_power_w < 30000.0f;
  }
};

/**
 * @brief Normalized PV Generation Measurement from DTU or PV meter.
 */
struct PvMeasurement {
  float active_power_w = NAN;     // Solar generation in Watts (>= 0)
  float yield_kwh = -1.0f;        // Cumulative solar yield in kWh, -1 if not provided

  bool isValid() const {
    return !std::isnan(active_power_w) && active_power_w >= 0.0f && active_power_w < 50000.0f;
  }
};

/**
 * @brief Ring-buffer event tracking grid source transitions (for diagnostics & WebUI).
 */
struct SourceEvent {
  uint32_t timestamp_s;
  char from[16];
  char to[16];
};
constexpr uint8_t SOURCE_EVENT_HISTORY = 5;
