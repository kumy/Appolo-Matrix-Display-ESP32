#pragma once

#include <Arduino.h>
#include <time.h>

// Falls back to a millis()-since-boot clock (nowMs_) until beginNtp() has
// been called AND the SNTP sync has actually landed (checked via a sane
// epoch lower bound, not just "was configTime() called") — public API
// (hours/minutes/seconds/timeString/dateString/synced) is unchanged either
// way, so callers never need to care which source is active.
class ClockService {
public:
  void begin();
  void beginNtp(const String& server, int16_t utcOffsetMinutes);
  void updateFromMillis(uint32_t nowMs);
  String timeString() const;
  String dateString() const;
  bool synced() const;
  uint8_t hours() const;
  uint8_t minutes() const;
  uint8_t seconds() const;
  // Only meaningful when synced() — 0 otherwise (no real epoch to report
  // pre-sync). Used by CountdownPage to diff against a target time_t.
  time_t epochSeconds() const;

private:
  uint32_t nowMs_ = 0;
  bool ntpConfigured_ = false;
};
