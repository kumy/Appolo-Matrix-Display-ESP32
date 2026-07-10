#include "storage/Settings.h"

void Settings::begin() {}

const DeviceSettings& Settings::values() const {
  return settings_;
}
