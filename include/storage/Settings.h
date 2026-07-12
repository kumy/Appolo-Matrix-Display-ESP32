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
  // Number of evenly-spaced steps (out of the 32 raw hardware levels)
  // actually used for gray-level quantization — see
  // DisplayDriver::setPaletteLevelCount(). 32 = full hardware resolution.
  uint8_t paletteLevelCount = 32;
  // Percent of normal speed for DemoPage's scene animations (100 =
  // unchanged, 200 = 2x, 50 = half) — see DemoPage::setAnimationSpeed().
  uint16_t animationSpeedPercent = 100;
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
  void setPaletteLevelCount(uint8_t count);
  void setAnimationSpeedPercent(uint16_t percent);

private:
  void load();
  void save() const;

  DeviceSettings settings_;
};
