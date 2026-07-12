#pragma once

#include <Arduino.h>
#include <ESPAsyncWebServer.h>

#include "core/RuntimeStats.h"

class Settings;
class WifiManager;
class DisplayDriver;
class ClockService;
class Application;

// Owns the AsyncWebServer instance for the whole device: a JSON REST API
// (ArduinoJson) at /api/*, a plain HTML WiFi-provisioning form at /wifi
// (there is no captive-portal DNS redirect — in AP mode, navigate to
// http://192.168.4.1/wifi), and a small self-hosted status/control page at
// /. That page pulls Bootstrap from a CDN (so it renders properly-styled,
// but only when the browser also has internet access — irrelevant in AP
// mode, fine once the device is joined to a real network with internet)
// and polls /api/status client-side rather than pushing live updates
// (previously via ESP-DASH's websocket dashboard, replaced 2026-07 because
// its ~90KB page was too large to load reliably given this device's other
// real-time constraints — see mem:architecture/networking).
class HttpServer {
public:
  HttpServer(Settings& settings, WifiManager& wifi, DisplayDriver& display, ClockService& clock, RuntimeStats& stats, Application& app);

  void begin();
  void poll();
  bool running() const;

private:
  void setupApiRoutes();
  void setupWifiRoute();
  void setupStatusPageRoute();

  Settings& settings_;
  WifiManager& wifi_;
  DisplayDriver& display_;
  ClockService& clock_;
  RuntimeStats& stats_;
  Application& app_;

  AsyncWebServer server_{80};

  bool running_ = false;
};
