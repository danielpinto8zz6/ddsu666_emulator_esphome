#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Update.h>
#include <Preferences.h>
#include <esp_task_wdt.h>
#include <ESPmDNS.h>
#include "chint_reader_config.h"
#include "ddsu666_master.h"
#include "websocket_server_mgr.h"
#include "chint_web_server.h"

// Core Application Components (SOLID Dependency Inversion)
static DDSU666Master           s_modbusMaster;
static WebSocketServerManager  s_wsManager(s_modbusMaster, WS_SERVER_PORT);
static ChintWebServer          s_webServer(s_modbusMaster, s_wsManager, 80);

// Thread-safe IPC Queue between Core 1 (Modbus Master) and Core 0 (Network / WebSockets)
static QueueHandle_t s_telemetryQueue = nullptr;
static bool          s_mdnsStarted = false;

static void checkMdns() {
  if (WiFi.status() == WL_CONNECTED && !s_mdnsStarted) {
    if (MDNS.begin("chint-reader")) {
      Serial.println("[mDNS] Responder active at http://chint-reader.local");
      MDNS.addService("http", "tcp", 80);
      MDNS.addService("ws", "tcp", WS_SERVER_PORT);
      s_mdnsStarted = true;
    }
  } else if (WiFi.status() != WL_CONNECTED) {
    s_mdnsStarted = false;
  }
}

/**
 * @brief Dedicated High-Speed Modbus RTU Master Task on Core 1.
 * Runs in hard real-time without Wi-Fi / TCP interrupt jitter.
 */
static void modbusEngineTask(void *param) {
  Serial.printf("[Modbus Engine] Dedicated Modbus RTU Task active on Core %d\n", xPortGetCoreID());
  esp_task_wdt_add(NULL);

  while (true) {
    esp_task_wdt_reset();
    s_modbusMaster.loop();
    taskYIELD();
  }
}

/**
 * @brief Callback invoked by Modbus Master when a fresh frame is decoded on Core 1.
 */
static void onFreshModbusData(const ChintTelemetry &data) {
  if (s_telemetryQueue) {
    xQueueOverwrite(s_telemetryQueue, &data);
  }
}

void setup() {
  Serial.begin(115200);

#ifdef STATUS_LED_PIN
  pinMode(STATUS_LED_PIN, OUTPUT);
  digitalWrite(STATUS_LED_PIN, HIGH); // OFF (active LOW)
#endif

  // 1-element overwrite queue for thread-safe telemetry handoff Core 1 -> Core 0
  s_telemetryQueue = xQueueCreate(1, sizeof(ChintTelemetry));

  // Connect to Local Wi-Fi Network
  WiFi.mode(WIFI_STA);
  WiFi.setHostname("chint-reader");
  WiFi.setAutoReconnect(true);
  WiFi.setSleep(false); // Disable modem sleep for low-latency (<3ms) WebSocket streaming
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.printf("[WiFi] Connecting to %s", WIFI_SSID);
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 10000) {
    delay(200);
    Serial.print(".");
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("\n[WiFi] Connected! IP: %s\n", WiFi.localIP().toString().c_str());
    checkMdns();
  } else {
    Serial.println("\n[WiFi] Offline start — will reconnect in background.");
  }

  // Initialize Modbus RTU Engine
  s_modbusMaster.begin(DEFAULT_SLAVE_ID);
  s_modbusMaster.onData(onFreshModbusData);

  // Initialize WebSocket Telemetry Streamer (Port 81)
  s_wsManager.begin();

  // Initialize HTTP Web Server & Dashboard UI (Port 80)
  s_webServer.begin();

  // Hardware Task Watchdog (15s timeout)
  esp_task_wdt_init(15, true);

  // Pin Real-Time Modbus RTU Engine to Core 1
  xTaskCreatePinnedToCore(
    modbusEngineTask,
    "ChintModbus",
    4096,
    NULL,
    3,
    NULL,
    1 // Core 1
  );

  Serial.println("[System] Chint Reader Initialized successfully.");
}

void loop() {
  // Service Network Stack (Core 0)
  checkMdns();
  s_webServer.loop();
  s_wsManager.loop();

  // Drain fresh Modbus telemetry from Core 1 and broadcast to active WebSocket clients
  ChintTelemetry data;
  if (s_telemetryQueue && xQueueReceive(s_telemetryQueue, &data, 0) == pdTRUE) {
    if (s_wsManager.hasClients()) {
      ModbusMasterStats stats = s_modbusMaster.getStats();
      s_wsManager.broadcastTelemetry(data, stats);
    }
  }

  vTaskDelay(pdMS_TO_TICKS(2));
}
