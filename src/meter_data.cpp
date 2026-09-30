#include "meter_data.h"
#include <cmath>

portMUX_TYPE MeterState::spinlock = portMUX_INITIALIZER_UNLOCKED;

MeterTelemetry MeterState::grid = { 230.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 50.0f, 0.0f, 0.0f };
MeterTelemetry MeterState::pv   = { 230.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 50.0f, 0.0f, 0.0f };

void MeterState::init() {
  setDefaults();
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
  portENTER_CRITICAL(&spinlock);
  if (import_kwh >= 0.0f) {
    grid.import_kwh = import_kwh;
  }
  if (export_kwh >= 0.0f) {
    grid.export_kwh = export_kwh;
  }
  portEXIT_CRITICAL(&spinlock);
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
  portENTER_CRITICAL(&spinlock);
  if (yield_kwh >= 0.0f) {
    pv.import_kwh = yield_kwh;
  }
  portEXIT_CRITICAL(&spinlock);
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
