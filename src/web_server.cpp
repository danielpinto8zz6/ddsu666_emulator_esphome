#include "web_server.h"
#include "telemetry/telemetry_manager.h"
#include "config.h"
#include "web_ui_gz.h"
#include <ArduinoJson.h>
#include <Update.h>
#include <esp_task_wdt.h>
#include <cmath>
#include <Preferences.h>


class SafeWebServer : public WebServer {
public:
  using WebServer::WebServer;
  bool isUploadActive() const { return _currentUpload != nullptr; }
};

static SafeWebServer s_server(80);

static ModbusRTUServer  *s_modbus    = nullptr;
static TelemetryManager *s_telemetry = nullptr;
static char s_jsonBuffer[1024];
volatile bool g_isUpdatingFirmware = false;
extern volatile UBaseType_t g_networkTaskStackHWM;  // set in main.cpp

static inline float round1(float v) { return std::round(v * 10.0f) / 10.0f; }
static inline float round2(float v) { return std::round(v * 100.0f) / 100.0f; }

static void handleHealthJson() {
  MeterTelemetry grid = MeterState::getGrid();
  MeterTelemetry pv   = MeterState::getPv();

  uint32_t now = millis();
  float gridAge = (now - s_telemetry->getLastGridMsgTime()) / 1000.0f;
  float chintAge = (s_telemetry->getChintSource().getLastMessageTime() != 0) ? (now - s_telemetry->getChintSource().getLastMessageTime()) / 1000.0f : -1.0f;
  float shellyAge = (s_telemetry->getShellySource().getLastMessageTime() != 0) ? (now - s_telemetry->getShellySource().getLastMessageTime()) / 1000.0f : -1.0f;
  float pvAge   = (now - s_telemetry->getLastPvMsgTime()) / 1000.0f;

  bool gridSourceActive = s_telemetry->getChintSource().isRecent() || s_telemetry->getShellySource().isConnected();

  bool healthy = s_telemetry->isWiFiConnected() && 
                 gridSourceActive && 
                 s_modbus->isEnabled();

  float gw = MeterState::getGridWatts();
  const char *flow = (gw > 5.0f) ? "IMPORT" : ((gw < -5.0f) ? "EXPORT" : "BALANCED");
  const char *source = s_telemetry->getActiveGridSource();

  uint32_t lastQueryTime = s_modbus ? s_modbus->getLastQueryTime() : 0;
  float modbusAge = (lastQueryTime != 0) ? (now - lastQueryTime) / 1000.0f : -1.0f;
  uint32_t lastGridQueryTime = s_modbus ? s_modbus->getLastGridQueryTime() : 0;
  float gridModbusAge = (lastGridQueryTime != 0) ? (now - lastGridQueryTime) / 1000.0f : -1.0f;
  uint32_t lastPvQueryTime = s_modbus ? s_modbus->getLastPvQueryTime() : 0;
  float pvModbusAge = (lastPvQueryTime != 0) ? (now - lastPvQueryTime) / 1000.0f : -1.0f;
  float modbusRate = s_modbus ? s_modbus->getQueryRateHz() : 0.0f;
  bool inverterConnected = s_modbus ? s_modbus->isInverterActive(2500) : false;

  StaticJsonDocument<1280> doc;
  doc["status"] = healthy ? "HEALTHY" : (s_modbus->isEnabled() ? "DEGRADED" : "FAILSAFE");
  doc["uptime_sec"] = (uint32_t)(millis() / 1000);
  doc["free_heap"] = (uint32_t)ESP.getFreeHeap();

  JsonObject pri = doc.createNestedObject("priority");
  pri["mode"] = (uint8_t)s_telemetry->getGridPriority();
  pri["description"] = s_telemetry->getGridPriorityDescription();

  JsonObject wifi = doc.createNestedObject("wifi");
  wifi["connected"] = s_telemetry->isWiFiConnected();
  wifi["ip"] = WiFi.localIP().toString();
  wifi["rssi"] = WiFi.RSSI();


  JsonObject chint = doc.createNestedObject("chint");
  chint["ws_connected"] = s_telemetry->getChintSource().isConnected();
  chint["ip"] = s_telemetry->getChintSource().getIp();
  chint["age_sec"] = (chintAge >= 0) ? round1(chintAge) : -1.0f;

  JsonObject shelly = doc.createNestedObject("shelly");
  shelly["ws_connected"] = s_telemetry->getShellySource().isConnected();
  shelly["ip"] = s_telemetry->getShellySource().getIp();
  shelly["age_sec"] = (shellyAge >= 0) ? round1(shellyAge) : -1.0f;

  JsonObject opendtu = doc.createNestedObject("opendtu");
  opendtu["mqtt_connected"] = s_telemetry->getOpenDtuSource().isConnected();
  opendtu["age_sec"] = round1(pvAge);

  JsonObject modbus = doc.createNestedObject("modbus");
  modbus["rs485_active"] = s_modbus->isEnabled();
  modbus["queries"] = s_modbus->getQueryCount();
  modbus["queries_grid"] = s_modbus->getQueryCountGrid();
  modbus["queries_pv"] = s_modbus->getQueryCountPv();
  modbus["crc_errors"] = s_modbus->getCrcErrorCount();
  modbus["rate_hz"] = round1(modbusRate);
  modbus["age_sec"] = (modbusAge >= 0) ? round1(modbusAge) : -1.0f;
  modbus["age_grid_sec"] = (gridModbusAge >= 0) ? round1(gridModbusAge) : -1.0f;
  modbus["age_pv_sec"] = (pvModbusAge >= 0) ? round1(pvModbusAge) : -1.0f;
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

  // Source-transition event history (#5)
  JsonArray evArr = doc.createNestedArray("source_events");
  uint8_t total = s_telemetry->getSourceEventCount();
  uint8_t head  = s_telemetry->getSourceEventHead();
  const SourceEvent *evs = s_telemetry->getSourceEvents();
  // Iterate oldest -> newest
  for (uint8_t i = 0; i < total; i++) {
    uint8_t idx = (head - total + i + SOURCE_EVENT_HISTORY) % SOURCE_EVENT_HISTORY;
    JsonObject ev = evArr.createNestedObject();
    ev["t"] = evs[idx].timestamp_s;
    ev["from"] = evs[idx].from;
    ev["to"]   = evs[idx].to;
  }

  // Diagnostic: network task stack HWM (#4)
  doc["net_stack_hwm"] = (uint32_t)g_networkTaskStackHWM;

  serializeJson(doc, s_jsonBuffer, sizeof(s_jsonBuffer));
  s_server.sendHeader("Access-Control-Allow-Origin", "*");
  s_server.send(200, "application/json", s_jsonBuffer);
}

static void handleRoot() {
#if ENABLE_WEB_DASHBOARD
  s_server.sendHeader("Content-Encoding", "gzip");
  s_server.send_P(200, "text/html", (const char*)WEB_UI_GZ, WEB_UI_GZ_SIZE);
#else
  s_server.sendHeader("Location", "/health");
  s_server.send(302, "text/plain", "");
#endif
}

static bool s_otaAuthorized = false;

static String getStoredOtaPassword() {
  Preferences prefs;
  if (prefs.begin("ddsu666", true)) {
    String value = prefs.getString("ota_pass", OTA_PASSWORD);
    prefs.end();
    return value;
  }
  return String(OTA_PASSWORD);
}

static bool isOtaAuthorized() {
  return true;
}

void initWebServer(ModbusRTUServer &modbus, TelemetryManager &telemetry) {
  s_modbus    = &modbus;
  s_telemetry = &telemetry;

  const char *headerkeys[] = {"X-OTA-Password", "Authorization"};
  s_server.collectHeaders(headerkeys, 2);

  s_server.on("/", handleRoot);
  s_server.on("/health", handleHealthJson);
  s_server.on("/status", handleHealthJson);

  s_server.on("/api/priority", HTTP_ANY, []() {
    if (s_server.hasArg("mode")) {
      uint8_t mode = (uint8_t)s_server.arg("mode").toInt();
      if (mode <= 3 && s_telemetry) {
        s_telemetry->setGridPriority((GridPriorityMode)mode);
        s_server.sendHeader("Access-Control-Allow-Origin", "*");
        s_server.send(200, "application/json", "{\"status\":\"OK\",\"mode\":" + String(mode) + ",\"desc\":\"" + s_telemetry->getGridPriorityDescription() + "\"}");
        return;
      }
    }
    if (s_telemetry) {
      s_server.sendHeader("Access-Control-Allow-Origin", "*");
      s_server.send(200, "application/json", "{\"mode\":" + String((uint8_t)s_telemetry->getGridPriority()) + ",\"desc\":\"" + s_telemetry->getGridPriorityDescription() + "\"}");
      return;
    }
    s_server.send(400, "application/json", "{\"error\":\"Invalid request\"}");
  });

  // ---- Runtime Config API (/api/config) ----
  // GET  -> returns current values (compile-time defaults overridden by NVS)
  // POST -> saves new values to NVS (takes effect after reboot)
  s_server.on("/api/config", HTTP_GET, []() {
    if (!isOtaAuthorized()) return s_server.requestAuthentication();
    Preferences prefs;
    prefs.begin("ddsu666", true); // read-only
    StaticJsonDocument<512> doc;
    doc["wifi_ssid"]     = prefs.getString("wifi_ssid",  WIFI_SSID);
    String storedWifiPass = prefs.getString("wifi_pass",  WIFI_PASSWORD);
    doc["wifi_pass_set"] = (storedWifiPass.length() > 0);
    doc["chint_ip"]      = prefs.getString("chint_ip",    CHINT_XIAO_IP);
    doc["shelly_ip"]     = prefs.getString("shelly_ip",   SHELLY_IP);
    doc["mqtt_broker"]   = prefs.getString("mqtt_broker", MQTT_BROKER_IP);
    doc["mqtt_port"]     = prefs.getUShort("mqtt_port",   MQTT_PORT);
    doc["mqtt_user"]     = prefs.getString("mqtt_user",   MQTT_USER);
    String storedPass = prefs.getString("mqtt_pass", MQTT_PASSWORD);
    doc["mqtt_pass_set"] = (storedPass.length() > 0);
    doc["ota_password"]  = prefs.getString("ota_pass",   OTA_PASSWORD);
    prefs.end();
    char buf[512];
    serializeJson(doc, buf, sizeof(buf));
    s_server.sendHeader("Access-Control-Allow-Origin", "*");
    s_server.send(200, "application/json", buf);
  });

  s_server.on("/api/config", HTTP_POST, []() {
    if (!isOtaAuthorized()) return s_server.requestAuthentication();
    Preferences prefs;
    prefs.begin("ddsu666", false); // read-write
    bool changed = false;

    if (s_server.hasArg("wifi_ssid")) {
      String v = s_server.arg("wifi_ssid");
      if (v.length() > 0) { prefs.putString("wifi_ssid", v); changed = true; }
    }
    if (s_server.hasArg("wifi_pass")) {
      prefs.putString("wifi_pass", s_server.arg("wifi_pass"));
      changed = true;
    }
    if (s_server.hasArg("chint_ip")) {
      String v = s_server.arg("chint_ip");
      if (v.length() >= 7) { prefs.putString("chint_ip", v); changed = true; }
    }
    if (s_server.hasArg("shelly_ip")) {
      String v = s_server.arg("shelly_ip");
      if (v.length() >= 7) { prefs.putString("shelly_ip", v); changed = true; }
    }
    if (s_server.hasArg("mqtt_broker")) {
      String v = s_server.arg("mqtt_broker");
      if (v.length() >= 7) { prefs.putString("mqtt_broker", v); changed = true; }
    }
    if (s_server.hasArg("mqtt_port")) {
      uint16_t p = (uint16_t)s_server.arg("mqtt_port").toInt();
      if (p > 0) { prefs.putUShort("mqtt_port", p); changed = true; }
    }
    if (s_server.hasArg("mqtt_user")) {
      prefs.putString("mqtt_user", s_server.arg("mqtt_user")); changed = true;
    }
    if (s_server.hasArg("mqtt_pass")) {
      prefs.putString("mqtt_pass", s_server.arg("mqtt_pass")); changed = true;
    }
    if (s_server.hasArg("ota_password")) {
      prefs.putString("ota_pass", s_server.arg("ota_password")); changed = true;
    }
    prefs.end();

    s_server.sendHeader("Access-Control-Allow-Origin", "*");
    s_server.send(200, "application/json",
      changed ? "{\"status\":\"OK\",\"msg\":\"Saved — reboot to apply\"}" :
                "{\"status\":\"OK\",\"msg\":\"No changes\"}" );
  });

  s_server.on("/api/config", HTTP_OPTIONS, []() {
    s_server.sendHeader("Access-Control-Allow-Origin", "*");
    s_server.sendHeader("Access-Control-Allow-Methods", "GET,POST,OPTIONS");
    s_server.sendHeader("Access-Control-Allow-Headers", "Content-Type,X-OTA-Password,Authorization");
    s_server.send(204);
  });

  s_server.on("/update", HTTP_GET, []() {
    s_server.sendHeader("Location", "/");
    s_server.send(302, "text/plain", "");
  });

  s_server.on("/reboot", HTTP_POST, []() {
    s_server.sendHeader("Connection", "close");
    s_server.send(200, "text/plain", "Rebooting...");
    delay(500);
    ESP.restart();
  });

  // Web Browser OTA firmware upload route (Passwordless)
  s_server.on("/update", HTTP_POST, []() {
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
      g_isUpdatingFirmware = true;
      Serial.printf("\n[Web OTA] Upload starting: %s\n", upload.filename.c_str());
      if (s_modbus) s_modbus->setEnabled(false);
      if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
        Update.printError(Serial);
        g_isUpdatingFirmware = false;
      }
    } else if (upload.status == UPLOAD_FILE_WRITE) {
      esp_task_wdt_reset();
      if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
        Update.printError(Serial);
        g_isUpdatingFirmware = false;
      }
    } else if (upload.status == UPLOAD_FILE_END) {
      esp_task_wdt_reset();
      if (Update.end(true)) {
        Serial.printf("\n[Web OTA] Finished: %u bytes. Rebooting...\n", upload.totalSize);
      } else {
        Update.printError(Serial);
        g_isUpdatingFirmware = false;
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
