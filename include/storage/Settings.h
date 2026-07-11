#pragma once

#include <Arduino.h>

#include "display/DisplayConfig.h"

struct DeviceSettings {
  DisplayConfig display;
  uint8_t brightness = 255;
  bool powerOn = true;
  String hostname = "matrix-display-2026";
};

class Settings {
public:
  void begin();
  const DeviceSettings& values() const;

private:
  DeviceSettings settings_;
};
