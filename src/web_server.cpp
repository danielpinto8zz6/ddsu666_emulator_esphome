#include "web_server.h"
#include "mqtt_manager.h"
#include "config.h"
#include "web_ui_gz.h"
#include <Update.h>
#include <esp_task_wdt.h>

#if ENABLE_ESPNOW
#include "esp_now_receiver.h"
#endif

static WebServer s_server(80);

static ModbusRTUServer *s_modbus = nullptr;
static MQTTManager     *s_mqtt   = nullptr;
static char s_jsonBuffer[1024];

static void handleHealthJson() {
  MeterTelemetry grid = MeterState::getGrid();
  MeterTelemetry pv   = MeterState::getPv();

  uint32_t now = millis();
  float gridAge = (now - s_mqtt->getLastGridMsgTime()) / 1000.0f;
  float pvAge   = (now - s_mqtt->getLastPvMsgTime()) / 1000.0f;

#if ENABLE_ESPNOW
  EspNowStats en = ESPNowReceiver::getStats();
  float enAge = (en.last_packet_time != 0) ? (now - en.last_packet_time) / 1000.0f : -1.0f;
  bool gridSourceActive = (GRID_SOURCE_PRIORITY == 1) ? ESPNowReceiver::isRecent() : (s_mqtt->isShellyWsConnected() || ESPNowReceiver::isRecent());
#else
  bool gridSourceActive = s_mqtt->isShellyWsConnected();
#endif

  bool healthy = s_mqtt->isWiFiConnected() && 
                 gridSourceActive && 
                 s_modbus->isEnabled();

  float gw = MeterState::getGridWatts();
  const char *flow = (gw > 5.0f) ? "IMPORT" : ((gw < -5.0f) ? "EXPORT" : "BALANCED");
#if ENABLE_ESPNOW
  const char *source = (GRID_SOURCE_PRIORITY != 0 && ESPNowReceiver::isRecent()) ? "ESPNOW" : (s_mqtt->isShellyWsConnected() ? "SHELLY_WS" : "NONE");
#else
  const char *source = s_mqtt->isShellyWsConnected() ? "SHELLY_WS" : "NONE";
#endif

  uint32_t lastQueryTime = s_modbus ? s_modbus->getLastQueryTime() : 0;
  float modbusAge = (lastQueryTime != 0) ? (now - lastQueryTime) / 1000.0f : -1.0f;
  float modbusRate = s_modbus ? s_modbus->getQueryRateHz() : 0.0f;
  bool inverterConnected = s_modbus ? s_modbus->isInverterActive(2500) : false;

  snprintf(s_jsonBuffer, sizeof(s_jsonBuffer),
    "{"
      "\"status\":\"%s\","
      "\"uptime_sec\":%lu,"
      "\"free_heap\":%lu,"
      "\"wifi\":{\"connected\":%s,\"ip\":\"%s\",\"rssi\":%d},"
#if ENABLE_ESPNOW
      "\"espnow\":{\"enabled\":%s,\"channel\":%d,\"mac\":\"%s\",\"packets\":%lu,\"age_sec\":%.1f,\"last_sender\":\"%s\",\"last_watts\":%.1f,\"rssi\":%d},"
#endif
      "\"shelly\":{\"ws_connected\":%s,\"ip\":\"%s\",\"age_sec\":%.1f},"
      "\"opendtu\":{\"mqtt_connected\":%s,\"age_sec\":%.1f},"
      "\"modbus\":{\"rs485_active\":%s,\"queries\":%lu,\"crc_errors\":%lu,\"rate_hz\":%.1f,\"age_sec\":%.1f,\"inverter_connected\":%s},"
      "\"grid\":{\"source\":\"%s\",\"watts\":%.1f,\"flow\":\"%s\",\"volts\":%.1f,\"amps\":%.2f,\"pf\":%.2f,\"freq\":%.1f,\"import_kwh\":%.2f,\"export_kwh\":%.2f},"
      "\"pv\":{\"watts\":%.1f,\"yield_kwh\":%.2f}"
    "}",
    healthy ? "HEALTHY" : (s_modbus->isEnabled() ? "DEGRADED" : "FAILSAFE"),
    (unsigned long)(millis() / 1000),
    (unsigned long)ESP.getFreeHeap(),
    s_mqtt->isWiFiConnected() ? "true" : "false",
    WiFi.localIP().toString().c_str(),
    WiFi.RSSI(),
#if ENABLE_ESPNOW
    en.initialized ? "true" : "false",
    WiFi.channel(),
    WiFi.macAddress().c_str(),
    (unsigned long)en.packet_count,
    enAge,
    en.last_mac_str,
    en.last_power_watts,
    en.rssi,
#endif
    s_mqtt->isShellyWsConnected() ? "true" : "false",
    SHELLY_IP,
    gridAge,
    s_mqtt->isMQTTConnected() ? "true" : "false",
    pvAge,
    s_modbus->isEnabled() ? "true" : "false",
    (unsigned long)s_modbus->getQueryCount(),
    (unsigned long)s_modbus->getCrcErrorCount(),
    modbusRate,
    modbusAge,
    inverterConnected ? "true" : "false",
    source,
    gw,
    flow,
    grid.voltage,
    grid.current,
    grid.power_factor,
    grid.frequency,
    grid.import_kwh,
    grid.export_kwh,
    MeterState::getPvWatts(),
    pv.import_kwh
  );

  s_server.sendHeader("Access-Control-Allow-Origin", "*");
  s_server.send(200, "application/json", s_jsonBuffer);
}

static void handleRoot() {
#if ENABLE_WEB_DASHBOARD
  s_server.sendHeader("Content-Encoding", "gzip");
  s_server.send_P(200, "text/html", (const char*)ROOT_HTML_GZ, ROOT_HTML_GZ_LEN);
#else
  s_server.sendHeader("Location", "/health");
  s_server.send(302, "text/plain", "");
#endif
}

void initWebServer(ModbusRTUServer &modbus, MQTTManager &mqtt) {
  s_modbus = &modbus;
  s_mqtt   = &mqtt;

  s_server.on("/", handleRoot);
  s_server.on("/health", handleHealthJson);
  s_server.on("/status", handleHealthJson);

  // Web Browser OTA firmware upload route
  s_server.on("/update", HTTP_POST, []() {
    s_server.sendHeader("Connection", "close");
    s_server.send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
    delay(500);
    ESP.restart();
  }, []() {
    HTTPUpload& upload = s_server.upload();
    esp_task_wdt_reset();
    if (upload.status == UPLOAD_FILE_START) {
      Serial.printf("\n[Web OTA] Upload starting: %s\n", upload.filename.c_str());
      if (s_modbus) s_modbus->setEnabled(false);
      if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_WRITE) {
      if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_END) {
      if (Update.end(true)) {
        Serial.printf("\n[Web OTA] Finished: %u bytes. Rebooting...\n", upload.totalSize);
      } else {
        Update.printError(Serial);
      }
    }
  });

  s_server.begin();

  Serial.println("[Web Server] Health Check UI ready at http://10.0.0.110/");
  Serial.println("[Web Server] JSON Health API at http://10.0.0.110/health");
}

void handleWebServer() {
  s_server.handleClient();
}
