#pragma once

#include <Arduino.h>
#include <WebServer.h>
#include <WiFi.h>
#include "meter_data.h"
#include "modbus_rtu.h"

extern volatile bool g_isUpdatingFirmware;

// Forward declaration of MQTTManager
class MQTTManager;

void initWebServer(ModbusRTUServer &modbus, MQTTManager &mqtt);
void handleWebServer();

