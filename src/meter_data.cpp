#include "meter_data.h"
#include <cmath>

portMUX_TYPE MeterState::spinlock = portMUX_INITIALIZER_UNLOCKED;
Preferences MeterState::prefs;

MeterTelemetry MeterState::grid = { 230.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 50.0f, 0.0f, 0.0f };
MeterTelemetry MeterState::pv   = { 230.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 50.0f, 0.0f, 0.0f };

float MeterState::lastSavedGridImport = 0.0f;
float MeterState::lastSavedGridExport = 0.0f;
float MeterState::lastSavedPvYield    = 0.0f;
uint32_t MeterState::lastGridSaveTime = 0;
uint32_t MeterState::lastPvSaveTime   = 0;

#define NVS_TIME_FLUSH_INTERVAL_MS 7200000UL // 2 hours

void MeterState::init() {
  setDefaults();

  // Load persisted energy counters from NVS
  prefs.begin("ddsu666", false);
  float g_imp = prefs.getFloat("g_imp", 0.0f);
  float g_exp = prefs.getFloat("g_exp", 0.0f);
  float pv_yld = prefs.getFloat("pv_yld", 0.0f);

  portENTER_CRITICAL(&spinlock);
  grid.import_kwh = g_imp;
  grid.export_kwh = g_exp;
  pv.import_kwh = pv_yld;
  portEXIT_CRITICAL(&spinlock);

  lastSavedGridImport = g_imp;
  lastSavedGridExport = g_exp;
  lastSavedPvYield = pv_yld;
  lastGridSaveTime = millis();
  lastPvSaveTime = millis();

  Serial.printf("[NVS] Loaded Energy Counters: Grid Imp=%.2f kWh, Grid Exp=%.2f kWh, PV=%.2f kWh\n",
                g_imp, g_exp, pv_yld);
}

void MeterState::setDefaults() {
  portENTER_CRITICAL(&spinlock);
  grid.voltage = 230.0f;
  grid.current = 0.0f;
  grid.active_power = 0.0f;
  grid.reactive_power = 0.0f;
  grid.apparent_power = 0.0f;
  grid.power_factor = 1.0f;
  grid.frequency = 50.0f;

  pv.voltage = 230.0f;
  pv.current = 0.0f;
  pv.active_power = 0.0f;
  pv.reactive_power = 0.0f;
  pv.apparent_power = 0.0f;
  pv.power_factor = 1.0f;
  pv.frequency = 50.0f;
  portEXIT_CRITICAL(&spinlock);
}

void MeterState::zeroPvPower() {
  portENTER_CRITICAL(&spinlock);
  pv.active_power = 0.0f;
  pv.apparent_power = 0.0f;
  pv.reactive_power = 0.0f;
  pv.current = 0.0f;
  portEXIT_CRITICAL(&spinlock);
}

void MeterState::updateGridRealtime(float v, float c, float p_kw, float s_kw, float q_kvar, float pf, float f) {
  portENTER_CRITICAL(&spinlock);
  grid.voltage = v;
  grid.current = c;
  grid.active_power = p_kw;
  grid.apparent_power = s_kw;
  grid.reactive_power = q_kvar;
  grid.power_factor = pf;
  grid.frequency = f;
  portEXIT_CRITICAL(&spinlock);
}

void MeterState::updateGridEnergy(float import_kwh, float export_kwh) {
  bool saveImp = false;
  bool saveExp = false;
  uint32_t now = millis();
  bool timeFlush = (now - lastGridSaveTime >= NVS_TIME_FLUSH_INTERVAL_MS);

  portENTER_CRITICAL(&spinlock);
  if (import_kwh >= 0.0f) {
    grid.import_kwh = import_kwh;
    if (std::fabs(import_kwh - lastSavedGridImport) >= 0.05f || (timeFlush && import_kwh != lastSavedGridImport)) {
      saveImp = true;
      lastSavedGridImport = import_kwh;
    }
  }
  if (export_kwh >= 0.0f) {
    grid.export_kwh = export_kwh;
    if (std::fabs(export_kwh - lastSavedGridExport) >= 0.05f || (timeFlush && export_kwh != lastSavedGridExport)) {
      saveExp = true;
      lastSavedGridExport = export_kwh;
    }
  }
  if (saveImp || saveExp) {
    lastGridSaveTime = now;
  }
  portEXIT_CRITICAL(&spinlock);

  // Write to NVS outside critical section to avoid blocking Core 1
  if (saveImp) prefs.putFloat("g_imp", import_kwh);
  if (saveExp) prefs.putFloat("g_exp", export_kwh);
}

void MeterState::updatePvPower(float p_watts) {
  portENTER_CRITICAL(&spinlock);
  float kw = p_watts / 1000.0f;
  pv.active_power = kw;
  pv.apparent_power = kw;
  pv.reactive_power = 0.0f;
  pv.power_factor = 1.0f;
  pv.current = p_watts / 230.0f;
  pv.voltage = 230.0f;
  pv.frequency = 50.0f;
  portEXIT_CRITICAL(&spinlock);
}

void MeterState::updatePvYield(float yield_kwh) {
  bool saveYld = false;
  uint32_t now = millis();
  bool timeFlush = (now - lastPvSaveTime >= NVS_TIME_FLUSH_INTERVAL_MS);

  portENTER_CRITICAL(&spinlock);
  if (yield_kwh >= 0.0f) {
    pv.import_kwh = yield_kwh;
    if (std::fabs(yield_kwh - lastSavedPvYield) >= 0.05f || (timeFlush && yield_kwh != lastSavedPvYield)) {
      saveYld = true;
      lastSavedPvYield = yield_kwh;
      lastPvSaveTime = now;
    }
  }
  portEXIT_CRITICAL(&spinlock);

  if (saveYld) prefs.putFloat("pv_yld", yield_kwh);
}

MeterTelemetry MeterState::getGrid() {
  MeterTelemetry copy;
  portENTER_CRITICAL(&spinlock);
  copy = grid;
  portEXIT_CRITICAL(&spinlock);
  return copy;
}

MeterTelemetry MeterState::getPv() {
  MeterTelemetry copy;
  portENTER_CRITICAL(&spinlock);
  copy = pv;
  portEXIT_CRITICAL(&spinlock);
  return copy;
}

float MeterState::getGridWatts() {
  float w;
  portENTER_CRITICAL(&spinlock);
  // active_power is in kW (inverted sign for Hoymiles) -> convert back to real watts
  w = grid.active_power * -1000.0f;
  portEXIT_CRITICAL(&spinlock);
  return w;
}

float MeterState::getPvWatts() {
  float w;
  portENTER_CRITICAL(&spinlock);
  w = pv.active_power * 1000.0f;
  portEXIT_CRITICAL(&spinlock);
  return w;
}
