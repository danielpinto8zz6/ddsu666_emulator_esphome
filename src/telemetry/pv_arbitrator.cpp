#include "pv_arbitrator.h"

DefaultPvArbitrator::DefaultPvArbitrator(PvPriorityMode mode)
  : _mode(mode) {}

void DefaultPvArbitrator::setMode(PvPriorityMode mode) {
  _mode = mode;
}

const char* DefaultPvArbitrator::getDescription() const {
  switch (_mode) {
    case PV_PRI_OPENDTU_ESPNOW: return "Auto: OpenDTU MQTT -> Sonoff POWCT ESP-NOW";
    case PV_PRI_ESPNOW_OPENDTU: return "Auto: Sonoff POWCT ESP-NOW -> OpenDTU MQTT";
    case PV_PRI_OPENDTU_ONLY:   return "OpenDTU MQTT Only";
    case PV_PRI_ESPNOW_ONLY:    return "Sonoff POWCT ESP-NOW Only";
    default:                    return "Unknown";
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
  ITelemetrySource *opendtu = findSource(sources, "OPENDTU_MQTT");
  ITelemetrySource *espnow  = findSource(sources, "ESPNOW_PV");

  bool opendtuFresh = opendtu && opendtu->isRecent(15000);
  bool espnowFresh  = espnow  && espnow->isRecent(10000);

  switch (_mode) {
    case PV_PRI_OPENDTU_ESPNOW:
      if (opendtuFresh) return "OPENDTU_MQTT";
      if (espnowFresh)  return "ESPNOW_PV";
      break;

    case PV_PRI_ESPNOW_OPENDTU:
      if (espnowFresh)  return "ESPNOW_PV";
      if (opendtuFresh) return "OPENDTU_MQTT";
      break;

    case PV_PRI_OPENDTU_ONLY:
      if (opendtuFresh) return "OPENDTU_MQTT";
      break;

    case PV_PRI_ESPNOW_ONLY:
      if (espnowFresh)  return "ESPNOW_PV";
      break;
  }
  return "NONE";
}

bool DefaultPvArbitrator::canUpdate(const char *sourceId, const std::vector<ITelemetrySource*> &sources) const {
  bool isOpenDtu = (strcmp(sourceId, "OPENDTU_MQTT") == 0);
  bool isEspNow  = (strcmp(sourceId, "ESPNOW_PV") == 0);

  ITelemetrySource *opendtu = findSource(sources, "OPENDTU_MQTT");
  ITelemetrySource *espnow  = findSource(sources, "ESPNOW_PV");

  switch (_mode) {
    case PV_PRI_OPENDTU_ESPNOW:
      if (isOpenDtu) return true;
      if (isEspNow)  return !(opendtu && opendtu->isRecent(15000));
      return false;

    case PV_PRI_ESPNOW_OPENDTU:
      if (isEspNow)  return true;
      if (isOpenDtu) return !(espnow && espnow->isRecent(10000));
      return false;

    case PV_PRI_OPENDTU_ONLY:
      return isOpenDtu;

    case PV_PRI_ESPNOW_ONLY:
      return isEspNow;

    default:
      return false;
  }
}
