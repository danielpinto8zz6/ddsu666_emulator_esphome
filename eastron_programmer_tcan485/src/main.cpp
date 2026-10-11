#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include "eastron_modbus.h"
#include "web_page.h"

#if __has_include("secrets.h")
#include "secrets.h"
#endif

// LILYGO T-CAN485 Pin Definitions
constexpr int PIN_RS485_TX      = 22;
constexpr int PIN_RS485_RX      = 21;
constexpr int PIN_RS485_DE_RE   = 17;
constexpr int PIN_RS485_5V_EN   = 16;
constexpr int PIN_RS485_CHIP_EN = 19;

EastronModbus meter(PIN_RS485_TX, PIN_RS485_RX, PIN_RS485_DE_RE, PIN_RS485_5V_EN, PIN_RS485_CHIP_EN);
WebServer server(80);

void runBenchmark(uint32_t durationSec);

void printMenu() {
  Serial.println("\n--------------------------------------------------------------");
  Serial.println("  ⚡ LILYGO T-CAN485 — EASTRON SDM120 PROGRAMMER MENU");
  Serial.println("--------------------------------------------------------------");
  Serial.println("  [1] 🔍 Auto-Scan Bus (Find meter baud rate & Slave ID)");
  Serial.println("  [2] 🚀 Quick Program -> 9600 Baud, Slave ID 3 (Hoymiles Bus)");
  Serial.println("  [3] 📊 Read Live Telemetry (Voltage, Watts, Amps, Hz, kWh)");
  Serial.println("  [4] 🔄 Reset to Factory Defaults -> 2400 Baud, Slave ID 1");
  Serial.println("  [5] 🛠️ Custom Slave ID & Baud Rate");
  Serial.println("  [6] ⚡ 30-Second Refresh Rate & Throughput Benchmark");
  Serial.println("--------------------------------------------------------------");
  Serial.print("Select an option [1-6]: ");
}

void handleRoot() {
  server.send_P(200, "text/html", EASTON_WEB_PAGE);
}

void handleApiScan() {
  Serial.println("\n[Web API] Bus scan requested...");
  auto found = meter.scanBus();
  
  StaticJsonDocument<1024> doc;
  JsonArray arr = doc.createNestedArray("found");
  String logMsg = "Bus Scan Results:\n";

  if (found.empty()) {
    logMsg += "No meters responded. Check wiring (A/B) and ensure meter is powered.\n";
  } else {
    for (const auto &m : found) {
      JsonObject obj = arr.createNestedObject();
      obj["id"] = m.slave_id;
      obj["baud"] = m.baud_rate;
      obj["voltage"] = m.voltage;
      logMsg += "• Found Meter: Slave ID " + String(m.slave_id) + " @ " + String(m.baud_rate) + " bps (Voltage: " + String(m.voltage, 1) + " V)\n";
    }
  }

  doc["success"] = !found.empty();
  doc["log"] = logMsg;

  String resp;
  serializeJson(doc, resp);
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "application/json", resp);
}

void handleApiProgram() {
  uint8_t curId   = server.hasArg("cur_id")   ? (uint8_t)server.arg("cur_id").toInt()   : 0;
  uint32_t curBaud = server.hasArg("cur_baud") ? (uint32_t)server.arg("cur_baud").toInt() : 0;
  uint8_t newId   = server.hasArg("new_id")   ? (uint8_t)server.arg("new_id").toInt()   : 3;
  uint32_t newBaud = server.hasArg("new_baud") ? (uint32_t)server.arg("new_baud").toInt() : 9600;

  Serial.printf("\n[Web API] Programming request: Cur ID %u (%u baud) -> New ID %u (%u baud)\n", curId, curBaud, newId, newBaud);
  String logMsg;
  bool ok = meter.programMeter(curId, curBaud, newId, newBaud, logMsg);

  StaticJsonDocument<1024> doc;
  doc["success"] = ok;
  doc["log"] = logMsg;

  String resp;
  serializeJson(doc, resp);
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "application/json", resp);
}

void handleApiRead() {
  uint8_t id = server.hasArg("id") ? (uint8_t)server.arg("id").toInt() : 3;
  uint32_t baud = server.hasArg("baud") ? (uint32_t)server.arg("baud").toInt() : 9600;

  meter.setBaudRate(baud);
  MeterLiveReadings r;
  bool ok = meter.readAllTelemetry(id, r);

  StaticJsonDocument<512> doc;
  doc["valid"] = ok;
  doc["voltage"] = r.voltage;
  doc["current"] = r.current;
  doc["active_power"] = r.active_power;
  doc["frequency"] = r.frequency;
  doc["total_energy"] = r.total_energy;

  String resp;
  serializeJson(doc, resp);
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "application/json", resp);
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("\n\n==============================================================");
  Serial.println("  ⚡ LILYGO T-CAN485 — EASTRON SDM120 MODBUS PROGRAMMER");
  Serial.println("==============================================================");

  // Initialize RS-485 Hardware Transceiver & Power Pins
  Serial.println("[RS485] Initializing Transceiver Pins (TX:22, RX:21, DE/RE:17, 5V_EN:16, CHIP_EN:19)...");
  meter.begin(2400);
  Serial.println("[RS485] Transceiver powered and ready.");

  // Connect to WiFi or create AP
  WiFi.mode(WIFI_MODE_APSTA);
#if defined(WIFI_SSID) && defined(WIFI_PASSWORD)
  Serial.printf("[WiFi] Connecting to %s...\n", WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  uint32_t t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 6000) {
    delay(250);
    Serial.print(".");
  }
  Serial.println();
#endif

  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("[WiFi] Connected! IP: http://%s/\n", WiFi.localIP().toString().c_str());
  } else {
    WiFi.softAP("Eastron-Programmer-AP", "12345678");
    Serial.printf("[WiFi] AP Mode created! Connect to 'Eastron-Programmer-AP' (pass: 12345678) -> http://%s/\n", WiFi.softAPIP().toString().c_str());
  }

  // Setup Web Server
  server.on("/", HTTP_GET, handleRoot);
  server.on("/api/scan", HTTP_GET, handleApiScan);
  server.on("/api/program", HTTP_ANY, handleApiProgram);
  server.on("/api/read", HTTP_GET, handleApiRead);
  server.begin();
  Serial.println("[HTTP] Web Server started on port 80.");

  // Initial Auto-Scan on boot
  Serial.println("\n[Boot] Running initial bus scan to check for connected meters...");
  auto found = meter.scanBus();
  if (found.empty()) {
    Serial.println("[Boot] No meter detected yet. (Ensure RS-485 A/B wires are connected and meter is powered).");
  } else {
    for (const auto &m : found) {
      Serial.printf(">>> DETECTED EASTRON: Slave ID %u @ %u baud (Voltage: %.1f V) <<<\n", m.slave_id, m.baud_rate, m.voltage);
    }
  }

  printMenu();
}

void handleSerialCommand(char cmd) {
  if (cmd == '1') {
    Serial.println("\n\n[CLI] Option 1: Scanning Modbus bus (2400, 9600, 4800, 1200 baud)...");
    auto found = meter.scanBus();
    if (found.empty()) {
      Serial.println("❌ No meters found. Check wiring (A to Terminal 9, B to Terminal 10) and ensure meter is powered.");
    } else {
      Serial.println("✓ Found Meter(s):");
      for (const auto &m : found) {
        Serial.printf("  • Slave ID: %u | Baud Rate: %u bps | Voltage: %.1f V\n", m.slave_id, m.baud_rate, m.voltage);
      }
    }
    printMenu();
  }
  else if (cmd == '2') {
    Serial.println("\n\n[CLI] Option 2: Quick Program -> Slave ID 3 @ 9600 bps");
    Serial.println("REMINDER: Make sure the Eastron LCD displays '-SET-' (long press front button for 3s).");
    String log;
    meter.programMeter(0, 0, 3, 9600, log);
    Serial.println(log);
    printMenu();
  }
  else if (cmd == '3') {
    Serial.println("\n\n[CLI] Option 3: Reading Live Telemetry...");
    // Try ID 3 at 9600 first, then ID 1 at 2400
    MeterLiveReadings r;
    uint8_t id = 3;
    uint32_t baud = 9600;
    meter.setBaudRate(baud);
    if (!meter.readAllTelemetry(id, r)) {
      id = 1;
      baud = 2400;
      meter.setBaudRate(baud);
      meter.readAllTelemetry(id, r);
    }
    if (r.valid) {
      Serial.printf("✓ Readings from Slave ID %u (@ %u baud):\n", id, baud);
      Serial.printf("  • Voltage:      %.1f V\n", r.voltage);
      Serial.printf("  • Active Power: %.1f W\n", r.active_power);
      Serial.printf("  • Current:      %.2f A\n", r.current);
      Serial.printf("  • Frequency:    %.1f Hz\n", r.frequency);
      Serial.printf("  • Total Energy: %.2f kWh\n", r.total_energy);
    } else {
      Serial.println("❌ Failed to read telemetry. (Try Option [1] to find meter's current ID and baud rate).");
    }
    printMenu();
  }
  else if (cmd == '4') {
    Serial.println("\n\n[CLI] Option 4: Resetting Meter to Factory Defaults -> Slave ID 1 @ 2400 bps");
    Serial.println("REMINDER: Make sure the Eastron LCD displays '-SET-' (long press front button for 3s).");
    String log;
    meter.programMeter(0, 0, 1, 2400, log);
    Serial.println(log);
    printMenu();
  }
  else if (cmd == '5') {
    Serial.println("\n\n[CLI] Option 5: Custom Programming");
    Serial.print("Enter Target Slave ID [1-247] (default 3): ");
    while (!Serial.available()) delay(10);
    int newId = Serial.parseInt();
    if (newId <= 0 || newId > 247) newId = 3;
    Serial.println(newId);

    Serial.print("Enter Target Baud Rate [2400, 4800, 9600, 19200] (default 9600): ");
    while (!Serial.available()) delay(10);
    int newBaud = Serial.parseInt();
    if (newBaud != 2400 && newBaud != 4800 && newBaud != 9600 && newBaud != 19200) newBaud = 9600;
    Serial.println(newBaud);

    String log;
    meter.programMeter(0, 0, (uint8_t)newId, (uint32_t)newBaud, log);
    Serial.println(log);
    printMenu();
  }
  else if (cmd == '6') {
    Serial.println("\n\n[CLI] Option 6: 30-Second Refresh Rate & Throughput Benchmark");
    runBenchmark(30);
    printMenu();
  }
}

void runBenchmark(uint32_t durationSec) {
  Serial.println("\n==============================================================");
  Serial.printf("  ⚡ RUNNING %u-SECOND EASTRON REFRESH RATE & THROUGHPUT TEST\n", durationSec);
  Serial.println("==============================================================");
  Serial.println("Querying Active Power (0x000C) and Voltage (0x0000) as fast as possible...");
  Serial.println("Measuring: RTT Latency, Modbus Transaction Rate, and Value Refresh Interval.\n");

  uint8_t slaveId = 3;
  uint32_t baud = 9600;
  meter.setBaudRate(baud);

  // Probe to verify connection
  float testV = 0.0f;
  if (!meter.readInputFloat(slaveId, 0x0000, testV, 60)) {
    // Try slave ID 1 if ID 3 fails
    if (meter.readInputFloat(1, 0x0000, testV, 60)) {
      slaveId = 1;
      Serial.printf("Note: Meter responded on Slave ID 1 (%.1f V). Testing ID 1.\n", testV);
    } else {
      Serial.println("❌ Error: Eastron meter not responding at 9600 baud. Check wiring/power.");
      return;
    }
  } else {
    Serial.printf("✓ Meter verified on Slave ID %u @ %u bps (Voltage: %.1f V).\n", slaveId, baud, testV);
  }

  uint32_t totalQueries = 0;
  uint32_t successfulQueries = 0;
  uint32_t timeouts = 0;

  uint32_t minRtt = 999999;
  uint32_t maxRtt = 0;
  uint64_t sumRtt = 0;

  float lastWatts = NAN;
  float lastVolts = NAN;
  uint32_t wattsChanges = 0;
  uint32_t voltsChanges = 0;
  uint32_t lastWattsChangeTime = 0;
  uint32_t lastVoltsChangeTime = 0;
  std::vector<uint32_t> wattsDeltaTimes;
  std::vector<uint32_t> voltsDeltaTimes;

  uint32_t startMs = millis();
  uint32_t endMs = startMs + (durationSec * 1000);
  uint32_t nextProgressPrint = startMs + 3000;

  while (millis() < endMs) {
    totalQueries++;
    uint32_t qStart = millis();

    float watts = NAN;
    bool ok = meter.readInputFloat(slaveId, 0x000C, watts, 50);
    uint32_t qEnd = millis();
    uint32_t rtt = qEnd - qStart;

    if (ok) {
      successfulQueries++;
      sumRtt += rtt;
      if (rtt < minRtt) minRtt = rtt;
      if (rtt > maxRtt) maxRtt = rtt;

      if (!std::isnan(lastWatts) && fabsf(watts - lastWatts) > 0.05f) {
        wattsChanges++;
        if (lastWattsChangeTime > 0) {
          wattsDeltaTimes.push_back(qStart - lastWattsChangeTime);
        }
        lastWattsChangeTime = qStart;
      }
      lastWatts = watts;
    } else {
      timeouts++;
    }

    // Small inter-frame gap required by Modbus RTU spec (3.5 chars = ~3.6ms at 9600)
    delay(4);

    if (millis() >= nextProgressPrint) {
      uint32_t elapsed = (millis() - startMs) / 1000;
      float curRate = (float)successfulQueries / (elapsed > 0 ? elapsed : 1);
      Serial.printf("  [%02us / %02us] Completed: %4u queries | Rate: %4.1f Hz | Latest Power: %5.1f W | Updates: %u\n",
                    elapsed, durationSec, successfulQueries, curRate, lastWatts, wattsChanges);
      nextProgressPrint = millis() + 3000;
    }
  }

  uint32_t actualDurationMs = millis() - startMs;
  float totalSec = (float)actualDurationMs / 1000.0f;
  float queryRateHz = (float)successfulQueries / totalSec;
  float avgRtt = successfulQueries > 0 ? ((float)sumRtt / successfulQueries) : 0;

  Serial.println("\n==============================================================");
  Serial.println("  📊 30-SECOND BENCHMARK RESULTS SUMMARY");
  Serial.println("==============================================================");
  Serial.printf("• Test Duration:             %.2f seconds\n", totalSec);
  Serial.printf("• Total Modbus Queries Sent: %u\n", totalQueries);
  Serial.printf("• Successful Responses:      %u (%.1f%% success rate)\n", successfulQueries, (float)successfulQueries * 100.0f / (totalQueries > 0 ? totalQueries : 1));
  Serial.printf("• Timeouts / Errors:         %u\n", timeouts);
  Serial.printf("• Real Query Throughput:     %.2f queries/sec (%.2f Hz)\n", queryRateHz, queryRateHz);
  Serial.printf("• Round-Trip Latency (RTT):  Min: %u ms | Avg: %.1f ms | Max: %u ms\n", minRtt == 999999 ? 0 : minRtt, avgRtt, maxRtt);
  Serial.printf("• Latest Readings:           Power: %.1f W\n", lastWatts);

  if (!wattsDeltaTimes.empty()) {
    uint32_t sumD = 0;
    for (uint32_t d : wattsDeltaTimes) sumD += d;
    float avgDeltaMs = (float)sumD / wattsDeltaTimes.size();
    float internalRefreshHz = 1000.0f / avgDeltaMs;
    Serial.printf("• Value Changes Detected:    %u times\n", wattsChanges);
    Serial.printf("• Internal Meter Update Interval: ~%.0f ms (Internal Refresh Rate: ~%.1f Hz)\n", avgDeltaMs, internalRefreshHz);
  } else {
    Serial.printf("• Value Changes Detected:    %u times (load power was static/stable during test)\n", wattsChanges);
  }
  Serial.println("==============================================================\n");
}

void loop() {
  server.handleClient();

  if (Serial.available()) {
    char c = (char)Serial.read();
    if (c >= '1' && c <= '6') {
      handleSerialCommand(c);
    }
  }

  delay(2);
}
