#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Update.h>
#include <WebSocketsServer.h>
#include <ArduinoJson.h>
#include <esp_task_wdt.h>
#include "chint_reader_config.h"
#include "ddsu666_master.h"

// Global instances
DDSU666Master g_ddsu666;
WebSocketsServer g_ws(WS_SERVER_PORT);
WebServer g_httpServer(80);

// Thread-safe queue between Core 1 (Modbus) and Core 0 (WebSockets)
static QueueHandle_t s_telemetryQueue = nullptr;
static uint32_t s_wsPacketsSent = 0;
static uint8_t s_wsClientCount = 0;

// Dedicated Real-Time Modbus Master Task on Core 1
void modbusTask(void *param) {
  Serial.printf("[Modbus Task] Running on Core %d (Dedicated Modbus Engine)\n", xPortGetCoreID());
  esp_task_wdt_add(NULL);

  while (true) {
    esp_task_wdt_reset();
    g_ddsu666.loop();
    taskYIELD();
  }
}

// Callback invoked by Modbus Master when a fresh reading arrives on Core 1
static void onFreshModbusData(const ChintTelemetry &data) {
  if (s_telemetryQueue) {
    xQueueOverwrite(s_telemetryQueue, &data);
  }
}

// WebSocket Event Handler (Runs on Core 0)
static void onWsEvent(uint8_t num, WStype_t type, uint8_t *payload, size_t length) {
  switch (type) {
    case WStype_DISCONNECTED:
      if (s_wsClientCount > 0) s_wsClientCount--;
      Serial.printf("[WS] Client #%u disconnected (active: %u)\n", num, s_wsClientCount);
      break;

    case WStype_CONNECTED: {
      s_wsClientCount++;
      IPAddress ip = g_ws.remoteIP(num);
      Serial.printf("[WS] Client #%u connected from %s (active: %u)\n", num, ip.toString().c_str(), s_wsClientCount);
      // Immediately send welcome packet with current telemetry
      ChintTelemetry data = g_ddsu666.getTelemetry();
      char buf[256];
      snprintf(buf, sizeof(buf),
        "{\"type\":\"welcome\",\"device\":\"Chint DDSU666 Master\","
        "\"watts\":%.1f,\"volts\":%.1f,\"amps\":%.2f,\"pf\":%.2f,\"freq\":%.1f,"
        "\"import_kwh\":%.2f,\"export_kwh\":%.2f,\"meter_online\":%s}",
        data.active_power, data.voltage, data.current, data.power_factor, data.frequency,
        data.import_kwh, data.export_kwh, g_ddsu666.getStats().meter_online ? "true" : "false"
      );
      g_ws.sendTXT(num, buf);
      break;
    }

    default:
      break;
  }
}

// Check OTA authorization (passwordless)
static bool isOtaAuthorized() {
  return true;
}

void setup() {
  Serial.begin(115200);

  // Initialize status LED
#ifdef STATUS_LED_PIN
  pinMode(STATUS_LED_PIN, OUTPUT);
  digitalWrite(STATUS_LED_PIN, HIGH);
#endif

  // 1-element queue for Core 1 -> Core 0 telemetry handoff
  s_telemetryQueue = xQueueCreate(1, sizeof(ChintTelemetry));

  // Connect to Wi-Fi
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.printf("[WiFi] Connecting to %s", WIFI_SSID);
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 10000) {
    delay(200);
    Serial.print(".");
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("\n[WiFi] Connected! IP: %s\n", WiFi.localIP().toString().c_str());
  } else {
    Serial.println("\n[WiFi] Offline start — will retry in background.");
  }

  // Initialize Modbus Master
  g_ddsu666.begin(DEFAULT_SLAVE_ID);
  g_ddsu666.onData(onFreshModbusData);

  // Initialize WebSockets Server on Port 81
  g_ws.begin();
  g_ws.onEvent(onWsEvent);
  Serial.printf("[WS] Server listening on ws://0.0.0.0:%d/\n", WS_SERVER_PORT);

  // Initialize Web Server on Port 80 for OTA & Health
  const char *headerkeys[] = {"X-OTA-Password", "Authorization"};
  g_httpServer.collectHeaders(headerkeys, 2);

  g_httpServer.on("/", HTTP_GET, []() {
    g_httpServer.sendHeader("Location", "/update");
    g_httpServer.send(302, "text/plain", "");
  });

  g_httpServer.on("/health", HTTP_GET, []() {
    ChintTelemetry data = g_ddsu666.getTelemetry();
    ModbusMasterStats stats = g_ddsu666.getStats();
    StaticJsonDocument<768> doc;
    doc["status"] = stats.meter_online ? "HEALTHY" : "DEGRADED";
    doc["device"] = "Seeed Studio XIAO ESP32-S3 Chint Master (Clean Core Split)";
    doc["uptime_sec"] = (uint32_t)(millis() / 1000);
    doc["free_heap"] = (uint32_t)ESP.getFreeHeap();

    JsonObject wifi = doc.createNestedObject("wifi");
    wifi["connected"] = (WiFi.status() == WL_CONNECTED);
    wifi["ip"] = WiFi.localIP().toString();
    wifi["rssi"] = WiFi.RSSI();

    JsonObject mb = doc.createNestedObject("modbus");
    mb["meter_online"] = stats.meter_online;
    mb["rate_hz"] = stats.query_rate_hz;
    mb["latency_ms"] = stats.response_latency_ms;
    mb["success"] = stats.successful_queries;
    mb["timeouts"] = stats.timeout_errors;

    JsonObject tel = doc.createNestedObject("telemetry");
    tel["watts"] = data.active_power;
    tel["volts"] = data.voltage;
    tel["amps"] = data.current;
    tel["pf"] = data.power_factor;
    tel["freq"] = data.frequency;
    tel["import_kwh"] = data.import_kwh;
    tel["export_kwh"] = data.export_kwh;

    JsonObject ws = doc.createNestedObject("websocket");
    ws["port"] = WS_SERVER_PORT;
    ws["clients"] = s_wsClientCount;
    ws["packets_sent"] = s_wsPacketsSent;

    char buf[768];
    serializeJson(doc, buf, sizeof(buf));
    g_httpServer.sendHeader("Access-Control-Allow-Origin", "*");
    g_httpServer.send(200, "application/json", buf);
  });

  g_httpServer.on("/set_slave", HTTP_GET, []() {
    if (g_httpServer.hasArg("id")) {
      uint8_t id = g_httpServer.arg("id").toInt();
      if (id >= 1 && id <= 247) {
        g_ddsu666.setSlaveId(id);
        g_httpServer.send(200, "text/plain", "OK");
        return;
      }
    }
    g_httpServer.send(400, "text/plain", "Invalid ID");
  });

  // OTA Endpoints (Passwordless)
  g_httpServer.on("/update", HTTP_GET, []() {
    g_httpServer.send(200, "text/html",
      "<!DOCTYPE html><html><head><title>Chint Reader OTA</title>"
      "<meta name='viewport' content='width=device-width,initial-scale=1'>"
      "<style>body{font-family:sans-serif;background:#121214;color:#eee;display:flex;justify-content:center;align-items:center;height:100vh;margin:0;}"
      ".card{background:#1e1e24;padding:32px;border-radius:12px;border:1px solid #333;text-align:center;box-shadow:0 8px 24px rgba(0,0,0,0.4);}"
      "input,button{margin-top:16px;padding:10px 16px;border-radius:8px;border:none;font-size:1rem;}"
      "button{background:#10b981;color:#fff;cursor:pointer;font-weight:bold;}"
      "</style></head><body>"
      "<div class='card'>"
      "<h2>📦 Chint Reader OTA Update</h2>"
      "<form method='POST' action='/update' enctype='multipart/form-data'>"
      "<input type='file' name='file' accept='.bin' required><br>"
      "<button type='submit'>Upload & Flash</button>"
      "</form></div></body></html>");
  });

  g_httpServer.on("/update", HTTP_POST, []() {
    g_httpServer.sendHeader("Connection", "close");
    g_httpServer.send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
    delay(500);
    ESP.restart();
  }, []() {
    HTTPUpload &upload = g_httpServer.upload();
    esp_task_wdt_reset();
    if (upload.status == UPLOAD_FILE_START) {
      if (!isOtaAuthorized()) return;
      Serial.printf("[OTA] Starting update: %s\n", upload.filename.c_str());
      if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_WRITE) {
      if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_END) {
      if (Update.end(true)) {
        Serial.printf("[OTA] Update Success! Size: %u bytes\n", upload.totalSize);
      } else {
        Update.printError(Serial);
      }
    }
  });

  g_httpServer.begin();

  // Watchdog
  esp_task_wdt_init(15, true);

  // Launch Core 1 Modbus Task
  xTaskCreatePinnedToCore(
    modbusTask,
    "ChintModbus",
    4096,
    NULL,
    3,
    NULL,
    1 // Core 1
  );
}

void loop() {
  g_httpServer.handleClient();
  g_ws.loop();

  // Drain fresh Modbus telemetry from Core 1 and broadcast on Core 0
  ChintTelemetry data;
  if (s_telemetryQueue && xQueueReceive(s_telemetryQueue, &data, 0) == pdTRUE) {
    if (s_wsClientCount > 0) {
      char buf[256];
      snprintf(buf, sizeof(buf),
        "{\"type\":\"telemetry\",\"watts\":%.1f,\"volts\":%.1f,\"amps\":%.2f,"
        "\"pf\":%.2f,\"freq\":%.1f,\"import_kwh\":%.2f,\"export_kwh\":%.2f,\"meter_online\":true}",
        data.active_power, data.voltage, data.current, data.power_factor, data.frequency,
        data.import_kwh, data.export_kwh
      );
      g_ws.broadcastTXT(buf);
      s_wsPacketsSent++;
    }
  }

  vTaskDelay(pdMS_TO_TICKS(2));
}
