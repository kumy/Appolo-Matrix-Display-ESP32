#include "network/HttpServer.h"

#include <ArduinoJson.h>
#include <LittleFS.h>

#include "core/Application.h"
#include "display/DisplayDriver.h"
#include "network/WifiManager.h"
#include "pages/DemoPage.h"
#include "storage/Settings.h"
#include "time/ClockService.h"
#include "util/Logger.h"

// HTML/JS now live on LittleFS (data/index.html, data/wifi.html — see
// setupStatusPageRoute()/setupWifiRoute()) rather than compiled into the
// firmware as PROGMEM strings. Upload them separately via
// `pio run --target uploadfs` (writes the LittleFS partition image; does
// NOT touch the app firmware) whenever data/ changes.

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
    if (!LittleFS.exists("/index.html")) {
      request->send(500, "text/plain", "index.html missing from LittleFS — run 'pio run --target uploadfs'");
      return;
    }
    request->send(LittleFS, "/index.html", "text/html");
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
    doc["fontName"] = settings_.values().fontName;
    doc["activePageId"] = settings_.values().activePageId;
    doc["clockFaceMode"] = settings_.values().clockFaceMode;
    doc["clockDisplayMode"] = settings_.values().clockDisplayMode;
    doc["clockAlternateIntervalMs"] = settings_.values().clockAlternateIntervalMs;
    doc["clockBlinkColon"] = settings_.values().clockBlinkColon;
    doc["clockAlign"] = settings_.values().clockAlign;
    doc["clockValign"] = settings_.values().clockValign;
    doc["textMessage"] = settings_.values().textMessage;
    doc["textAlign"] = settings_.values().textAlign;
    doc["textValign"] = settings_.values().textValign;
    doc["textOffsetX"] = settings_.values().textOffsetX;
    doc["textOffsetY"] = settings_.values().textOffsetY;
    doc["textAnimMode"] = settings_.values().textAnimMode;
    doc["textDirection"] = settings_.values().textDirection;
    doc["textEffect"] = settings_.values().textEffect;
    doc["demoFixedScene"] = settings_.values().demoFixedScene;
    doc["diagView"] = settings_.values().diagView;
    doc["countdownYear"] = settings_.values().countdownYear;
    doc["countdownMonth"] = settings_.values().countdownMonth;
    doc["countdownDay"] = settings_.values().countdownDay;
    doc["countdownHour"] = settings_.values().countdownHour;
    doc["countdownMinute"] = settings_.values().countdownMinute;
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
      const uint8_t pageId = static_cast<uint8_t>(constrain(body["activePageId"].as<int>(), 0, 5));
      settings_.setActivePageId(pageId);
      app_.setActivePage(pageId);
    }
    if (body["clockFaceMode"].is<int>()) {
      const uint8_t mode = static_cast<uint8_t>(constrain(body["clockFaceMode"].as<int>(), 0, 2));
      settings_.setClockFaceMode(mode);
      app_.clockPage().setFaceMode(static_cast<ClockFaceMode>(mode));
    }
    if (body["clockDisplayMode"].is<int>()) {
      const uint8_t mode = static_cast<uint8_t>(constrain(body["clockDisplayMode"].as<int>(), 0, 2));
      settings_.setClockDisplayMode(mode);
      app_.clockPage().setDisplayMode(static_cast<ClockDisplayMode>(mode));
    }
    if (body["clockAlternateIntervalMs"].is<int>()) {
      const uint32_t intervalMs = static_cast<uint32_t>(constrain(body["clockAlternateIntervalMs"].as<int>(), 250, 60000));
      settings_.setClockAlternateIntervalMs(intervalMs);
      app_.clockPage().setAlternateIntervalMs(intervalMs);
    }
    if (body["clockBlinkColon"].is<bool>()) {
      const bool blink = body["clockBlinkColon"].as<bool>();
      settings_.setClockBlinkColon(blink);
      app_.clockPage().setBlinkColon(blink);
    }
    if (body["clockAlign"].is<int>()) {
      const uint8_t align = static_cast<uint8_t>(constrain(body["clockAlign"].as<int>(), 0, 2));
      settings_.setClockAlign(align);
      app_.clockPage().setAlign(static_cast<HorizontalAlign>(align));
    }
    if (body["clockValign"].is<int>()) {
      const uint8_t valign = static_cast<uint8_t>(constrain(body["clockValign"].as<int>(), 0, 2));
      settings_.setClockValign(valign);
      app_.clockPage().setVerticalAlign(static_cast<VerticalAlign>(valign));
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
    if (body["textValign"].is<int>()) {
      const uint8_t valign = static_cast<uint8_t>(constrain(body["textValign"].as<int>(), 0, 2));
      settings_.setTextValign(valign);
      app_.textPage().setVerticalAlign(static_cast<VerticalAlign>(valign));
    }
    if (body["textOffsetX"].is<int>()) {
      const int16_t offsetX = static_cast<int16_t>(constrain(body["textOffsetX"].as<int>(), -80, 80));
      settings_.setTextOffsetX(offsetX);
      app_.textPage().setOffsetX(offsetX);
    }
    if (body["textOffsetY"].is<int>()) {
      const int16_t offsetY = static_cast<int16_t>(constrain(body["textOffsetY"].as<int>(), -16, 16));
      settings_.setTextOffsetY(offsetY);
      app_.textPage().setOffsetY(offsetY);
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
    if (body["countdownYear"].is<int>() || body["countdownMonth"].is<int>() || body["countdownDay"].is<int>()
        || body["countdownHour"].is<int>() || body["countdownMinute"].is<int>()) {
      const uint16_t year = static_cast<uint16_t>(constrain(
          body["countdownYear"].is<int>() ? body["countdownYear"].as<int>() : settings_.values().countdownYear, 2024, 2100));
      const uint8_t month = static_cast<uint8_t>(constrain(
          body["countdownMonth"].is<int>() ? body["countdownMonth"].as<int>() : settings_.values().countdownMonth, 1, 12));
      const uint8_t day = static_cast<uint8_t>(constrain(
          body["countdownDay"].is<int>() ? body["countdownDay"].as<int>() : settings_.values().countdownDay, 1, 31));
      const uint8_t hour = static_cast<uint8_t>(constrain(
          body["countdownHour"].is<int>() ? body["countdownHour"].as<int>() : settings_.values().countdownHour, 0, 23));
      const uint8_t minute = static_cast<uint8_t>(constrain(
          body["countdownMinute"].is<int>() ? body["countdownMinute"].as<int>() : settings_.values().countdownMinute, 0, 59));
      settings_.setCountdownTarget(year, month, day, hour, minute);
      app_.countdownPage().setTarget(year, month, day, hour, minute);
    }
    if (body["fontName"].is<const char*>()) {
      const String name = body["fontName"].as<String>();
      settings_.setFontName(name);
      app_.setFont(name);
    }
    request->send(200, "application/json", "{\"ok\":true}");
  });
  settingsHandler->setMethod(HTTP_POST);
  server_.addHandler(settingsHandler);
}

void HttpServer::setupWifiRoute() {
  // Static form (data/wifi.html) — pulls current WiFi state via the
  // already-existing /api/status (wifiState/apMode/ssid) with client-side
  // fetch() instead of server-generated HTML, matching index.html's
  // pattern. Only the credential-saving POST stays a server-side handler.
  server_.on("/wifi", HTTP_GET, [](AsyncWebServerRequest* request) {
    if (!LittleFS.exists("/wifi.html")) {
      request->send(500, "text/plain", "wifi.html missing from LittleFS — run 'pio run --target uploadfs'");
      return;
    }
    request->send(LittleFS, "/wifi.html", "text/html");
  });

  server_.on("/wifi", HTTP_POST, [this](AsyncWebServerRequest* request) {
    const String ssid = request->hasParam("ssid", true) ? request->getParam("ssid", true)->value() : "";
    const String password = request->hasParam("password", true) ? request->getParam("password", true)->value() : "";
    settings_.setWifiCredentials(ssid, password);
    wifi_.applyCredentials(ssid, password);
    request->send(200, "application/json", "{\"ok\":true}");
  });
}
