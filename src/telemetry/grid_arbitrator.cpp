#include "grid_arbitrator.h"

DefaultGridArbitrator::DefaultGridArbitrator(GridPriorityMode mode)
  : _mode(mode) {}

void DefaultGridArbitrator::setMode(GridPriorityMode mode) {
  _mode = mode;
}

const char* DefaultGridArbitrator::getDescription() const {
  switch (_mode) {
    case GRID_PRI_CHINT_SHELLY: return "Auto: Chint WS -> Shelly WS";
    case GRID_PRI_SHELLY_CHINT: return "Auto: Shelly WS -> Chint WS";
    case GRID_PRI_CHINT_ONLY:   return "Chint WS Only";
    case GRID_PRI_SHELLY_ONLY:  return "Shelly WS Only";
    default:                    return "Unknown";
  }
}

ITelemetrySource* DefaultGridArbitrator::findSource(const std::vector<ITelemetrySource*> &sources, const char *id) {
  for (auto *src : sources) {
    if (src && strcmp(src->getId(), id) == 0) {
      return src;
    }
  }
  return nullptr;
}

const char* DefaultGridArbitrator::getActiveSource(const std::vector<ITelemetrySource*> &sources) const {
  ITelemetrySource *chint = findSource(sources, "CHINT_WS");
  ITelemetrySource *shelly = findSource(sources, "SHELLY_WS");

  bool chintFresh = chint && chint->isRecent(CHINT_TIMEOUT_MS);
  bool shellyFresh = shelly && shelly->isRecent(5000);

  switch (_mode) {
    case GRID_PRI_CHINT_SHELLY:
      if (chintFresh) return "CHINT_WS";
      if (shellyFresh) return "SHELLY_WS";
      break;

    case GRID_PRI_SHELLY_CHINT:
      if (shellyFresh) return "SHELLY_WS";
      if (chintFresh) return "CHINT_WS";
      break;

    case GRID_PRI_CHINT_ONLY:
      if (chintFresh) return "CHINT_WS";
      break;

    case GRID_PRI_SHELLY_ONLY:
      if (shellyFresh) return "SHELLY_WS";
      break;
  }
  return "NONE";
}

bool DefaultGridArbitrator::canUpdate(const char *sourceId, const std::vector<ITelemetrySource*> &sources) const {
  bool isChint = (strcmp(sourceId, "CHINT_WS") == 0);
  bool isShelly = (strcmp(sourceId, "SHELLY_WS") == 0);

  ITelemetrySource *chint = findSource(sources, "CHINT_WS");
  ITelemetrySource *shelly = findSource(sources, "SHELLY_WS");

  switch (_mode) {
    case GRID_PRI_CHINT_SHELLY:
      if (isChint) return true;
      if (isShelly) return !(chint && chint->isRecent(CHINT_TIMEOUT_MS));
      return false;

    case GRID_PRI_SHELLY_CHINT:
      if (isShelly) return true;
      if (isChint) return !(shelly && shelly->isRecent(5000));
      return false;

    case GRID_PRI_CHINT_ONLY:
      return isChint;

    case GRID_PRI_SHELLY_ONLY:
      return isShelly;

    default:
      return false;
  }
}
