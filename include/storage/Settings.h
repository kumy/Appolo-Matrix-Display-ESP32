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
  // 1=Clock, 2=Text, 3=Diagnostics, 4=DeathDates, 5=Countdown).
  uint8_t activePageId = 0;

  // --- Per-page settings, only meaningful while that page is active ---

  // ClockFaceMode: 0=Digital, 1=Analog, 2=AnalogWithText.
  uint8_t clockFaceMode = 0;
  // ClockDisplayMode: 0=TimeOnly, 1=DateOnly, 2=Alternating.
  uint8_t clockDisplayMode = 0;
  uint32_t clockAlternateIntervalMs = 3000;
  bool clockBlinkColon = false;
  uint8_t clockAlign = 1;   // HorizontalAlign, default Center
  uint8_t clockValign = 1;  // VerticalAlign, default Middle

  String textMessage = "LINE1\nLINE2";
  uint8_t textAlign = 0;       // HorizontalAlign: 0=Left, 1=Center, 2=Right
  uint8_t textValign = 1;      // VerticalAlign: 0=Top, 1=Middle, 2=Bottom
  int16_t textOffsetX = 0;     // fine pixel nudge on top of align/valign
  int16_t textOffsetY = 0;
  uint8_t textAnimMode = 0;    // 0=Fixed, 1=Marquee
  uint8_t textDirection = 0;   // marquee scroll: 0=Left(<-), 1=Right(->)
  uint8_t textEffect = 0;      // 0=None, 1=FadeIn — see TextPage::TextEffect

  // 255 = auto-rotate through every scene (original behavior); otherwise
  // an index into DemoPage's own scene list, pinning it to one scene.
  uint8_t demoFixedScene = 255;

  // Which fixed diagnostic view to show — see DiagnosticsPage::View.
  uint8_t diagView = 0;

  // CountdownPage target date/time (local, per ntpServer/utcOffsetMinutes)
  // — see CountdownPage::setTarget(). Defaults to the next New Year at the
  // time these defaults were authored; CountdownPage auto-rolls the year
  // forward once the target is more than a day in the past, so this only
  // ever needs manual reconfiguration to change the occasion.
  uint16_t countdownYear = 2027;
  uint8_t countdownMonth = 1;
  uint8_t countdownDay = 1;
  uint8_t countdownHour = 0;
  uint8_t countdownMinute = 0;

  // Base name (no extension) of the active font under /fonts/ on LittleFS
  // — resolved as "/fonts/" + fontName + ".font". See Font.h/Application.
  String fontName = "classic3x5";
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
  void setClockFaceMode(uint8_t mode);
  void setClockDisplayMode(uint8_t mode);
  void setClockAlternateIntervalMs(uint32_t intervalMs);
  void setClockBlinkColon(bool blink);
  void setClockAlign(uint8_t align);
  void setClockValign(uint8_t valign);
  void setTextMessage(const String& message);
  void setTextAlign(uint8_t align);
  void setTextValign(uint8_t valign);
  void setTextOffsetX(int16_t offsetX);
  void setTextOffsetY(int16_t offsetY);
  void setTextAnimMode(uint8_t mode);
  void setTextDirection(uint8_t direction);
  void setTextEffect(uint8_t effect);
  void setDemoFixedScene(uint8_t sceneIndex);
  void setDiagView(uint8_t view);
  void setCountdownTarget(uint16_t year, uint8_t month, uint8_t day, uint8_t hour, uint8_t minute);
  void setFontName(const String& name);

private:
  void load();
  void save() const;

  DeviceSettings settings_;
};
