#include "pages/DeathDatesPage.h"

#include "display/GrayLevels.h"
#include "display/Renderer.h"

namespace {
struct Entry {
  const char* name;
  const char* deathDate;
};

// Renderer's bitmap font only covers space/colon/dash/slash/dot/0-9/A-Z
// (see Renderer.cpp's glyphRowsFor()), so accented characters are
// transliterated here (Aout not Août, Decembre not Décembre, etc.) —
// passing UTF-8 accented bytes through would render as blank/garbage
// glyphs, not the accented letter.
constexpr uint8_t kEntryCount = 18;  // must match DeathDatesPage::kEntryCount (private, so not referenced directly here)
constexpr Entry kEntries[kEntryCount] = {
    {"Michael Jackson", "25 Juin 2009"},
    {"Elvis Presley", "16 Aout 1977"},
    {"Freddie Mercury", "24 Novembre 1991"},
    {"John Lennon", "8 Decembre 1980"},
    {"David Bowie", "10 Janvier 2016"},
    {"Prince", "21 Avril 2016"},
    {"Whitney Houston", "11 Fevrier 2012"},
    {"Amy Winehouse", "23 Juillet 2011"},
    {"Tina Turner", "24 Mai 2023"},
    {"Diego Maradona", "25 Novembre 2020"},
    {"Ayrton Senna", "1 Mai 1994"},
    {"Princess Diana", "31 Aout 1997"},
    {"Nelson Mandela", "5 Decembre 2013"},
    {"Steve Jobs", "5 Octobre 2011"},
    {"Robin Williams", "11 Aout 2014"},
    {"Muhammad Ali", "3 Juin 2016"},
    {"Albert Einstein", "18 Avril 1955"},
    {"Stephen Hawking", "14 Mars 2018"},
};

constexpr int16_t kCharPitch = 4;  // must match Renderer's fixed glyph pitch
constexpr int16_t kTextY = 5;      // vertically centers the 5px-tall glyphs in 16px

void drawCentered(Renderer& renderer, const char* text, int16_t y, uint8_t gray) {
  const String value(text);
  const int16_t width = static_cast<int16_t>(value.length() * kCharPitch);
  const int16_t x = static_cast<int16_t>((80 - width) / 2);
  renderer.drawText(x, y, value, gray);
}
}

void DeathDatesPage::enter() {
  shuffleOrder();
  orderIndex_ = 0;
  phase_ = DeathDatesPhase::SlideIn;
  phaseStartedAtMs_ = millis();
}

void DeathDatesPage::shuffleOrder() {
  for (uint8_t i = 0; i < kEntryCount; ++i) {
    order_[i] = i;
  }
  for (uint8_t i = kEntryCount - 1; i > 0; --i) {
    const uint8_t j = static_cast<uint8_t>(random(0, i + 1));
    const uint8_t tmp = order_[i];
    order_[i] = order_[j];
    order_[j] = tmp;
  }
}

uint32_t DeathDatesPage::phaseDurationMs() const {
  switch (phase_) {
    case DeathDatesPhase::SlideIn: return 700UL;
    case DeathDatesPhase::Hold: return 1600UL;
    case DeathDatesPhase::Blank: return 300UL;
    case DeathDatesPhase::ShowDate: return 1800UL;
  }
  return 1000UL;
}

void DeathDatesPage::advancePhase(uint32_t nowMs) {
  phaseStartedAtMs_ = nowMs;
  switch (phase_) {
    case DeathDatesPhase::SlideIn:
      phase_ = DeathDatesPhase::Hold;
      break;
    case DeathDatesPhase::Hold:
      phase_ = DeathDatesPhase::Blank;
      break;
    case DeathDatesPhase::Blank:
      phase_ = DeathDatesPhase::ShowDate;
      break;
    case DeathDatesPhase::ShowDate:
      phase_ = DeathDatesPhase::SlideIn;
      ++orderIndex_;
      if (orderIndex_ >= kEntryCount) {
        orderIndex_ = 0;
        shuffleOrder();
      }
      break;
  }
}

void DeathDatesPage::update(uint32_t nowMs) {
  if ((nowMs - phaseStartedAtMs_) >= phaseDurationMs()) {
    advancePhase(nowMs);
  }
}

void DeathDatesPage::draw(Renderer& renderer) {
  renderer.clear(0);
  const Entry& entry = kEntries[order_[orderIndex_]];
  const uint32_t elapsed = millis() - phaseStartedAtMs_;

  switch (phase_) {
    case DeathDatesPhase::SlideIn: {
      constexpr int16_t kStartY = 16;
      constexpr uint32_t kSlideDurationMs = 700UL;
      const uint32_t clampedElapsed = min<uint32_t>(elapsed, kSlideDurationMs);
      const int16_t travel = static_cast<int16_t>((static_cast<int32_t>(kStartY - kTextY) * clampedElapsed) / kSlideDurationMs);
      const int16_t y = static_cast<int16_t>(kStartY - travel);
      // Fades in alongside the slide, not just a hard cut to full brightness.
      drawCentered(renderer, entry.name, y, GrayLevels::shade(clampedElapsed, kSlideDurationMs));
      break;
    }
    case DeathDatesPhase::Hold:
      drawCentered(renderer, entry.name, kTextY, GrayLevels::kFull);
      break;
    case DeathDatesPhase::Blank:
      break;
    case DeathDatesPhase::ShowDate:
      drawCentered(renderer, entry.deathDate, kTextY, GrayLevels::kFull);
      break;
  }
}
