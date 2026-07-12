#include "network/HttpServer.h"

#include <ArduinoJson.h>

#include "display/DisplayDriver.h"
#include "network/WifiManager.h"
#include "storage/Settings.h"
#include "time/ClockService.h"
#include "util/Logger.h"

namespace {
// Bootstrap itself loads from a CDN (only the browser fetches it, not this
// device), so this page is a few KB, not ~90KB — small enough that the
// throughput ceiling that made the old ESP-DASH page unusable doesn't
// matter here. Status values are populated client-side by polling
// /api/status rather than a server-push websocket.
const char kStatusPageHtml[] PROGMEM = R"HTML(<!DOCTYPE html>
<html lang="en" data-bs-theme="dark">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>matrix-display-2026</title>
<link href="https://cdn.jsdelivr.net/npm/bootstrap@5.3.3/dist/css/bootstrap.min.css" rel="stylesheet">
</head>
<body class="p-3">
<div class="container" style="max-width: 480px;">
  <h3 class="mb-4">matrix-display-2026</h3>

  <div class="card mb-3">
    <div class="card-body">
      <h6 class="card-subtitle mb-3 text-body-secondary">Status</h6>
      <div class="d-flex justify-content-between"><span>WiFi</span><span id="wifiState">-</span></div>
      <div class="d-flex justify-content-between"><span>SSID</span><span id="ssid">-</span></div>
      <div class="d-flex justify-content-between"><span>IP</span><span id="ip">-</span></div>
      <div class="d-flex justify-content-between"><span>Clock</span><span id="clock">-</span></div>
      <div class="d-flex justify-content-between"><span>FPS</span><span id="fps">-</span></div>
      <div class="d-flex justify-content-between"><span>Uptime</span><span id="uptime">-</span></div>
    </div>
  </div>

  <div class="card mb-3">
    <div class="card-body">
      <h6 class="card-subtitle mb-3 text-body-secondary">Controls</h6>
      <div class="mb-3 form-check form-switch">
        <input class="form-check-input" type="checkbox" role="switch" id="powerSwitch">
        <label class="form-check-label" for="powerSwitch">Power</label>
      </div>
      <label for="brightnessRange" class="form-label">Brightness (<span id="brightnessValue">-</span>)</label>
      <input type="range" class="form-range" min="0" max="255" id="brightnessRange">
    </div>
  </div>

  <div class="card">
    <div class="card-body">
      <h6 class="card-subtitle mb-3 text-body-secondary">Network</h6>
      <a href="/wifi" class="btn btn-outline-secondary btn-sm">Configure WiFi</a>
    </div>
  </div>
</div>

<script>
let suppressUntil = 0;

function refresh() {
  fetch('/api/status').then(function (r) { return r.json(); }).then(function (data) {
    document.getElementById('wifiState').textContent = data.apMode ? 'AP mode' : data.wifiState;
    document.getElementById('ssid').textContent = data.ssid;
    document.getElementById('ip').textContent = data.ip;
    document.getElementById('clock').textContent = data.timeSynced ? (data.time + ' ' + data.date) : 'not synced';
    document.getElementById('fps').textContent = data.fps;
    document.getElementById('uptime').textContent = Math.round(data.uptimeMs / 1000) + 's';
    if (Date.now() > suppressUntil) {
      document.getElementById('powerSwitch').checked = data.powerOn;
      document.getElementById('brightnessRange').value = data.brightness;
      document.getElementById('brightnessValue').textContent = data.brightness;
    }
  }).catch(function () {});
}

function pushSettings(partial) {
  suppressUntil = Date.now() + 2000;
  fetch('/api/settings', {
    method: 'POST',
    headers: {'Content-Type': 'application/json'},
    body: JSON.stringify(partial)
  }).catch(function () {});
}

document.getElementById('powerSwitch').addEventListener('change', function (e) {
  pushSettings({powerOn: e.target.checked});
});
document.getElementById('brightnessRange').addEventListener('input', function (e) {
  document.getElementById('brightnessValue').textContent = e.target.value;
});
document.getElementById('brightnessRange').addEventListener('change', function (e) {
  pushSettings({brightness: parseInt(e.target.value, 10)});
});

refresh();
setInterval(refresh, 5000);
</script>
<script src="https://cdn.jsdelivr.net/npm/bootstrap@5.3.3/dist/js/bootstrap.bundle.min.js"></script>
</body>
</html>
)HTML";
}

HttpServer::HttpServer(Settings& settings, WifiManager& wifi, DisplayDriver& display, ClockService& clock, RuntimeStats& stats)
    : settings_(settings), wifi_(wifi), display_(display), clock_(clock), stats_(stats) {}

void HttpServer::begin() {
  setupApiRoutes();
  setupWifiRoute();
  setupStatusPageRoute();

  server_.begin();
  running_ = true;
  Logger::info(String("HTTP server started on port 80"));
}

void HttpServer::poll() {
  // Nothing to do here anymore: AsyncWebServer is fully async (no polling
  // needed) and the status page now pulls state via client-side fetch()
  // instead of a server-push dashboard that needed a periodic tick to
  // drive it. Kept as a no-op so Application::tick() doesn't need to
  // change what it calls.
}

bool HttpServer::running() const {
  return running_;
}

void HttpServer::setupStatusPageRoute() {
  server_.on("/", HTTP_GET, [](AsyncWebServerRequest* request) {
    request->send(200, "text/html", kStatusPageHtml);
  });
}

void HttpServer::setupApiRoutes() {
  server_.on("/api/status", HTTP_GET, [this](AsyncWebServerRequest* request) {
    JsonDocument doc;
    doc["wifiState"] = wifi_.state();
    doc["apMode"] = wifi_.isApMode();
    doc["ssid"] = wifi_.isApMode() ? wifi_.apSsid() : wifi_.staSsid();
    doc["ip"] = wifi_.localIp().toString();
    doc["brightness"] = settings_.values().brightness;
    doc["powerOn"] = settings_.values().powerOn;
    doc["fps"] = stats_.appFps;
    doc["refreshHz"] = stats_.refreshHz;
    doc["scanUnderruns"] = stats_.scanUnderruns;
    doc["droppedPresents"] = stats_.droppedPresents;
    doc["renderLatencyUs"] = stats_.renderLatencyUs;
    doc["publishLatencyUs"] = stats_.publishLatencyUs;
    // Read directly from DisplayDriver's own stats (populated by the scan
    // task itself), bypassing Application::tick()'s copy into stats_ above
    // — if tick() itself is starved, stats_.refreshHz freezes at stale/zero
    // regardless of whether the scan task is actually healthy. Comparing
    // the two distinguishes "scan task is broken" from "loop task is
    // starved and never gets to copy the scan task's real numbers".
    doc["driverRefreshHz"] = display_.stats().refreshHz;
    doc["driverScanUnderruns"] = display_.stats().scanUnderruns;
    doc["scanStepMaxUs"] = display_.stats().scanStepMaxUs;
    doc["scanStepLastUs"] = display_.stats().scanStepLastUs;
    doc["waitStepLastUs"] = display_.stats().waitStepLastUs;
    doc["gpioStepLastUs"] = display_.stats().gpioStepLastUs;
    doc["rearmStepLastUs"] = display_.stats().rearmStepLastUs;
    doc["uptimeMs"] = millis();
    doc["timeSynced"] = clock_.synced();
    doc["time"] = clock_.timeString();
    doc["date"] = clock_.dateString();

    String body;
    serializeJson(doc, body);
    request->send(200, "application/json", body);
  });

  auto* settingsHandler = new AsyncCallbackJsonWebHandler("/api/settings", [this](AsyncWebServerRequest* request, JsonVariant& json) {
    const JsonObject body = json.as<JsonObject>();
    if (body["brightness"].is<int>()) {
      const uint8_t brightness = static_cast<uint8_t>(constrain(body["brightness"].as<int>(), 0, 255));
      settings_.setBrightness(brightness);
      display_.setBrightness(brightness);
    }
    if (body["powerOn"].is<bool>()) {
      const bool powerOn = body["powerOn"].as<bool>();
      settings_.setPowerOn(powerOn);
      display_.setPower(powerOn);
    }
    request->send(200, "application/json", "{\"ok\":true}");
  });
  settingsHandler->setMethod(HTTP_POST);
  server_.addHandler(settingsHandler);
}

void HttpServer::setupWifiRoute() {
  server_.on("/wifi", HTTP_GET, [this](AsyncWebServerRequest* request) {
    String html = "<!DOCTYPE html><html><head><meta name='viewport' content='width=device-width,initial-scale=1'>"
                  "<title>WiFi setup</title></head><body>"
                  "<h3>matrix-display-2026 - WiFi setup</h3>";
    html += "<p>Status: ";
    html += wifi_.state();
    if (wifi_.isApMode()) {
      html += " (provisioning AP: ";
      html += wifi_.apSsid();
      html += ")";
    }
    html += "</p>";
    html += "<form method='POST' action='/wifi'>"
            "SSID:<br><input name='ssid' value='";
    html += settings_.values().wifiSsid;
    html += "'><br>Password:<br><input name='password' type='password'><br><br>"
            "<button type='submit'>Save &amp; Connect</button>"
            "</form></body></html>";
    request->send(200, "text/html", html);
  });

  server_.on("/wifi", HTTP_POST, [this](AsyncWebServerRequest* request) {
    const String ssid = request->hasParam("ssid", true) ? request->getParam("ssid", true)->value() : "";
    const String password = request->hasParam("password", true) ? request->getParam("password", true)->value() : "";
    settings_.setWifiCredentials(ssid, password);
    wifi_.applyCredentials(ssid, password);
    request->send(200, "text/html", "<!DOCTYPE html><html><body>Saved. Reconnecting&hellip; "
                                     "<a href='/wifi'>back</a></body></html>");
  });
}
