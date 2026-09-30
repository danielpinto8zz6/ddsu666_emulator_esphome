#pragma once

#include <Arduino.h>
#include <vector>
#include <cstring>
#include "telemetry_types.h"
#include "telemetry_source.h"

/**
 * @brief Strategy interface for PV Priority Arbitration.
 * Determines which solar telemetry source is active and which source has permission to write PV data.
 */
class IPvArbitrator {
public:
  virtual ~IPvArbitrator() = default;

  virtual void setMode(PvPriorityMode mode) = 0;
  virtual PvPriorityMode getMode() const = 0;
  virtual const char* getDescription() const = 0;

  virtual const char* getActiveSource(const std::vector<ITelemetrySource*> &sources) const = 0;
  virtual bool canUpdate(const char *sourceId, const std::vector<ITelemetrySource*> &sources) const = 0;
};

/**
 * @brief Default PV arbitrator (OpenDTU Solar PV via MQTT or Disabled).
 */
class DefaultPvArbitrator : public IPvArbitrator {
public:
  DefaultPvArbitrator(PvPriorityMode mode = DEFAULT_PV_PRIORITY_MODE);

  void setMode(PvPriorityMode mode) override;
  PvPriorityMode getMode() const override { return _mode; }
  const char* getDescription() const override;

  const char* getActiveSource(const std::vector<ITelemetrySource*> &sources) const override;
  bool canUpdate(const char *sourceId, const std::vector<ITelemetrySource*> &sources) const override;

private:
  PvPriorityMode _mode;
  static ITelemetrySource* findSource(const std::vector<ITelemetrySource*> &sources, const char *id);
};
