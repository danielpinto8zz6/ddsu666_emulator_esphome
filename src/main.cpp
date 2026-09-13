#include <Arduino.h>
#include <esp_task_wdt.h>
#include <esp_bt.h>
#include "config.h"
#include "secrets.h"
#include "meter_data.h"
#include "modbus_rtu.h"
#include "mqtt_manager.h"
#include "web_server.h"

#define WDT_TIMEOUT_SECONDS 15

static ModbusRTUServer s_modbusServer;
static MQTTManager s_mqttManager(s_modbusServer);
static bool s_webInitialized = false;

// High-Priority Real-Time Modbus Task on Core 1
void modbusTask(void *param) {
  Serial.printf("[Modbus Task] Running on Core %d\n", xPortGetCoreID());
  esp_task_wdt_add(NULL);
  s_modbusServer.begin();

  while (true) {
    esp_task_wdt_reset();
    s_modbusServer.handle();
    taskYIELD();
  }
}

// Networking, MQTT, and Web Server Task on Core 0
void networkTask(void *param) {
  Serial.printf("[Network Task] Running on Core %d\n", xPortGetCoreID());
  esp_task_wdt_add(NULL);

  s_mqttManager.begin();

  while (true) {
    esp_task_wdt_reset(); // Feed watchdog

    s_mqttManager.loop();

    // Start web server once Wi-Fi has acquired a valid IP address
    if (s_mqttManager.isWiFiConnected()) {
      if (!s_webInitialized) {
        initWebServer(s_modbusServer, s_mqttManager);
        s_webInitialized = true;
      }
      handleWebServer();
    } else {
      s_webInitialized = false;
    }

    vTaskDelay(pdMS_TO_TICKS(10)); // 10ms loop iteration (100 context switches/sec)
  }
}

void setup() {
  // Reclaim ~30 KB of internal DRAM from unused Bluetooth controller
  esp_bt_controller_mem_release(ESP_BT_MODE_BTDM);

  Serial.begin(115200);
  delay(200);
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
    8192,
    NULL,
    1,
    NULL,
    0 // Pin to Core 0
  );
}

void loop() {
  vTaskDelete(NULL);
}
