#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <WebSocketsClient.h>
#include <ArduinoJson.h>
#include <WebServer.h>
#include <Update.h>
#include "transmitter_config.h"
#include "packet_format.h"
#include "lolin_web_ui.h"

static WebSocketsClient s_wsClient;
static esp_now_peer_info_t s_peerInfo;
static WebServer s_server(80);

static uint8_t s_targetMac[6];
static bool s_espNowReady = false;
static bool s_shellyConnected = false;

static uint32_t s_packetsSent = 0;
static uint32_t s_packetsSuccess = 0;
static uint32_t s_packetsFail = 0;

static uint32_t s_lastPollTime = 0;
static uint32_t s_lastEnergyPollTime = 0;
static uint32_t s_lastStatusPrint = 0;

static float s_lastWatts = 0.0f;
static float s_lastVolts = 0.0f;
static float s_lastCurrent = 0.0f;
static float s_lastPf = 1.0f;
static float s_lastFreq = 50.0f;
static float s_cachedEnergyKwh = -1.0f;

static uint32_t s_lastShellyMsgTime = 0;
static uint32_t s_lastEspNowTxTime = 0;
static char s_jsonBuffer[768];
#ifdef STATUS_LED_PIN
static uint32_t s_ledPulseTime = 0;
#endif

// ESP-NOW Send Callback (Reports 802.11 ACK status from receiver)
static void onDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  if (status == ESP_NOW_SEND_SUCCESS) {
    s_packetsSuccess++;
  } else {
    s_packetsFail++;
  }
}

static void initEspNow() {
  if (s_espNowReady) return;

  if (esp_now_init() != ESP_OK) {
    Serial.println("[ESP-NOW] Error: Failed to initialize!");
    return;
  }

  esp_now_register_send_cb(onDataSent);
  esp_wifi_config_espnow_rate(WIFI_IF_STA, WIFI_PHY_RATE_1M_L);

  if (USE_BROADCAST) {
    memset(s_targetMac, 0xFF, 6);
  } else {
    memcpy(s_targetMac, LILYGO_MAC, 6);
  }

  memset(&s_peerInfo, 0, sizeof(s_peerInfo));
  memcpy(s_peerInfo.peer_addr, s_targetMac, 6);
  s_peerInfo.channel = WiFi.channel();
  s_peerInfo.encrypt = false;

  if (esp_now_add_peer(&s_peerInfo) != ESP_OK) {
    Serial.println("[ESP-NOW] Error: Failed to add target peer!");
    return;
  }

  s_espNowReady = true;
  Serial.printf("[ESP-NOW] Peer configured! Target: %02X:%02X:%02X:%02X:%02X:%02X on Channel %d\n",
                s_targetMac[0], s_targetMac[1], s_targetMac[2],
                s_targetMac[3], s_targetMac[4], s_targetMac[5],
                WiFi.channel());
}

static void transmitEspNow(float watts, float voltage, float current, float pf, float freq, float energyKwh) {
  if (!s_espNowReady) return;

  EspNowPowerPacket pkt;
  pkt.magic = ESPNOW_MAGIC_BYTE;
  pkt.packet_type = ESPNOW_TYPE_GRID;
  pkt.active_power = watts;
  pkt.voltage = voltage;
  pkt.current = current;
  pkt.power_factor = pf;
  pkt.frequency = freq;
  pkt.energy_kwh = energyKwh;

  esp_err_t err = esp_now_send(s_targetMac, (const uint8_t *)&pkt, sizeof(pkt));
  s_packetsSent++;
  s_lastEspNowTxTime = millis();
#ifdef STATUS_LED_PIN
  digitalWrite(STATUS_LED_PIN, LOW); // Active LOW: Pulse LED ON on packet transmission
  s_ledPulseTime = millis();
#endif

  if (err != ESP_OK) {
    Serial.printf("[ESP-NOW] Send error code: %d\n", err);
  }
}

static void handleShellyJson(const uint8_t *payload, size_t length) {
  static StaticJsonDocument<256> filter;
  static bool s_filterInit = false;
  if (!s_filterInit) {
    filter["result"]["em1:0"] = true;
    filter["result"]["em1data:0"] = true;
    filter["result"]["voltage"] = true;
    filter["result"]["current"] = true;
    filter["result"]["act_power"] = true;
    filter["result"]["aprt_power"] = true;
    filter["result"]["pf"] = true;
    filter["result"]["freq"] = true;
    filter["result"]["total_act_energy"] = true;
    filter["params"]["em1:0"] = true;
    filter["params"]["em1data:0"] = true;
    s_filterInit = true;
  }

  StaticJsonDocument<512> doc;
  DeserializationError err = deserializeJson(doc, payload, length, DeserializationOption::Filter(filter));
  if (err) return;

  JsonObject em1;
  JsonObject em1data;

  if (doc.containsKey("result")) {
    JsonObject res = doc["result"];
    if (res.containsKey("em1:0")) {
      em1 = res["em1:0"];
    } else if (res.containsKey("voltage")) {
      em1 = res;
    }

    if (res.containsKey("em1data:0")) {
      em1data = res["em1data:0"];
    } else if (res.containsKey("total_act_energy")) {
      em1data = res;
    }
  } else if (doc.containsKey("params")) {
    JsonObject par = doc["params"];
    if (par.containsKey("em1:0")) {
      em1 = par["em1:0"];
    }
    if (par.containsKey("em1data:0")) {
      em1data = par["em1data:0"];
    }
  }

  // Check for Energy counters
  if (!em1data.isNull() && em1data.containsKey("total_act_energy")) {
    float wh = em1data["total_act_energy"].as<float>();
    if (!std::isnan(wh) && wh >= 0.0f) {
      s_cachedEnergyKwh = wh / 1000.0f;
    }
  }

  // Check for Real-Time Telemetry
  if (!em1.isNull() && em1.containsKey("voltage")) {
    float v = em1["voltage"].as<float>();
    if (!std::isnan(v) && v >= 180.0f && v <= 280.0f) {
      float act_power = 0.0f;
      if (em1.containsKey("act_power")) {
        float raw_p = em1["act_power"].as<float>();
        if (!std::isnan(raw_p) && raw_p > -15000.0f && raw_p < 15000.0f) {
          act_power = raw_p;
        }
      }

      float current = 0.0f;
      if (em1.containsKey("current")) {
        float raw_c = em1["current"].as<float>();
        if (!std::isnan(raw_c) && raw_c >= 0.0f && raw_c < 250.0f) {
          current = raw_c;
        }
      } else {
        current = std::fabs(act_power) / v;
      }

      float pf = 1.0f;
      if (em1.containsKey("pf")) {
        float raw_pf = em1["pf"].as<float>();
        if (!std::isnan(raw_pf) && raw_pf >= -1.0f && raw_pf <= 1.0f && raw_pf != 0.0f) {
          pf = raw_pf;
        }
      }

      float freq = 50.0f;
      if (em1.containsKey("freq")) {
        float raw_f = em1["freq"].as<float>();
        if (!std::isnan(raw_f) && raw_f > 45.0f && raw_f < 55.0f) {
          freq = raw_f;
        }
      }

      s_lastWatts = act_power;
      s_lastVolts = v;
      s_lastCurrent = current;
      s_lastPf = pf;
      s_lastFreq = freq;
      s_lastShellyMsgTime = millis();

      // Immediately transmit over ESP-NOW to Lilygo!
      transmitEspNow(act_power, v, current, pf, freq, s_cachedEnergyKwh);
    }
  }
}

static void onWsEvent(WStype_t type, uint8_t *payload, size_t length) {
  switch (type) {
    case WStype_DISCONNECTED:
      s_shellyConnected = false;
      Serial.println("[Shelly WS] Disconnected");
      break;

    case WStype_CONNECTED:
      s_shellyConnected = true;
      Serial.printf("[Shelly WS] Connected to ws://%s:%d%s\n", SHELLY_IP, SHELLY_WS_PORT, SHELLY_WS_PATH);
      // Query initial status and immediate energy accumulators
      s_wsClient.sendTXT("{\"id\":10,\"src\":\"lolin_c3\",\"method\":\"Shelly.GetStatus\"}");
      s_wsClient.sendTXT("{\"id\":11,\"src\":\"lolin_c3\",\"method\":\"EM1Data.GetStatus\",\"params\":{\"id\":0}}");
      break;

    case WStype_TEXT:
      handleShellyJson(payload, length);
      break;

    case WStype_ERROR:
      Serial.println("[Shelly WS] Communication error");
      break;

    default:
      break;
  }
}

static void handleHealthJson() {
  uint32_t now = millis();
  float shellyAge = (s_lastShellyMsgTime != 0) ? (now - s_lastShellyMsgTime) / 1000.0f : -1.0f;
  float espnowAge = (s_lastEspNowTxTime != 0) ? (now - s_lastEspNowTxTime) / 1000.0f : -1.0f;

  float ackRate = (s_packetsSent > 0) ? ((float)s_packetsSuccess / (float)s_packetsSent * 100.0f) : 100.0f;
  bool healthy = (WiFi.status() == WL_CONNECTED) && s_shellyConnected && s_espNowReady;

  char targetMacStr[18];
  snprintf(targetMacStr, sizeof(targetMacStr), "%02X:%02X:%02X:%02X:%02X:%02X",
           s_targetMac[0], s_targetMac[1], s_targetMac[2],
           s_targetMac[3], s_targetMac[4], s_targetMac[5]);

  snprintf(s_jsonBuffer, sizeof(s_jsonBuffer),
    "{"
      "\"status\":\"%s\","
#if defined(BOARD_ESP32C3_SUPERMINI)
      "\"device\":\"ESP32-C3 SuperMini Bridge\","
#else
      "\"device\":\"ESP32-C3 Transmitter Bridge\","
#endif
      "\"uptime_sec\":%lu,"
      "\"free_heap\":%lu,"
      "\"wifi\":{\"connected\":%s,\"ip\":\"%s\",\"rssi\":%d,\"channel\":%d},"
      "\"shelly\":{\"connected\":%s,\"ip\":\"%s\",\"age_sec\":%.1f,\"watts\":%.1f,\"volts\":%.1f,\"amps\":%.2f,\"pf\":%.2f,\"freq\":%.1f,\"energy_kwh\":%.2f},"
      "\"espnow\":{\"mode\":\"%s\",\"target_mac\":\"%s\",\"channel\":%d,\"sent\":%lu,\"ack\":%lu,\"fail\":%lu,\"ack_rate_pct\":%.1f,\"age_sec\":%.1f}"
    "}",
    healthy ? "HEALTHY" : (s_shellyConnected ? "DEGRADED" : "OFFLINE"),
    (unsigned long)(millis() / 1000),
    (unsigned long)ESP.getFreeHeap(),
    (WiFi.status() == WL_CONNECTED) ? "true" : "false",
    WiFi.localIP().toString().c_str(),
    WiFi.RSSI(),
    WiFi.channel(),
    s_shellyConnected ? "true" : "false",
    SHELLY_IP,
    shellyAge,
    s_lastWatts,
    s_lastVolts,
    s_lastCurrent,
    s_lastPf,
    s_lastFreq,
    s_cachedEnergyKwh,
    USE_BROADCAST ? "BROADCAST" : "UNICAST",
    targetMacStr,
    WiFi.channel(),
    (unsigned long)s_packetsSent,
    (unsigned long)s_packetsSuccess,
    (unsigned long)s_packetsFail,
    ackRate,
    espnowAge
  );

  s_server.sendHeader("Access-Control-Allow-Origin", "*");
  s_server.send(200, "application/json", s_jsonBuffer);
}

static void handleRoot() {
  s_server.sendHeader("Content-Encoding", "gzip");
  s_server.send_P(200, "text/html", (const char *)LOLIN_HTML_GZ, LOLIN_HTML_GZ_LEN);
}

static void initWebServer() {
  s_server.on("/", handleRoot);
  s_server.on("/health", handleHealthJson);
  s_server.on("/status", handleHealthJson);

  // Wireless Web Browser OTA route
  s_server.on("/update", HTTP_POST, []() {
    s_server.sendHeader("Connection", "close");
    s_server.send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
    delay(500);
    ESP.restart();
  }, []() {
    HTTPUpload& upload = s_server.upload();
    if (upload.status == UPLOAD_FILE_START) {
      Serial.printf("\n[Web OTA] Upload starting: %s\n", upload.filename.c_str());
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
  Serial.printf("[Web Server] Dashboard live at http://%s/\n", WiFi.localIP().toString().c_str());
  Serial.printf("[Web Server] Health API at http://%s/health\n", WiFi.localIP().toString().c_str());
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("\n==================================================");
#if defined(BOARD_ESP32C3_SUPERMINI)
  Serial.println(" ESP32-C3 SuperMini: Shelly WS -> ESP-NOW Bridge");
#else
  Serial.println(" ESP32-C3: Shelly WS -> ESP-NOW Bridge");
#endif
  Serial.println(" Direct Zero-Latency Bridge to Lilygo DDSU666");
  Serial.println("==================================================");

#ifdef STATUS_LED_PIN
  pinMode(STATUS_LED_PIN, OUTPUT);
  digitalWrite(STATUS_LED_PIN, HIGH); // Initially OFF (Active LOW)
#endif

  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.setSleep(false);
  WiFi.setTxPower(WIFI_POWER_19_5dBm);

  Serial.printf("[WiFi] Connecting to %s...\n", WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  while (WiFi.status() != WL_CONNECTED) {
    delay(200);
    Serial.print(".");
#ifdef STATUS_LED_PIN
    digitalWrite(STATUS_LED_PIN, !digitalRead(STATUS_LED_PIN)); // Blink during connection
#endif
  }
#ifdef STATUS_LED_PIN
  digitalWrite(STATUS_LED_PIN, HIGH); // OFF once connected
#endif

  Serial.println("\n[WiFi] Connected!");
  Serial.printf("[WiFi] IP Address: %s\n", WiFi.localIP().toString().c_str());
  Serial.printf("[WiFi] Signal RSSI: %d dBm\n", WiFi.RSSI());
  Serial.printf("[WiFi] Radio Channel: %d (Locks ESP-NOW to this channel)\n", WiFi.channel());

  // Initialize ESP-NOW
  initEspNow();

  // Initialize Web Dashboard & Health API
  initWebServer();

  // Connect to Shelly Pro EM
  Serial.printf("[Shelly WS] Connecting to ws://%s:%d%s\n", SHELLY_IP, SHELLY_WS_PORT, SHELLY_WS_PATH);
  s_wsClient.begin(SHELLY_IP, SHELLY_WS_PORT, SHELLY_WS_PATH);
  s_wsClient.onEvent(onWsEvent);
  s_wsClient.setReconnectInterval(2000);

  s_lastPollTime = millis();
  s_lastEnergyPollTime = millis();
  s_lastStatusPrint = millis();
}

void loop() {
  s_server.handleClient();
  s_wsClient.loop();

  uint32_t now = millis();

#ifdef STATUS_LED_PIN
  if (s_ledPulseTime != 0 && (now - s_ledPulseTime >= 20)) {
    digitalWrite(STATUS_LED_PIN, HIGH); // Turn OFF after 20ms pulse
    s_ledPulseTime = 0;
  }
#endif

  // Re-verify Wi-Fi connection
  if (WiFi.status() != WL_CONNECTED) {
    s_espNowReady = false;
    s_shellyConnected = false;
    delay(100);
    return;
  } else if (!s_espNowReady) {
    initEspNow();
  }

  // Periodic real-time status polling (EM1.GetStatus every 200ms)
  if (s_shellyConnected && (now - s_lastPollTime >= SHELLY_POLL_MS)) {
    s_lastPollTime = now;
    s_wsClient.sendTXT("{\"id\":1,\"src\":\"lolin_c3\",\"method\":\"EM1.GetStatus\",\"params\":{\"id\":0}}");
  }

  // Periodic energy counter polling (every 60s)
  if (s_shellyConnected && (now - s_lastEnergyPollTime >= SHELLY_ENERGY_POLL_MS)) {
    s_lastEnergyPollTime = now;
    s_wsClient.sendTXT("{\"id\":2,\"src\":\"lolin_c3\",\"method\":\"EM1Data.GetStatus\",\"params\":{\"id\":0}}");
  }

  // Console Telemetry Dashboard every 3 seconds
  if (now - s_lastStatusPrint >= 3000) {
    s_lastStatusPrint = now;
    Serial.printf("[BRIDGE STATS] Shelly WS: %s | ESP-NOW Sent: %lu (ACK: %lu, Fail: %lu)\n",
                  s_shellyConnected ? "ONLINE" : "OFFLINE",
                  (unsigned long)s_packetsSent,
                  (unsigned long)s_packetsSuccess,
                  (unsigned long)s_packetsFail);
  }
}
