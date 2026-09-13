#include "web_server.h"
#include "mqtt_manager.h"
#include "config.h"
#include "web_ui_gz.h"
#include <ArduinoJson.h>
#include <Update.h>
#include <esp_task_wdt.h>
#include <cmath>

#if ENABLE_ESPNOW
#include "esp_now_receiver.h"
#endif

class SafeWebServer : public WebServer {
public:
  using WebServer::WebServer;
  bool isUploadActive() const { return _currentUpload != nullptr; }
};

static SafeWebServer s_server(80);

static ModbusRTUServer *s_modbus = nullptr;
static MQTTManager     *s_mqtt   = nullptr;
static char s_jsonBuffer[1024];

static inline float round1(float v) { return std::round(v * 10.0f) / 10.0f; }
static inline float round2(float v) { return std::round(v * 100.0f) / 100.0f; }

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

  StaticJsonDocument<1024> doc;
  doc["status"] = healthy ? "HEALTHY" : (s_modbus->isEnabled() ? "DEGRADED" : "FAILSAFE");
  doc["uptime_sec"] = (uint32_t)(millis() / 1000);
  doc["free_heap"] = (uint32_t)ESP.getFreeHeap();

  JsonObject wifi = doc.createNestedObject("wifi");
  wifi["connected"] = s_mqtt->isWiFiConnected();
  wifi["ip"] = WiFi.localIP().toString();
  wifi["rssi"] = WiFi.RSSI();

#if ENABLE_ESPNOW
  JsonObject espnow = doc.createNestedObject("espnow");
  espnow["enabled"] = en.initialized;
  espnow["channel"] = WiFi.channel();
  espnow["mac"] = WiFi.macAddress();
  espnow["packets"] = (uint32_t)en.packet_count;
  espnow["age_sec"] = (enAge >= 0) ? round1(enAge) : -1.0f;
  espnow["last_sender"] = en.last_mac_str;
  espnow["last_watts"] = round1(en.last_power_watts);
  espnow["rssi"] = en.rssi;
#endif

  JsonObject shelly = doc.createNestedObject("shelly");
  shelly["ws_connected"] = s_mqtt->isShellyWsConnected();
  shelly["ip"] = SHELLY_IP;
  shelly["age_sec"] = round1(gridAge);

  JsonObject opendtu = doc.createNestedObject("opendtu");
  opendtu["mqtt_connected"] = s_mqtt->isMQTTConnected();
  opendtu["age_sec"] = round1(pvAge);

  JsonObject modbus = doc.createNestedObject("modbus");
  modbus["rs485_active"] = s_modbus->isEnabled();
  modbus["queries"] = s_modbus->getQueryCount();
  modbus["crc_errors"] = s_modbus->getCrcErrorCount();
  modbus["rate_hz"] = round1(modbusRate);
  modbus["age_sec"] = (modbusAge >= 0) ? round1(modbusAge) : -1.0f;
  modbus["inverter_connected"] = inverterConnected;

  JsonObject gridObj = doc.createNestedObject("grid");
  gridObj["source"] = source;
  gridObj["watts"] = round1(gw);
  gridObj["flow"] = flow;
  gridObj["volts"] = round1(grid.voltage);
  gridObj["amps"] = round2(grid.current);
  gridObj["pf"] = round2(grid.power_factor);
  gridObj["freq"] = round1(grid.frequency);
  gridObj["import_kwh"] = round2(grid.import_kwh);
  gridObj["export_kwh"] = round2(grid.export_kwh);

  JsonObject pvObj = doc.createNestedObject("pv");
  pvObj["watts"] = round1(MeterState::getPvWatts());
  pvObj["yield_kwh"] = round2(pv.import_kwh);

  serializeJson(doc, s_jsonBuffer, sizeof(s_jsonBuffer));
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

static bool s_otaAuthorized = false;

static bool isOtaAuthorized() {
#ifndef OTA_PASSWORD
  return true;
#else
  if (strlen(OTA_PASSWORD) == 0) return true;
  if (s_server.authenticate("admin", OTA_PASSWORD)) return true;
  if (s_server.hasArg("password") && s_server.arg("password") == OTA_PASSWORD) return true;
  if (s_server.hasHeader("X-OTA-Password") && s_server.header("X-OTA-Password") == OTA_PASSWORD) return true;
  return false;
#endif
}

void initWebServer(ModbusRTUServer &modbus, MQTTManager &mqtt) {
  s_modbus = &modbus;
  s_mqtt   = &mqtt;

  const char *headerkeys[] = {"X-OTA-Password", "Authorization"};
  s_server.collectHeaders(headerkeys, 2);

  s_server.on("/", handleRoot);
  s_server.on("/health", handleHealthJson);
  s_server.on("/status", handleHealthJson);

  s_server.on("/update", HTTP_GET, []() {
    s_server.sendHeader("Location", "/");
    s_server.send(302, "text/plain", "");
  });

  // Web Browser OTA firmware upload route (Protected with OTA_PASSWORD)
  s_server.on("/update", HTTP_POST, []() {
    if (!isOtaAuthorized() || !s_otaAuthorized) {
      s_otaAuthorized = false;
      s_server.sendHeader("Connection", "close");
      return s_server.requestAuthentication();
    }
    s_otaAuthorized = false;
    s_server.sendHeader("Connection", "close");
    if (Update.hasError()) {
      s_server.send(500, "text/plain", "FAIL");
    } else {
      s_server.send(200, "text/plain", "OK");
      delay(500);
      ESP.restart();
    }
  }, []() {
    if (!s_server.isUploadActive()) return;
    HTTPUpload& upload = s_server.upload();
    esp_task_wdt_reset();
    if (upload.status == UPLOAD_FILE_START) {
      s_otaAuthorized = isOtaAuthorized();
      if (!s_otaAuthorized) {
        Serial.println("\n[Web OTA] Unauthorized upload attempt blocked!");
        return;
      }
      Serial.printf("\n[Web OTA] Authorized upload starting: %s\n", upload.filename.c_str());
      if (s_modbus) s_modbus->setEnabled(false);
      if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_WRITE) {
      if (!s_otaAuthorized) return;
      if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_END) {
      if (!s_otaAuthorized) return;
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
