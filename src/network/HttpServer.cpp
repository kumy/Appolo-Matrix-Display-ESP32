#include "network/HttpServer.h"

#include <ArduinoJson.h>

#include "core/Application.h"
#include "display/DisplayDriver.h"
#include "network/WifiManager.h"
#include "pages/DemoPage.h"
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
      <input type="range" class="form-range" min="1" max="255" id="brightnessRange">

      <label for="paletteRange" class="form-label mt-2">Gray levels (<span id="paletteValue">-</span>)</label>
      <input type="range" class="form-range" min="2" max="32" id="paletteRange">

      <label for="speedRange" class="form-label mt-2">Animation speed (<span id="speedValue">-</span>%)</label>
      <input type="range" class="form-range" min="10" max="400" id="speedRange">
    </div>
  </div>

  <div class="card mb-3">
    <div class="card-body">
      <h6 class="card-subtitle mb-3 text-body-secondary">Page</h6>
      <select class="form-select mb-3" id="pageSelect">
        <option value="0">Demo</option>
        <option value="1">Clock</option>
        <option value="2">Text</option>
        <option value="3">Diagnostics</option>
        <option value="4">Death Dates</option>
      </select>

      <div id="pane-1" class="page-pane d-none">
        <div class="form-check form-switch">
          <input class="form-check-input" type="checkbox" role="switch" id="clockAnalogSwitch">
          <label class="form-check-label" for="clockAnalogSwitch">Analog clock</label>
        </div>
      </div>

      <div id="pane-2" class="page-pane d-none">
        <label for="textMessageInput" class="form-label">Message</label>
        <textarea class="form-control mb-2" id="textMessageInput" rows="2"></textarea>
        <label for="textAlignSelect" class="form-label">Position</label>
        <select class="form-select mb-2" id="textAlignSelect">
          <option value="0">Left</option>
          <option value="1">Center</option>
          <option value="2">Right</option>
        </select>
        <label for="textAnimSelect" class="form-label">Animation</label>
        <select class="form-select mb-2" id="textAnimSelect">
          <option value="0">Fixed</option>
          <option value="1">Marquee</option>
        </select>
        <label for="textDirSelect" class="form-label">Marquee direction</label>
        <select class="form-select mb-2" id="textDirSelect">
          <option value="0">Left (&larr;)</option>
          <option value="1">Right (&rarr;)</option>
        </select>
        <label for="textEffectSelect" class="form-label">Appear effect</label>
        <select class="form-select mb-2" id="textEffectSelect">
          <option value="0">None</option>
          <option value="1">Fade in</option>
        </select>
        <button type="button" class="btn btn-outline-secondary btn-sm" id="textMessageApply">Apply message</button>
      </div>

      <div id="pane-0" class="page-pane d-none">
        <label for="demoSceneSelect" class="form-label">Scene</label>
        <select class="form-select" id="demoSceneSelect">
          <option value="-1">Auto-rotate</option>
          <option value="0">Fill</option>
          <option value="1">Grayscale</option>
          <option value="2">Primitives</option>
          <option value="3">Text</option>
          <option value="4">MultilineText</option>
          <option value="5">Clock</option>
          <option value="6">Bitmap</option>
          <option value="7">Transition</option>
          <option value="8">Checkerboard</option>
          <option value="9">EdgeStress</option>
          <option value="10">Diagnostics</option>
          <option value="11">Marquee</option>
          <option value="12">BouncingBall</option>
          <option value="13">SparklingHearts</option>
          <option value="14">Snake</option>
          <option value="15">SpaceInvaders</option>
          <option value="16">Tetris</option>
          <option value="17">Pong</option>
          <option value="18">Mario</option>
          <option value="19">Fireworks</option>
        </select>
      </div>

      <div id="pane-3" class="page-pane d-none">
        <label for="diagViewSelect" class="form-label">View</label>
        <select class="form-select" id="diagViewSelect">
          <option value="0">Overview</option>
          <option value="1">Render timing</option>
          <option value="2">Scan timing</option>
        </select>
      </div>
    </div>
  </div>

  <div class="card mb-3">
    <div class="card-body">
      <h6 class="card-subtitle mb-3 text-body-secondary">Network</h6>
      <a href="/wifi" class="btn btn-outline-secondary btn-sm">Configure WiFi</a>
    </div>
  </div>

  <div class="card">
    <div class="card-body">
      <h6 class="card-subtitle mb-3 text-body-secondary">Diagnostics (raw /api/status)</h6>
      <table class="table table-sm table-borderless mb-0" style="font-size: 0.85rem;">
        <tbody id="diagnosticsTable"></tbody>
      </table>
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
      document.getElementById('brightnessRange').value = Math.max(1, data.brightness);
      document.getElementById('brightnessValue').textContent = data.brightness;
      document.getElementById('paletteRange').value = data.paletteLevelCount;
      document.getElementById('paletteValue').textContent = data.paletteLevelCount;
      document.getElementById('speedRange').value = data.animationSpeedPercent;
      document.getElementById('speedValue').textContent = data.animationSpeedPercent;

      document.getElementById('pageSelect').value = data.activePageId;
      showPane(data.activePageId);
      document.getElementById('clockAnalogSwitch').checked = data.clockAnalogMode;
      document.getElementById('textMessageInput').value = data.textMessage;
      document.getElementById('textAlignSelect').value = data.textAlign;
      document.getElementById('textAnimSelect').value = data.textAnimMode;
      document.getElementById('textDirSelect').value = data.textDirection;
      document.getElementById('textEffectSelect').value = data.textEffect;
      document.getElementById('demoSceneSelect').value = (data.demoFixedScene === 255) ? -1 : data.demoFixedScene;
      document.getElementById('diagViewSelect').value = data.diagView;
    }

    const rateFields = ['fps', 'refreshHz', 'driverRefreshHz'];
    const usFields = ['renderLatencyUs', 'publishLatencyUs', 'scanStepMaxUs', 'scanStepLastUs', 'waitStepLastUs', 'gpioStepLastUs', 'rearmStepLastUs'];
    const counterFields = ['scanUnderruns', 'driverScanUnderruns', 'droppedPresents', 'droppedEvents'];

    function classify(key, value) {
      if (rateFields.indexOf(key) !== -1) {
        if (value >= 50) return 'text-success';
        if (value >= 20) return 'text-warning';
        return 'text-danger';
      }
      if (usFields.indexOf(key) !== -1) {
        if (value < 1000) return 'text-success';
        if (value < 10000) return 'text-warning';
        return 'text-danger';
      }
      if (counterFields.indexOf(key) !== -1) {
        if (value < 100) return 'text-success';
        if (value < 10000) return 'text-warning';
        return 'text-danger';
      }
      if (key === 'wifiState') {
        return value === 'connected' ? 'text-success' : 'text-warning';
      }
      if (key === 'timeSynced') {
        return value ? 'text-success' : 'text-warning';
      }
      return '';
    }

    const rows = Object.keys(data).sort().map(function (key) {
      const cls = classify(key, data[key]);
      return '<tr><td class="text-body-secondary">' + key + '</td><td class="text-end ' + cls + '">' + data[key] + '</td></tr>';
    });
    document.getElementById('diagnosticsTable').innerHTML = rows.join('');
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

function showPane(pageId) {
  document.querySelectorAll('.page-pane').forEach(function (el) {
    el.classList.add('d-none');
  });
  const pane = document.getElementById('pane-' + pageId);
  if (pane) {
    pane.classList.remove('d-none');
  }
}

document.getElementById('pageSelect').addEventListener('change', function (e) {
  const pageId = parseInt(e.target.value, 10);
  showPane(pageId);
  pushSettings({activePageId: pageId});
});
document.getElementById('clockAnalogSwitch').addEventListener('change', function (e) {
  pushSettings({clockAnalogMode: e.target.checked});
});
document.getElementById('textMessageApply').addEventListener('click', function () {
  pushSettings({textMessage: document.getElementById('textMessageInput').value});
});
document.getElementById('textAlignSelect').addEventListener('change', function (e) {
  pushSettings({textAlign: parseInt(e.target.value, 10)});
});
document.getElementById('textAnimSelect').addEventListener('change', function (e) {
  pushSettings({textAnimMode: parseInt(e.target.value, 10)});
});
document.getElementById('textDirSelect').addEventListener('change', function (e) {
  pushSettings({textDirection: parseInt(e.target.value, 10)});
});
document.getElementById('textEffectSelect').addEventListener('change', function (e) {
  pushSettings({textEffect: parseInt(e.target.value, 10)});
});
document.getElementById('demoSceneSelect').addEventListener('change', function (e) {
  pushSettings({demoFixedScene: parseInt(e.target.value, 10)});
});
document.getElementById('diagViewSelect').addEventListener('change', function (e) {
  pushSettings({diagView: parseInt(e.target.value, 10)});
});

document.getElementById('powerSwitch').addEventListener('change', function (e) {
  pushSettings({powerOn: e.target.checked});
});
document.getElementById('brightnessRange').addEventListener('input', function (e) {
  document.getElementById('brightnessValue').textContent = e.target.value;
});
document.getElementById('brightnessRange').addEventListener('change', function (e) {
  pushSettings({brightness: parseInt(e.target.value, 10)});
});
document.getElementById('paletteRange').addEventListener('input', function (e) {
  document.getElementById('paletteValue').textContent = e.target.value;
});
document.getElementById('paletteRange').addEventListener('change', function (e) {
  pushSettings({paletteLevelCount: parseInt(e.target.value, 10)});
});
document.getElementById('speedRange').addEventListener('input', function (e) {
  document.getElementById('speedValue').textContent = e.target.value;
});
document.getElementById('speedRange').addEventListener('change', function (e) {
  pushSettings({animationSpeedPercent: parseInt(e.target.value, 10)});
});

refresh();
setInterval(refresh, 5000);
</script>
<script src="https://cdn.jsdelivr.net/npm/bootstrap@5.3.3/dist/js/bootstrap.bundle.min.js"></script>
</body>
</html>
)HTML";
}

HttpServer::HttpServer(Settings& settings, WifiManager& wifi, DisplayDriver& display, ClockService& clock, RuntimeStats& stats, Application& app)
    : settings_(settings), wifi_(wifi), display_(display), clock_(clock), stats_(stats), app_(app) {}

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
    doc["paletteLevelCount"] = settings_.values().paletteLevelCount;
    doc["animationSpeedPercent"] = settings_.values().animationSpeedPercent;
    doc["activePageId"] = settings_.values().activePageId;
    doc["clockAnalogMode"] = settings_.values().clockAnalogMode;
    doc["textMessage"] = settings_.values().textMessage;
    doc["textAlign"] = settings_.values().textAlign;
    doc["textAnimMode"] = settings_.values().textAnimMode;
    doc["textDirection"] = settings_.values().textDirection;
    doc["textEffect"] = settings_.values().textEffect;
    doc["demoFixedScene"] = settings_.values().demoFixedScene;
    doc["diagView"] = settings_.values().diagView;
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
    if (body["paletteLevelCount"].is<int>()) {
      const uint8_t count = static_cast<uint8_t>(constrain(body["paletteLevelCount"].as<int>(), 2, 32));
      settings_.setPaletteLevelCount(count);
      display_.setPaletteLevelCount(count);
    }
    if (body["animationSpeedPercent"].is<int>()) {
      const uint16_t percent = static_cast<uint16_t>(constrain(body["animationSpeedPercent"].as<int>(), 10, 400));
      settings_.setAnimationSpeedPercent(percent);
      app_.demoPage().setAnimationSpeedPercent(percent);
    }
    if (body["activePageId"].is<int>()) {
      const uint8_t pageId = static_cast<uint8_t>(constrain(body["activePageId"].as<int>(), 0, 4));
      settings_.setActivePageId(pageId);
      app_.setActivePage(pageId);
    }
    if (body["clockAnalogMode"].is<bool>()) {
      const bool analog = body["clockAnalogMode"].as<bool>();
      settings_.setClockAnalogMode(analog);
      app_.clockPage().setAnalogMode(analog);
    }
    if (body["textMessage"].is<const char*>()) {
      const String message = body["textMessage"].as<String>();
      settings_.setTextMessage(message);
      app_.textPage().setMessage(message);
    }
    if (body["textAlign"].is<int>()) {
      const uint8_t align = static_cast<uint8_t>(constrain(body["textAlign"].as<int>(), 0, 2));
      settings_.setTextAlign(align);
      app_.textPage().setAlign(static_cast<HorizontalAlign>(align));
    }
    if (body["textAnimMode"].is<int>()) {
      const uint8_t mode = static_cast<uint8_t>(constrain(body["textAnimMode"].as<int>(), 0, 1));
      settings_.setTextAnimMode(mode);
      app_.textPage().setAnimMode(static_cast<TextAnimMode>(mode));
    }
    if (body["textDirection"].is<int>()) {
      const uint8_t direction = static_cast<uint8_t>(constrain(body["textDirection"].as<int>(), 0, 1));
      settings_.setTextDirection(direction);
      app_.textPage().setDirection(static_cast<TextScrollDirection>(direction));
    }
    if (body["textEffect"].is<int>()) {
      const uint8_t effect = static_cast<uint8_t>(constrain(body["textEffect"].as<int>(), 0, 1));
      settings_.setTextEffect(effect);
      app_.textPage().setEffect(static_cast<TextEffect>(effect));
    }
    if (body["demoFixedScene"].is<int>()) {
      const int requested = body["demoFixedScene"].as<int>();
      const uint8_t sceneIndex = requested < 0 ? 255U : static_cast<uint8_t>(constrain(requested, 0, 19));
      settings_.setDemoFixedScene(sceneIndex);
      app_.demoPage().setFixedScene(sceneIndex == 255U ? -1 : static_cast<int8_t>(sceneIndex));
    }
    if (body["diagView"].is<int>()) {
      const uint8_t view = static_cast<uint8_t>(constrain(body["diagView"].as<int>(), 0, 2));
      settings_.setDiagView(view);
      app_.diagnosticsPage().setView(static_cast<DiagnosticsPage::View>(view));
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
