#include <Arduino.h>
#include <esp_task_wdt.h>
#include <esp_bt.h>
#include "config.h"
#include "secrets.h"
#include "meter_data.h"
#include "modbus_rtu.h"
#include "config/app_config.h"
#include "telemetry/telemetry_manager.h"
#include "telemetry/sources/chint_ws_source.h"
#include "telemetry/sources/shelly_ws_source.h"
#include "telemetry/sources/opendtu_mqtt_source.h"
#include "telemetry/sources/espnow_pv_source.h"
#include "telemetry/grid_arbitrator.h"
#include "telemetry/pv_arbitrator.h"
#include "web_server.h"
#include <ESPmDNS.h>
#include "mqtt/mqtt_manager.h"
#include "ha_discovery.h"

#define WDT_TIMEOUT_SECONDS 15

// Core Domain & Communication Abstractions
static MeterStateDataProvider s_meterDataProvider;
static ModbusRTUServer        s_modbusServer(&s_meterDataProvider);
static GridFilter             s_gridFilter;
static DefaultGridArbitrator  s_gridArbitrator;
static DefaultPvArbitrator    s_pvArbitrator;
static TelemetryManager       s_telemetryManager(s_modbusServer, &s_gridFilter, &s_gridArbitrator, &s_pvArbitrator);

// Pluggable Technology Drivers (Instantiated in Composition Root)
static ChintWsSource          s_chintSource;
static ShellyWsSource         s_shellySource;
static OpenDtuMqttSource      s_opendtuSource;
static EspNowPvSource         s_espnowSource;

// Home Assistant Discovery & Telemetry Publisher (SOLID: SRP & DIP)
static HaDiscovery            s_haDiscovery;

static bool s_webInitialized = false;
static bool s_mdnsInitialized = false;
volatile UBaseType_t g_networkTaskStackHWM = 0;  // sampled every loop iteration (#4)

// High-Priority Real-Time Modbus Task on Core 1
void modbusTask(void *param) {
  Serial.printf("[Modbus Task] Running on Core %d\n", xPortGetCoreID());
  esp_task_wdt_add(NULL);
  s_modbusServer.begin();

  while (true) {
    esp_task_wdt_reset();
    if (g_isUpdatingFirmware) {
      vTaskDelay(pdMS_TO_TICKS(50));
      continue;
    }
    s_modbusServer.handle();
    taskYIELD();
  }
}

// Networking, Telemetry, and Web Server Task on Core 0
void networkTask(void *param) {
  Serial.printf("[Network Task] Running on Core %d\n", xPortGetCoreID());
  esp_task_wdt_add(NULL);

  // Load configuration from NVS
  AppConfig &cfg = AppConfig::getInstance();
  cfg.begin();
  const SystemSettings &s = cfg.get();

  // Configure sources with persisted settings
  s_chintSource.setIp(s.chint_ip);
  s_shellySource.setIp(s.shelly_ip);
  s_opendtuSource.setBroker(s.mqtt_broker, s.mqtt_port, s.mqtt_user, s.mqtt_pass);

  // Initialize unified MQTT Engine & Home Assistant Auto-Discovery
  MqttManager::getInstance().begin(s.mqtt_broker, s.mqtt_port, s.mqtt_user, s.mqtt_pass);
  s_haDiscovery.begin(&s_modbusServer, &s_telemetryManager);

  // Register sources with TelemetryManager (Dependency Injection / OCP)
  s_telemetryManager.registerSource(&s_chintSource);
  s_telemetryManager.registerSource(&s_shellySource);
  s_telemetryManager.registerSource(&s_opendtuSource);
  s_telemetryManager.registerSource(&s_espnowSource);

  s_telemetryManager.begin();

  while (true) {
    esp_task_wdt_reset(); // Feed watchdog

    s_telemetryManager.loop();

    // Start web server and mDNS responder once Wi-Fi has acquired a valid IP address
    if (s_telemetryManager.isWiFiConnected()) {
      if (!s_mdnsInitialized) {
        if (MDNS.begin("ddsu666")) {
          Serial.println("[mDNS] Responder active at http://ddsu666.local");
          MDNS.addService("http", "tcp", 80);
          MDNS.addService("ws", "tcp", 81);
          s_mdnsInitialized = true;
        }
      }

      if (!s_webInitialized) {
        initWebServer(s_modbusServer, s_telemetryManager);
        s_webInitialized = true;
      }
      handleWebServer();
    } else {
      s_webInitialized = false;
      s_mdnsInitialized = false;
    }

    // Service Unified MQTT Stack & Home Assistant Telemetry cadence
    MqttManager::getInstance().loop();
    s_haDiscovery.loop();

    // Sample stack high-water mark for /health diagnostic (#4)
    g_networkTaskStackHWM = uxTaskGetStackHighWaterMark(NULL);

    vTaskDelay(pdMS_TO_TICKS(10)); // 10ms loop iteration (100 context switches/sec)
  }
}

void setup() {
  // Reclaim ~30 KB of internal DRAM from unused Bluetooth controller
  esp_bt_controller_mem_release(ESP_BT_MODE_BTDM);

  Serial.begin(115200);
#if ARDUINO_USB_CDC_ON_BOOT
  unsigned long startWait = millis();
  while (!Serial && (millis() - startWait < 2000)) {
    delay(10);
  }
#else
  delay(200);
#endif
  Serial.println("\n==================================================");
  Serial.printf(" DDSU666 Modbus RTU Dual Meter Emulator\n");
  Serial.printf(" Board: %s\n", BOARD_NAME);
  Serial.printf(" High-Performance Zero-Export Engine + Web & OTA\n");
  Serial.println("==================================================");

  // Initialize shared thread-safe meter storage & load persisted energy
  MeterState::init();

  // Initialize Hardware Task Watchdog (15s panic timeout)
  esp_task_wdt_init(WDT_TIMEOUT_SECONDS, true);

  // Launch Core 1 Real-Time Modbus Server (Priority 3 - High)
  xTaskCreatePinnedToCore(
    modbusTask,
    "ModbusRTU",
    4096,
    NULL,
    3,
    NULL,
    1 // Pin to Core 1
  );

  // Launch Core 0 Networking & MQTT Engine (Priority 1 - Normal)
  xTaskCreatePinnedToCore(
    networkTask,
    "NetworkTask",
    12288,          // Increased from 8192: WS clients + MQTT + JSON (#4)
    NULL,
    1,
    NULL,
    0 // Pin to Core 0
  );
}

void loop() {
  vTaskDelete(NULL);
}
