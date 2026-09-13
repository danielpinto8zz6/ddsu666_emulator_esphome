#pragma once

#include <Arduino.h>
#include <WebServer.h>
#include <WiFi.h>
#include "meter_data.h"
#include "modbus_rtu.h"

// Forward declaration of MQTTManager
class MQTTManager;

void initWebServer(ModbusRTUServer &modbus, MQTTManager &mqtt);
void handleWebServer();

