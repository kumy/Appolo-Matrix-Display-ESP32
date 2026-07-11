#pragma once

#include <Arduino.h>

class ClockService {
public:
  void begin();
  void updateFromMillis(uint32_t nowMs);
  String timeString() const;
  String dateString() const;
  bool synced() const;
  uint8_t hours() const;
  uint8_t minutes() const;
  uint8_t seconds() const;

private:
  uint32_t nowMs_ = 0;
  bool synced_ = false;
};
