#include "chint_web_server.h"
#include "web_dashboard.h"
#include <WiFi.h>
#include <esp_task_wdt.h>

ChintWebServer::ChintWebServer(IChintMeterReader &reader, ITelemetryPublisher &publisher, uint16_t port)
  : _reader(reader), _publisher(publisher), _server(port) {}

void ChintWebServer::begin() {
  const char *headerkeys[] = {"X-OTA-Password", "Authorization"};
  _server.collectHeaders(headerkeys, 2);

  _server.on("/", HTTP_ANY, [this]() { handleRoot(); });
  _server.on("/health", HTTP_ANY, [this]() { handleHealthJson(); });
  _server.on("/status", HTTP_ANY, [this]() { handleHealthJson(); });

  _server.on("/api/settings", HTTP_ANY, [this]() { handleSettingsApi(); });
  _server.on("/set_slave", HTTP_ANY, [this]() { handleSettingsApi(); });

  _server.on("/reboot", HTTP_POST, [this]() { handleReboot(); });

  setupOtaRoutes();

  _server.begin();
  Serial.println("[Web Server] HTTP Web Server ready on port 80");
}

void ChintWebServer::loop() {
  _server.handleClient();
}

void ChintWebServer::handleRoot() {
  _server.sendHeader("Connection", "keep-alive");
  _server.send_P(200, "text/html; charset=utf-8", CHINT_DASHBOARD_HTML);
}

void ChintWebServer::handleHealthJson() {
  ChintTelemetry data = _reader.getTelemetry();
  ModbusMasterStats stats = _reader.getStats();

  StaticJsonDocument<1024> doc;
  doc["status"] = stats.meter_online ? "HEALTHY" : "DEGRADED";
#if defined(BOARD_LILYGO_TCAN485)
  doc["device"] = "LilyGO T-CAN485 Chint Master (Clean Core Split)";
#else
  doc["device"] = "Seeed Studio XIAO ESP32-S3 Chint Master (Clean Core Split)";
#endif
  doc["uptime_sec"] = (uint32_t)(millis() / 1000);
  doc["free_heap"] = (uint32_t)ESP.getFreeHeap();

  JsonObject wifi = doc.createNestedObject("wifi");
  wifi["connected"] = (WiFi.status() == WL_CONNECTED);
  wifi["ip"] = WiFi.localIP().toString();
  wifi["rssi"] = WiFi.RSSI();

  JsonObject mb = doc.createNestedObject("modbus");
  mb["meter_online"] = stats.meter_online;
  mb["slave_id"] = stats.slave_id;
  mb["rate_hz"] = stats.query_rate_hz;
  mb["latency_ms"] = stats.response_latency_ms;
  mb["total_queries"] = stats.total_queries;
  mb["success"] = stats.successful_queries;
  mb["timeouts"] = stats.timeout_errors;
  mb["crc_errors"] = stats.crc_errors;

  JsonObject tel = doc.createNestedObject("telemetry");
  tel["watts"] = data.active_power;
  tel["volts"] = data.voltage;
  tel["amps"] = data.current;
  tel["pf"] = data.power_factor;
  tel["freq"] = data.frequency;
  tel["import_kwh"] = data.import_kwh;
  tel["export_kwh"] = data.export_kwh;
  tel["last_update_ms"] = data.last_update;

  JsonObject ws = doc.createNestedObject("websocket");
  ws["port"] = 81;
  ws["clients"] = _publisher.getClientCount();
  ws["packets_sent"] = _publisher.getPacketsBroadcast();
  JsonArray clientIps = ws.createNestedArray("client_ips");
  for (const String &ip : _publisher.getClientIPs()) {
    clientIps.add(ip);
  }

  char buf[1024];
  serializeJson(doc, buf, sizeof(buf));
  _server.sendHeader("Access-Control-Allow-Origin", "*");
  _server.send(200, "application/json", buf);
}

void ChintWebServer::handleSettingsApi() {
  uint8_t slaveId = 0;
  if (_server.hasArg("slave")) {
    slaveId = (uint8_t)_server.arg("slave").toInt();
  } else if (_server.hasArg("id")) {
    slaveId = (uint8_t)_server.arg("id").toInt();
  }

  if (slaveId >= 1 && slaveId <= 247) {
    _reader.setSlaveId(slaveId);
    _server.sendHeader("Access-Control-Allow-Origin", "*");
    _server.send(200, "application/json", "{\"status\":\"OK\",\"slave_id\":" + String(slaveId) + "}");
    return;
  }

  _server.sendHeader("Access-Control-Allow-Origin", "*");
  _server.send(400, "application/json", "{\"error\":\"Invalid slave ID. Must be 1-247.\"}");
}

void ChintWebServer::handleReboot() {
  _server.sendHeader("Connection", "close");
  _server.send(200, "text/plain", "Rebooting device...");
  delay(500);
  ESP.restart();
}

void ChintWebServer::setupOtaRoutes() {
  // Web OTA Upload Form
  _server.on("/update", HTTP_GET, [this]() {
    _server.send(200, "text/html; charset=utf-8",
      "<!DOCTYPE html><html lang='en'><head>"
      "<meta charset='utf-8'>"
      "<meta name='viewport' content='width=device-width,initial-scale=1'>"
      "<title>Chint Reader OTA Update</title>"
      "<style>"
      ":root { --bg: #0f172a; --card-bg: #1e293b; --accent: #38bdf8; --green: #10b981; --border: rgba(255,255,255,0.1); --text: #f8fafc; --muted: #94a3b8; }"
      "* { box-sizing: border-box; margin: 0; padding: 0; font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif; }"
      "body { background: var(--bg); color: var(--text); display: flex; justify-content: center; align-items: center; min-height: 100vh; padding: 20px; }"
      ".card { background: var(--card-bg); padding: 32px; border-radius: 16px; border: 1px solid var(--border); text-align: center; box-shadow: 0 10px 25px -5px rgba(0,0,0,0.5); max-width: 400px; width: 100%; }"
      "h2 { font-size: 1.3rem; margin-bottom: 8px; }"
      "p { font-size: 0.85rem; color: var(--muted); margin-bottom: 20px; }"
      "input[type=file] { width: 100%; padding: 10px; background: rgba(0,0,0,0.2); border: 1px dashed var(--border); border-radius: 8px; color: var(--muted); font-size: 0.85rem; cursor: pointer; }"
      "button { margin-top: 16px; width: 100%; padding: 12px; border-radius: 8px; border: none; background: #0284c7; color: #fff; font-weight: 700; font-size: 0.95rem; cursor: pointer; transition: background 0.2s; }"
      "button:hover { background: #0369a1; }"
      "a { display: inline-block; margin-top: 16px; color: var(--accent); text-decoration: none; font-size: 0.85rem; }"
      "a:hover { text-decoration: underline; }"
      "</style></head><body>"
      "<div class='card'>"
      "<h2>📦 Chint Reader OTA Update</h2>"
      "<p>Upload a compiled firmware (.bin) to update Seeed Studio XIAO ESP32-S3 wirelessly.</p>"
      "<form method='POST' action='/update' enctype='multipart/form-data'>"
      "<input type='file' name='file' accept='.bin' required><br>"
      "<button type='submit'>Upload &amp; Flash Firmware</button>"
      "</form>"
      "<a href='/'>&larr; Back to Live Dashboard</a>"
      "</div></body></html>");
  });

  // Web OTA Post Handler (Passwordless)
  _server.on("/update", HTTP_POST, [this]() {
    _server.sendHeader("Connection", "close");
    _server.send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
    delay(500);
    ESP.restart();
  }, [this]() {
    HTTPUpload &upload = _server.upload();
    esp_task_wdt_reset();
    if (upload.status == UPLOAD_FILE_START) {
      Serial.printf("[OTA] Starting firmware update: %s\n", upload.filename.c_str());
      if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_WRITE) {
      if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_END) {
      if (Update.end(true)) {
        Serial.printf("[OTA] Update Success! Size: %u bytes. Rebooting...\n", upload.totalSize);
      } else {
        Update.printError(Serial);
      }
    }
  });
}
