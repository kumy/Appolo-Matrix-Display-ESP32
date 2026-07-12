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
