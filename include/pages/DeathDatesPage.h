#pragma once

#include <Arduino.h>

#include "core/Page.h"

enum class DeathDatesPhase {
  SlideIn,
  Hold,
  Blank,
  ShowDate,
};

// Slideshow of notable people and their death dates, in randomized order:
// name slides in from the bottom and settles centered, holds, a short
// blank beat, then the death date appears centered, holds, then advances.
// Re-shuffles once the shuffled order is exhausted rather than picking a
// fresh random index each time, so every entry is shown once per lap
// (avoids immediate repeats/long-tail unfairness that pure per-step
// randomness would produce).
class DeathDatesPage : public Page {
public:
  void enter() override;
  void update(uint32_t nowMs) override;
  void draw(Renderer& renderer) override;

private:
  void advancePhase(uint32_t nowMs);
  void shuffleOrder();
  uint32_t phaseDurationMs() const;

  static constexpr uint8_t kEntryCount = 18;

  DeathDatesPhase phase_ = DeathDatesPhase::SlideIn;
  uint32_t phaseStartedAtMs_ = 0;
  uint8_t order_[kEntryCount] = {0};
  uint8_t orderIndex_ = 0;
};
