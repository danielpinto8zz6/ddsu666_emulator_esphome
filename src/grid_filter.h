#pragma once

#include <Arduino.h>
#include <cmath>

/**
 * @brief Strategy interface for active power signal conditioning (Open/Closed Principle).
 * Allows plugging in different smoothing algorithms (e.g. Multi-Zone, EMA, PassThrough).
 */
class IPowerFilter {
public:
  virtual ~IPowerFilter() = default;
  virtual float apply(float rawWatts) = 0;
  virtual void reset() = 0;
  virtual float getFilteredWatts() const = 0;
  virtual bool isInitialized() const = 0;
};

/**
 * @brief Zero-latency pass-through filter (returns raw meter readings unaltered).
 */
class PassThroughFilter : public IPowerFilter {
public:
  float apply(float rawWatts) override {
    _rawWatts = rawWatts;
    _initialized = true;
    return rawWatts;
  }
  void reset() override {
    _rawWatts = 0.0f;
    _initialized = false;
  }
  float getFilteredWatts() const override { return _rawWatts; }
  bool isInitialized() const override { return _initialized; }

private:
  float _rawWatts = 0.0f;
  bool _initialized = false;
};

/**
 * @brief Multi-Zone Precision Damping Filter for Zero-Export Inverter Control.
 *
 * Physical meters (such as Chint DDSU666 or Shelly Pro EM) measure instantaneous active
 * power at high frequency (14.5 Hz). Due to household reactive currents (switch-mode power
 * supplies, LED drivers, capacitors), the apparent power (VA) has tiny phase jitter
 * causing instantaneous active power to fluctuate by +/- 30W to 50W.
 *
 * Feeding raw high-frequency noise into a solar inverter's closed-loop zero-export controller
 * causes PID hunting and thermal stress on switching transistors, because the inverter's
 * physical power stage reacts on a ~1 second timeframe and cannot keep up with 14.5 Hz noise.
 *
 * GridFilter provides non-linear multi-zone damping:
 * - Zone 1 (<= 35W): Zero-export equilibrium band. Strong damping (tau = 2000ms)
 *   attenuates 14.5 Hz AC measurement noise by > 96% and prevents inverter PID hunting.
 * - Zone 2 (35W .. 120W): Normal household drifts (lighting, TV). Smoothly scales tau from 2000ms to 400ms.
 * - Zone 3 (120W .. 200W): Fast response for moderate steps (tau 400ms down to 100ms).
 * - Zone 4 (>= 200W): Instant 0ms bypass for heavy appliances (kettles, ovens, heaters).
 */
class GridFilter : public IPowerFilter {
public:
  static constexpr float ZONE1_EQUILIBRIUM_THRESHOLD_W = 35.0f;
  static constexpr float ZONE2_DRIFT_THRESHOLD_W       = 120.0f;
  static constexpr float ZONE3_FAST_STEP_THRESHOLD_W   = 200.0f;

  static constexpr float TAU_ZONE1_EQUILIBRIUM_MS      = 2000.0f;
  static constexpr float TAU_ZONE2_FAST_MS             = 400.0f;
  static constexpr float TAU_ZONE3_FASTEST_MS          = 100.0f;

  GridFilter();

  /**
   * @brief Apply adaptive multi-zone filtering to a new raw grid power reading.
   * @param rawWatts Raw active power in Watts from meter.
   * @return Filtered active power in Watts.
   */
  float apply(float rawWatts) override;

  /**
   * @brief Reset filter state immediately (used during source transitions or reconnects).
   */
  void reset() override;

  /**
   * @brief Get the current filtered watts value.
   */
  float getFilteredWatts() const override { return _filteredWatts; }

  /**
   * @brief Check if filter is initialized with at least one valid reading.
   */
  bool isInitialized() const override { return !std::isnan(_filteredWatts) && _lastUpdateTime != 0; }

private:
  float _filteredWatts;
  uint32_t _lastUpdateTime;
};

