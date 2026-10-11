#include "pv_arbitrator.h"

DefaultPvArbitrator::DefaultPvArbitrator(PvPriorityMode mode)
  : _mode(mode > PV_PRI_FAKE_50W ? DEFAULT_PV_PRIORITY_MODE : mode) {}

void DefaultPvArbitrator::setMode(PvPriorityMode mode) {
  _mode = (mode > PV_PRI_FAKE_50W) ? DEFAULT_PV_PRIORITY_MODE : mode;
}

const char* DefaultPvArbitrator::getDescription() const {
  switch (_mode) {
    case PV_PRI_AUTO:         return "Auto: Eastron -> OpenDTU Fallback";
    case PV_PRI_EASTRON_ONLY: return "Eastron SDM120 Only";
    case PV_PRI_OPENDTU_ONLY: return "OpenDTU Solar PV (MQTT)";
    case PV_PRI_DISABLED:     return "Disabled";
    case PV_PRI_FAKE_50W:     return "Fake 50W Solar PV";
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
  if (_mode == PV_PRI_FAKE_50W) return "FAKE_50W";

  bool eastronLocalRecent = (_lastEastronLocalTime != 0 && (millis() - _lastEastronLocalTime <= 5000));
  if (eastronLocalRecent) return "EASTRON_LOCAL";

  ITelemetrySource *chint = findSource(sources, "CHINT_WS");
  bool eastronWsRecent = (chint && chint->isPvRecent(5000));
  if (eastronWsRecent) return "EASTRON_WS";

  ITelemetrySource *opendtu = findSource(sources, "OPENDTU_MQTT");
  bool opendtuRecent = (opendtu && opendtu->isRecent(15000));

  if (_mode == PV_PRI_AUTO) {
    if (opendtuRecent) return "OPENDTU_MQTT";
    return "NONE";
  }
  if (_mode == PV_PRI_EASTRON_ONLY) {
    return "NONE";
  }
  if (_mode == PV_PRI_OPENDTU_ONLY) {
    return opendtuRecent ? "OPENDTU_MQTT" : "NONE";
  }
  return "NONE";
}

bool DefaultPvArbitrator::canUpdate(const char *sourceId, const std::vector<ITelemetrySource*> &sources) const {
  if (_mode == PV_PRI_DISABLED || _mode == PV_PRI_FAKE_50W) return false;

  bool isEastron = (strcmp(sourceId, "EASTRON_WS") == 0 || strcmp(sourceId, "EASTRON_LOCAL") == 0);
  bool isOpenDtu = (strcmp(sourceId, "OPENDTU_MQTT") == 0);

  if (isEastron) {
    if (strcmp(sourceId, "EASTRON_LOCAL") == 0) {
      _lastEastronLocalTime = millis();
    }
    return true;
  }

  if (_mode == PV_PRI_AUTO) {
    if (isOpenDtu) {
      bool eastronLocalRecent = (_lastEastronLocalTime != 0 && (millis() - _lastEastronLocalTime <= 5000));
      ITelemetrySource *chint = findSource(sources, "CHINT_WS");
      bool eastronWsRecent = (chint && chint->isPvRecent(5000));
      return (!eastronLocalRecent && !eastronWsRecent);
    }
    return false;
  }
  if (_mode == PV_PRI_EASTRON_ONLY) {
    return isEastron;
  }
  if (_mode == PV_PRI_OPENDTU_ONLY) {
    return isOpenDtu;
  }
  return false;
}
