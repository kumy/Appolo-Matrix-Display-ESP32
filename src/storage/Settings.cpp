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
  settings_.clockAnalogMode = prefs.getBool("clockAnalog", settings_.clockAnalogMode);
  settings_.textMessage = prefs.getString("textMsg", settings_.textMessage);
  settings_.textAlign = prefs.getUChar("textAlign", settings_.textAlign);
  settings_.textAnimMode = prefs.getUChar("textAnim", settings_.textAnimMode);
  settings_.textDirection = prefs.getUChar("textDir", settings_.textDirection);
  settings_.textEffect = prefs.getUChar("textEffect", settings_.textEffect);
  settings_.demoFixedScene = prefs.getUChar("demoScene", settings_.demoFixedScene);
  settings_.diagView = prefs.getUChar("diagView", settings_.diagView);
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
  prefs.putBool("clockAnalog", settings_.clockAnalogMode);
  prefs.putString("textMsg", settings_.textMessage);
  prefs.putUChar("textAlign", settings_.textAlign);
  prefs.putUChar("textAnim", settings_.textAnimMode);
  prefs.putUChar("textDir", settings_.textDirection);
  prefs.putUChar("textEffect", settings_.textEffect);
  prefs.putUChar("demoScene", settings_.demoFixedScene);
  prefs.putUChar("diagView", settings_.diagView);
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

void Settings::setClockAnalogMode(bool analog) {
  settings_.clockAnalogMode = analog;
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
