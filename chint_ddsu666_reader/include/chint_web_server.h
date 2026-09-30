#pragma once

#include <Arduino.h>
#include <WebServer.h>
#include <Update.h>
#include <ArduinoJson.h>
#include "interfaces.h"

/**
 * @brief HTTP Web Server managing Dashboard UI, Health JSON, Settings, and OTA (SRP).
 */
class ChintWebServer {
public:
  ChintWebServer(IChintMeterReader &reader, ITelemetryPublisher &publisher, uint16_t port = 80);

  void begin();
  void loop();

private:
  IChintMeterReader &_reader;
  ITelemetryPublisher &_publisher;
  WebServer _server;

  void handleRoot();
  void handleHealthJson();
  void handleSettingsApi();
  void handleReboot();
  void setupOtaRoutes();
};
