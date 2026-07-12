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

  // Which Page is shown on the physical display — see
  // Application::setActivePage()/pageById() for the id mapping (0=Demo,
  // 1=Clock, 2=Text, 3=Diagnostics, 4=DeathDates).
  uint8_t activePageId = 0;

  // --- Per-page settings, only meaningful while that page is active ---
  bool clockAnalogMode = false;

  String textMessage = "LINE1\nLINE2";
  uint8_t textAlign = 0;       // HorizontalAlign: 0=Left, 1=Center, 2=Right
  uint8_t textAnimMode = 0;    // 0=Fixed, 1=Marquee
  uint8_t textDirection = 0;   // marquee scroll: 0=Left(<-), 1=Right(->)
  uint8_t textEffect = 0;      // 0=None, 1=FadeIn — see TextPage::TextEffect

  // 255 = auto-rotate through every scene (original behavior); otherwise
  // an index into DemoPage's own scene list, pinning it to one scene.
  uint8_t demoFixedScene = 255;

  // Which fixed diagnostic view to show — see DiagnosticsPage::View.
  uint8_t diagView = 0;
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
  void setActivePageId(uint8_t pageId);
  void setClockAnalogMode(bool analog);
  void setTextMessage(const String& message);
  void setTextAlign(uint8_t align);
  void setTextAnimMode(uint8_t mode);
  void setTextDirection(uint8_t direction);
  void setTextEffect(uint8_t effect);
  void setDemoFixedScene(uint8_t sceneIndex);
  void setDiagView(uint8_t view);

private:
  void load();
  void save() const;

  DeviceSettings settings_;
};
