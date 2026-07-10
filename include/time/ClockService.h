#pragma once

#include <Arduino.h>

class ClockService {
public:
  void begin();
  void updateFromMillis(uint32_t nowMs);
  String timeString() const;
  String dateString() const;
  bool synced() const;

private:
  uint32_t nowMs_ = 0;
  bool synced_ = false;
};
