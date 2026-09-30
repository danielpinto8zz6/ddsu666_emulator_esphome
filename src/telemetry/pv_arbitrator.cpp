#include "pv_arbitrator.h"

DefaultPvArbitrator::DefaultPvArbitrator(PvPriorityMode mode)
  : _mode(mode) {}

void DefaultPvArbitrator::setMode(PvPriorityMode mode) {
  _mode = mode;
}

const char* DefaultPvArbitrator::getDescription() const {
  switch (_mode) {
    case PV_PRI_OPENDTU_ONLY: return "OpenDTU Solar PV (MQTT)";
    case PV_PRI_DISABLED:     return "Disabled";
    default:                  return "Unknown";
  }
}

ITelemetrySource* DefaultPvArbitrator::findSource(const std::vector<ITelemetrySource*> &sources, const char *id) {
  for (auto *src : sources) {
    if (src && strcmp(src->getId(), id) == 0) {
      return src;
    }
  }
  return nullptr;
}

const char* DefaultPvArbitrator::getActiveSource(const std::vector<ITelemetrySource*> &sources) const {
  if (_mode == PV_PRI_DISABLED) return "NONE";

  ITelemetrySource *opendtu = findSource(sources, "OPENDTU_MQTT");
  if (opendtu && opendtu->isRecent(15000)) {
    return "OPENDTU_MQTT";
  }
  return "NONE";
}

bool DefaultPvArbitrator::canUpdate(const char *sourceId, const std::vector<ITelemetrySource*> &sources) const {
  if (_mode == PV_PRI_DISABLED) return false;
  return (strcmp(sourceId, "OPENDTU_MQTT") == 0);
}
