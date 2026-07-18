#include "storage/Settings.h"

#include <Preferences.h>

namespace {
constexpr const char* kNamespace = "settings";
}

void Settings::begin() {
  load();
  // Persist the (possibly still-default) values immediately: on a fresh
  // device individual keys don't exist yet, and Preferences logs an
  // ESP_LOGE-level "NOT_FOUND" for every missing key on every load() even
  // though the fallback default is used correctly — this makes that a
  // one-time thing rather than every boot.
  save();
}

void Settings::load() {
  Preferences prefs;
  // Read-write, not read-only: on a fresh device the "settings" namespace
  // doesn't exist yet, and opening a nonexistent namespace read-only fails
  // (logs a scary but harmless "nvs_open failed: NOT_FOUND"). Opening
  // read-write creates it, so the first boot is clean and every boot after
  // behaves the same way.
  prefs.begin(kNamespace, false);
  settings_.brightness = prefs.getUChar("brightness", settings_.brightness);
  settings_.powerOn = prefs.getBool("powerOn", settings_.powerOn);
  settings_.hostname = prefs.getString("hostname", settings_.hostname);
  settings_.wifiSsid = prefs.getString("wifiSsid", settings_.wifiSsid);
  settings_.wifiPassword = prefs.getString("wifiPass", settings_.wifiPassword);
  settings_.ntpServer = prefs.getString("ntpServer", settings_.ntpServer);
  settings_.utcOffsetMinutes = prefs.getShort("utcOffset", settings_.utcOffsetMinutes);
  settings_.paletteLevelCount = prefs.getUChar("paletteLvls", settings_.paletteLevelCount);
  settings_.animationSpeedPercent = prefs.getUShort("animSpeed", settings_.animationSpeedPercent);
  settings_.activePageId = prefs.getUChar("activePage", settings_.activePageId);
  settings_.clockFaceMode = prefs.getUChar("clockFace", settings_.clockFaceMode);
  settings_.clockDisplayMode = prefs.getUChar("clockDispMode", settings_.clockDisplayMode);
  settings_.clockAlternateIntervalMs = prefs.getUInt("clockAltMs", settings_.clockAlternateIntervalMs);
  settings_.clockBlinkColon = prefs.getBool("clockBlink", settings_.clockBlinkColon);
  settings_.clockAlign = prefs.getUChar("clockAlign", settings_.clockAlign);
  settings_.clockValign = prefs.getUChar("clockValign", settings_.clockValign);
  settings_.textMessage = prefs.getString("textMsg", settings_.textMessage);
  settings_.textAlign = prefs.getUChar("textAlign", settings_.textAlign);
  settings_.textValign = prefs.getUChar("textValign", settings_.textValign);
  settings_.textOffsetX = prefs.getShort("textOffX", settings_.textOffsetX);
  settings_.textOffsetY = prefs.getShort("textOffY", settings_.textOffsetY);
  settings_.textAnimMode = prefs.getUChar("textAnim", settings_.textAnimMode);
  settings_.textDirection = prefs.getUChar("textDir", settings_.textDirection);
  settings_.textEffect = prefs.getUChar("textEffect", settings_.textEffect);
  settings_.demoFixedScene = prefs.getUChar("demoScene", settings_.demoFixedScene);
  settings_.diagView = prefs.getUChar("diagView", settings_.diagView);
  settings_.countdownYear = prefs.getUShort("cdYear", settings_.countdownYear);
  settings_.countdownMonth = prefs.getUChar("cdMonth", settings_.countdownMonth);
  settings_.countdownDay = prefs.getUChar("cdDay", settings_.countdownDay);
  settings_.countdownHour = prefs.getUChar("cdHour", settings_.countdownHour);
  settings_.countdownMinute = prefs.getUChar("cdMinute", settings_.countdownMinute);
  settings_.fontName = prefs.getString("fontName", settings_.fontName);
  prefs.end();
}

void Settings::save() const {
  Preferences prefs;
  prefs.begin(kNamespace, false);
  prefs.putUChar("brightness", settings_.brightness);
  prefs.putBool("powerOn", settings_.powerOn);
  prefs.putString("hostname", settings_.hostname);
  prefs.putString("wifiSsid", settings_.wifiSsid);
  prefs.putString("wifiPass", settings_.wifiPassword);
  prefs.putString("ntpServer", settings_.ntpServer);
  prefs.putShort("utcOffset", settings_.utcOffsetMinutes);
  prefs.putUChar("paletteLvls", settings_.paletteLevelCount);
  prefs.putUShort("animSpeed", settings_.animationSpeedPercent);
  prefs.putUChar("activePage", settings_.activePageId);
  prefs.putUChar("clockFace", settings_.clockFaceMode);
  prefs.putUChar("clockDispMode", settings_.clockDisplayMode);
  prefs.putUInt("clockAltMs", settings_.clockAlternateIntervalMs);
  prefs.putBool("clockBlink", settings_.clockBlinkColon);
  prefs.putUChar("clockAlign", settings_.clockAlign);
  prefs.putUChar("clockValign", settings_.clockValign);
  prefs.putString("textMsg", settings_.textMessage);
  prefs.putUChar("textAlign", settings_.textAlign);
  prefs.putUChar("textValign", settings_.textValign);
  prefs.putShort("textOffX", settings_.textOffsetX);
  prefs.putShort("textOffY", settings_.textOffsetY);
  prefs.putUChar("textAnim", settings_.textAnimMode);
  prefs.putUChar("textDir", settings_.textDirection);
  prefs.putUChar("textEffect", settings_.textEffect);
  prefs.putUChar("demoScene", settings_.demoFixedScene);
  prefs.putUChar("diagView", settings_.diagView);
  prefs.putUShort("cdYear", settings_.countdownYear);
  prefs.putUChar("cdMonth", settings_.countdownMonth);
  prefs.putUChar("cdDay", settings_.countdownDay);
  prefs.putUChar("cdHour", settings_.countdownHour);
  prefs.putUChar("cdMinute", settings_.countdownMinute);
  prefs.putString("fontName", settings_.fontName);
  prefs.end();
}

const DeviceSettings& Settings::values() const {
  return settings_;
}

void Settings::setBrightness(uint8_t brightness) {
  settings_.brightness = brightness;
  save();
}

void Settings::setPowerOn(bool powerOn) {
  settings_.powerOn = powerOn;
  save();
}

void Settings::setWifiCredentials(const String& ssid, const String& password) {
  settings_.wifiSsid = ssid;
  settings_.wifiPassword = password;
  save();
}

void Settings::setPaletteLevelCount(uint8_t count) {
  settings_.paletteLevelCount = count;
  save();
}

void Settings::setAnimationSpeedPercent(uint16_t percent) {
  settings_.animationSpeedPercent = percent;
  save();
}

void Settings::setActivePageId(uint8_t pageId) {
  settings_.activePageId = pageId;
  save();
}

void Settings::setClockFaceMode(uint8_t mode) {
  settings_.clockFaceMode = mode;
  save();
}

void Settings::setClockDisplayMode(uint8_t mode) {
  settings_.clockDisplayMode = mode;
  save();
}

void Settings::setClockAlternateIntervalMs(uint32_t intervalMs) {
  settings_.clockAlternateIntervalMs = intervalMs;
  save();
}

void Settings::setClockBlinkColon(bool blink) {
  settings_.clockBlinkColon = blink;
  save();
}

void Settings::setClockAlign(uint8_t align) {
  settings_.clockAlign = align;
  save();
}

void Settings::setClockValign(uint8_t valign) {
  settings_.clockValign = valign;
  save();
}

void Settings::setTextMessage(const String& message) {
  settings_.textMessage = message;
  save();
}

void Settings::setTextAlign(uint8_t align) {
  settings_.textAlign = align;
  save();
}

void Settings::setTextValign(uint8_t valign) {
  settings_.textValign = valign;
  save();
}

void Settings::setTextOffsetX(int16_t offsetX) {
  settings_.textOffsetX = offsetX;
  save();
}

void Settings::setTextOffsetY(int16_t offsetY) {
  settings_.textOffsetY = offsetY;
  save();
}

void Settings::setTextAnimMode(uint8_t mode) {
  settings_.textAnimMode = mode;
  save();
}

void Settings::setTextDirection(uint8_t direction) {
  settings_.textDirection = direction;
  save();
}

void Settings::setTextEffect(uint8_t effect) {
  settings_.textEffect = effect;
  save();
}

void Settings::setDemoFixedScene(uint8_t sceneIndex) {
  settings_.demoFixedScene = sceneIndex;
  save();
}

void Settings::setDiagView(uint8_t view) {
  settings_.diagView = view;
  save();
}

void Settings::setCountdownTarget(uint16_t year, uint8_t month, uint8_t day, uint8_t hour, uint8_t minute) {
  settings_.countdownYear = year;
  settings_.countdownMonth = month;
  settings_.countdownDay = day;
  settings_.countdownHour = hour;
  settings_.countdownMinute = minute;
  save();
}

void Settings::setFontName(const String& name) {
  settings_.fontName = name;
  save();
}
