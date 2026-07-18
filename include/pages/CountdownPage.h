#pragma once

#include <Arduino.h>
#include <time.h>

#include "core/Page.h"
#include "display/Font.h"

class ClockService;
class Renderer;

// Counts down to a configurable target date/time (default use case: New
// Year). The displayed granularity and font size both narrow automatically
// as the target approaches — see formatRemaining() for the tier table —
// rather than always showing a fixed "DD:HH:MM:SS" format that wastes
// space when the target is months away and loses readability when it's
// seconds away.
class CountdownPage : public Page {
public:
  explicit CountdownPage(ClockService& clock);

  // Loads this page's own size-tiered fonts from LittleFS. Must be called
  // once after LittleFS has mounted (see Application::begin()) — draw()
  // falls back to Renderer's compiled-in glyphs for any tier whose font
  // failed to load, same safety pattern as Font.h's class comment.
  void begin();

  void update(uint32_t nowMs) override;
  void draw(Renderer& renderer) override;

  void setTarget(uint16_t year, uint8_t month, uint8_t day, uint8_t hour, uint8_t minute);

private:
  time_t targetEpoch() const;
  String formatRemaining(int64_t remainingSeconds, const Font** fontOut) const;

  ClockService& clock_;
  uint32_t nowMs_ = 0;
  uint16_t targetYear_ = 2027;
  uint8_t targetMonth_ = 1;
  uint8_t targetDay_ = 1;
  uint8_t targetHour_ = 0;
  uint8_t targetMinute_ = 0;

  // Small: multi-word "days away" style tiers. Medium: HH:MM/DD:HH style.
  // Large: bare seconds count, as prominent as possible for the final
  // countdown.
  Font fontSmall_;
  Font fontMedium_;
  Font fontLarge_;
};
