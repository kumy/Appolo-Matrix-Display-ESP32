#pragma once

#include <Arduino.h>
#include <IPAddress.h>

// STA connection with DHCP (default ESP32 behavior whenever no static IP is
// configured via WiFi.config() — deliberately never called here) using
// credentials from Settings. Falls back to a local access point for
// first-boot/no-credentials provisioning; HttpServer serves the config form
// that calls applyCredentials() once the user submits new ones, so a
// reboot isn't required to join the configured network. If STA connection
// doesn't succeed within kStaConnectTimeoutMs (wrong password, network out
// of range, etc.), automatically falls back to AP mode rather than being
// stuck retrying forever with no way to reconfigure — the bad credentials
// stay saved in Settings, but the user can reach /wifi again via the AP to
// fix them.
class WifiManager {
public:
  void begin(const String& hostname, const String& ssid, const String& password);
  void poll();
  void applyCredentials(const String& ssid, const String& password);

  const char* state() const;
  bool isConnected() const;
  bool isApMode() const;
  IPAddress localIp() const;
  String apSsid() const;
  String staSsid() const;

private:
  void startStation(const String& ssid, const String& password);
  void startAccessPoint();

  String hostname_;
  String ssid_;
  String apSsid_;
  const char* state_ = "disabled";
  bool apMode_ = false;
  uint32_t lastReconnectAttemptMs_ = 0;
  uint32_t staConnectStartedMs_ = 0;
};
