#include "network/WifiManager.h"

#include <WiFi.h>

#include <cstring>

#include "util/Logger.h"

namespace {
constexpr const char* kApPassword = "matrix2026";
constexpr uint32_t kReconnectIntervalMs = 10000;
constexpr uint32_t kStaConnectTimeoutMs = 30000;

String deviceSuffix() {
  // WiFi.softAPmacAddress()/macAddress() both read back all-zeros here:
  // the per-interface MAC apparently isn't assigned until esp_wifi_start()
  // actually runs (inside WiFi.softAP()), not just after WiFi.mode(). The
  // efuse MAC has no such ordering dependency — it's a direct hardware
  // fuse read, valid from boot regardless of WiFi driver state.
  char buffer[7];
  const uint64_t mac = ESP.getEfuseMac();
  snprintf(buffer, sizeof(buffer), "%06X", static_cast<unsigned int>(mac & 0xFFFFFFULL));
  return String(buffer);
}
}

void WifiManager::begin(const String& hostname, const String& ssid, const String& password) {
  hostname_ = hostname;
  if (ssid.isEmpty()) {
    startAccessPoint();
  } else {
    startStation(ssid, password);
  }
}

void WifiManager::startStation(const String& ssid, const String& password) {
  apMode_ = false;
  ssid_ = ssid;
  WiFi.mode(WIFI_STA);
  // Modem sleep (default ON) makes the radio nap between beacon intervals
  // to save power, adding real per-packet latency — a well-known cause of
  // "AsyncWebServer responds but crawls" on ESP32. This device is mains
  // powered, so there's no reason to trade throughput for power savings.
  WiFi.setSleep(false);
  WiFi.setHostname(hostname_.c_str());
  // No WiFi.config() call: leaving DHCP as the (default) address source.
  WiFi.begin(ssid.c_str(), password.c_str());
  state_ = "connecting";
  lastReconnectAttemptMs_ = millis();
  staConnectStartedMs_ = lastReconnectAttemptMs_;
}

void WifiManager::startAccessPoint() {
  apMode_ = true;
  ssid_ = "";
  WiFi.mode(WIFI_AP);
  WiFi.setSleep(false);
  apSsid_ = String("MatrixDisplay-") + deviceSuffix();
  const bool ok = WiFi.softAP(apSsid_.c_str(), kApPassword);
  state_ = "ap";
  Logger::info(String("WiFi AP ") + (ok ? "started: " : "FAILED to start: ") + apSsid_ + " ip=" + WiFi.softAPIP().toString());
}

void WifiManager::poll() {
  if (apMode_) {
    return;
  }
  if (WiFi.status() == WL_CONNECTED) {
    if (strcmp(state_, "connected") != 0) {
      Logger::info(String("WiFi connected, ip=") + WiFi.localIP().toString());
    }
    state_ = "connected";
    return;
  }
  state_ = "connecting";
  const uint32_t nowMs = millis();
  if ((nowMs - staConnectStartedMs_) >= kStaConnectTimeoutMs) {
    Logger::info(String("WiFi: could not join '") + ssid_ + "' within " + String(kStaConnectTimeoutMs / 1000UL) + "s, falling back to AP for reconfiguration");
    startAccessPoint();
    return;
  }
  if ((nowMs - lastReconnectAttemptMs_) >= kReconnectIntervalMs) {
    lastReconnectAttemptMs_ = nowMs;
    WiFi.reconnect();
  }
}

void WifiManager::applyCredentials(const String& ssid, const String& password) {
  // Only meaningful (and only avoids a harmless-but-noisy logged ESP_FAIL)
  // if we were actually in STA mode already — disconnecting from AP mode,
  // where there's nothing to disconnect from, always fails.
  if (!apMode_) {
    WiFi.disconnect(true, true);
  }
  if (ssid.isEmpty()) {
    startAccessPoint();
  } else {
    startStation(ssid, password);
  }
}

const char* WifiManager::state() const {
  return state_;
}

bool WifiManager::isConnected() const {
  return !apMode_ && WiFi.status() == WL_CONNECTED;
}

bool WifiManager::isApMode() const {
  return apMode_;
}

IPAddress WifiManager::localIp() const {
  return apMode_ ? WiFi.softAPIP() : WiFi.localIP();
}

String WifiManager::apSsid() const {
  return apSsid_;
}

String WifiManager::staSsid() const {
  return ssid_;
}
