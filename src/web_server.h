#pragma once

#include <Arduino.h>
#include <WebServer.h>
#include <WiFi.h>
#include "meter_data.h"
#include "modbus_rtu.h"

extern volatile bool g_isUpdatingFirmware;

// Forward declaration of TelemetryManager
class TelemetryManager;

void initWebServer(ModbusRTUServer &modbus, TelemetryManager &telemetry);
void handleWebServer();
