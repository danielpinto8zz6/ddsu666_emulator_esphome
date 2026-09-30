#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Update.h>
#include <ESPmDNS.h>
#include "secrets.h"

WebServer server(80);

const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Lilygo Minimal OTA Recovery</title>
  <style>
    body { font-family: sans-serif; background: #0f172a; color: #f8fafc; display: flex; justify-content: center; align-items: center; height: 100vh; margin: 0; }
    .box { background: #1e293b; padding: 32px; border-radius: 12px; border: 1px solid #334155; text-align: center; max-width: 420px; width: 90%; box-shadow: 0 10px 25px rgba(0,0,0,0.5); }
    h2 { margin-top: 0; color: #38bdf8; }
    input[type=file] { margin: 16px 0; color: #94a3b8; font-size: 0.9rem; width: 100%; }
    input[type=password] { background: #0f172a; border: 1px solid #475569; color: #fff; padding: 8px 12px; border-radius: 6px; width: 80%; margin-bottom: 16px; font-size: 0.9rem; }
    button { background: #0284c7; color: #fff; border: none; padding: 10px 20px; border-radius: 6px; font-size: 1rem; cursor: pointer; font-weight: bold; width: 100%; transition: background 0.2s; }
    button:hover { background: #0369a1; }
    #status { margin-top: 16px; font-size: 0.85rem; color: #cbd5e1; }
  </style>
</head>
<body>
  <div class="box">
    <h2>&#9889; Lilygo OTA Recovery</h2>
    <p style="font-size: 0.85rem; color: #94a3b8;">Minimal safe mode for uploading large firmware without task or cache conflicts.<br>Also reachable at <b>http://lilygo-ota.local</b></p>
    <form method="POST" action="/update" enctype="multipart/form-data" id="upload-form">
      <input type="password" name="password" id="pass" placeholder="Password (admin)" value="admin"><br>
      <input type="file" name="file" id="file" accept=".bin" required><br>
      <button type="submit" id="btn">Flash Full Firmware</button>
    </form>
    <div id="status"></div>
  </div>
  <script>
    const form = document.getElementById('upload-form');
    form.onsubmit = function(e) {
      document.getElementById('status').innerText = 'Uploading firmware... Please wait...';
      document.getElementById('btn').disabled = true;
    };
  </script>
</body>
</html>
)rawliteral";

void setup() {
  Serial.begin(115200);
  Serial.println("\n[OTA Trampoline] Starting...");

  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.setSleep(false);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(200);
    Serial.print(".");
  }
  Serial.printf("\n[WiFi] Connected! IP: %s\n", WiFi.localIP().toString().c_str());

  // mDNS: reachable at http://lilygo-ota.local (#9)
  if (MDNS.begin("lilygo-ota")) {
    Serial.println("[mDNS] Reachable at http://lilygo-ota.local");
  }

  server.on("/", HTTP_GET, []() {
    server.send_P(200, "text/html", INDEX_HTML);
  });

  server.on("/health", HTTP_GET, []() {
    server.send(200, "application/json",
      "{\"status\":\"TRAMPOLINE_OTA_READY\",\"ip\":\"" + WiFi.localIP().toString() +
      "\",\"free_heap\":" + String(ESP.getFreeHeap()) + "}");
  });

  server.on("/update", HTTP_POST, []() {
    if (server.hasArg("password") && server.arg("password") != "admin") {
      server.send(401, "text/plain", "Unauthorized");
      return;
    }
    server.sendHeader("Connection", "close");
    if (Update.hasError()) {
      server.send(500, "text/plain", "FAIL");
    } else {
      server.send(200, "text/plain", "OK! Rebooting...");
      delay(500);
      ESP.restart();
    }
  }, []() {
    HTTPUpload& upload = server.upload();
    if (upload.status == UPLOAD_FILE_START) {
      Serial.printf("[OTA] Starting: %s\n", upload.filename.c_str());
      if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_WRITE) {
      if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_END) {
      if (Update.end(true)) {
        Serial.printf("[OTA] Success: %u bytes\n", upload.totalSize);
      } else {
        Update.printError(Serial);
      }
    }
  });

  server.begin();
  Serial.println("[Web Server] Ready!");
}

void loop() {
  server.handleClient();
  delay(1);
}
