#pragma once

#include <Arduino.h>

#include "display/DisplayConfig.h"

struct DeviceSettings {
  DisplayConfig display;
  uint8_t brightness = 255;
  bool powerOn = true;
  String hostname = "matrix-display-2026";
  String wifiSsid = "";
  String wifiPassword = "";
  String ntpServer = "pool.ntp.org";
  int16_t utcOffsetMinutes = 0;
};

// Backed by NVS via the Preferences API (namespace "settings"). Every
// setter persists immediately — this device has no explicit "save" UI
// action, so writes need to survive an unexpected power loss right after
// they're made.
class Settings {
public:
  void begin();
  const DeviceSettings& values() const;

  void setBrightness(uint8_t brightness);
  void setPowerOn(bool powerOn);
  void setWifiCredentials(const String& ssid, const String& password);

private:
  void load();
  void save() const;

  DeviceSettings settings_;
};
