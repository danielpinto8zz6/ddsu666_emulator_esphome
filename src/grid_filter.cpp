#include "grid_filter.h"

GridFilter::GridFilter()
  : _filteredWatts(NAN),
    _lastUpdateTime(0) {
}

void GridFilter::reset() {
  _filteredWatts = NAN;
  _lastUpdateTime = 0;
}

float GridFilter::apply(float rawWatts) {
  uint32_t now = millis();

  // First sample initialization
  if (std::isnan(_filteredWatts) || _lastUpdateTime == 0) {
    _filteredWatts = rawWatts;
    _lastUpdateTime = now;
    return _filteredWatts;
  }

  uint32_t dt_ms = now - _lastUpdateTime;
  _lastUpdateTime = now;

  // Reconnection or communication gaps (> 5 seconds): snap immediately
  if (dt_ms > 5000) {
    _filteredWatts = rawWatts;
    return _filteredWatts;
  }

  float delta = std::fabs(rawWatts - _filteredWatts);

  // Zone 4: Heavy appliance step (>= 200W): instant 0ms tracking
  if (delta >= ZONE3_FAST_STEP_THRESHOLD_W) {
    _filteredWatts = rawWatts;
    return _filteredWatts;
  }

  // Multi-Zone Precision Damping Curve:
  // - Zone 1 (<= 35W): Zero-export equilibrium band. Strong damping (tau = 2000ms)
  //   attenuates 14.5 Hz AC measurement noise by > 96% and stops inverter PID hunting.
  // - Zone 2 (35W .. 120W): Normal household drifts. Smoothly scales tau from 2000ms to 400ms.
  // - Zone 3 (120W .. 200W): Fast response (tau 400ms down to 100ms) for mid-size loads.
  float tau_ms;
  if (delta <= ZONE1_EQUILIBRIUM_THRESHOLD_W) {
    tau_ms = TAU_ZONE1_EQUILIBRIUM_MS;
  } else if (delta < ZONE2_DRIFT_THRESHOLD_W) {
    float k = (delta - ZONE1_EQUILIBRIUM_THRESHOLD_W) / (ZONE2_DRIFT_THRESHOLD_W - ZONE1_EQUILIBRIUM_THRESHOLD_W);
    tau_ms = TAU_ZONE1_EQUILIBRIUM_MS - k * (TAU_ZONE1_EQUILIBRIUM_MS - TAU_ZONE2_FAST_MS);
  } else {
    float k = (delta - ZONE2_DRIFT_THRESHOLD_W) / (ZONE3_FAST_STEP_THRESHOLD_W - ZONE2_DRIFT_THRESHOLD_W);
    tau_ms = TAU_ZONE2_FAST_MS - k * (TAU_ZONE2_FAST_MS - TAU_ZONE3_FASTEST_MS);
  }

  // Continuous exponential moving average coefficient
  float alpha = 1.0f - std::exp(-((float)dt_ms) / tau_ms);
  if (alpha > 1.0f) alpha = 1.0f;
  if (alpha < 0.02f) alpha = 0.02f;

  _filteredWatts += alpha * (rawWatts - _filteredWatts);

  return _filteredWatts;
}
