#pragma once

#include <Arduino.h>
#include <vector>
#include <cstring>
#include "telemetry_types.h"
#include "telemetry_source.h"
#include "config.h"

/**
 * @brief Strategy interface for Grid Priority Arbitration (Single Responsibility & Open/Closed).
 * Determines which telemetry source is active and which source has permission to write grid data.
 */
class IGridArbitrator {
public:
  virtual ~IGridArbitrator() = default;

  virtual void setMode(GridPriorityMode mode) = 0;
  virtual GridPriorityMode getMode() const = 0;
  virtual const char* getDescription() const = 0;

  /**
   * @brief Determine which source is currently the active grid data provider.
   */
  virtual const char* getActiveSource(const std::vector<ITelemetrySource*> &sources) const = 0;

  /**
   * @brief Check whether a specific source is authorized to update grid telemetry.
   */
  virtual bool canUpdate(const char *sourceId, const std::vector<ITelemetrySource*> &sources) const = 0;
};

/**
 * @brief Default dual-source arbitrator supporting Auto-failover and Exclusive modes
 * between Chint DDSU666 Master and Shelly Pro EM.
 */
class DefaultGridArbitrator : public IGridArbitrator {
public:
  DefaultGridArbitrator(GridPriorityMode mode = DEFAULT_GRID_PRIORITY_MODE);

  void setMode(GridPriorityMode mode) override;
  GridPriorityMode getMode() const override { return _mode; }
  const char* getDescription() const override;

  const char* getActiveSource(const std::vector<ITelemetrySource*> &sources) const override;
  bool canUpdate(const char *sourceId, const std::vector<ITelemetrySource*> &sources) const override;

private:
  GridPriorityMode _mode;
  static ITelemetrySource* findSource(const std::vector<ITelemetrySource*> &sources, const char *id);
};
