#pragma once

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/portmacro.h>

struct MeterTelemetry {
  float voltage;          // V
  float current;          // A
  float active_power;     // kW (Grid: inverted for Hoymiles; PV: positive)
  float reactive_power;   // kVAR
  float apparent_power;   // kVA
  float power_factor;     // -1.0 to +1.0
  float frequency;        // Hz
  float import_kwh;       // kWh
  float export_kwh;       // kWh
};

class MeterState {
public:
  static void init();
  static void setDefaults();
  static void zeroPvPower();

  static void updateGridRealtime(float v, float c, float p_kw, float s_kw, float q_kvar, float pf, float f);
  static void updateGridEnergy(float import_kwh, float export_kwh);
  static void updatePvPower(float p_watts);
  static void updatePvYield(float yield_kwh);

  static MeterTelemetry getGrid();
  static MeterTelemetry getPv();

  static float getGridWatts();
  static float getPvWatts();

private:
  static portMUX_TYPE spinlock;
  static MeterTelemetry grid;
  static MeterTelemetry pv;
};
