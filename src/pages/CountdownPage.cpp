#include "pages/CountdownPage.h"

#include "display/GrayLevels.h"
#include "display/Renderer.h"
#include "time/ClockService.h"
#include "util/Logger.h"

CountdownPage::CountdownPage(ClockService& clock) : clock_(clock) {}

void CountdownPage::begin() {
  fontSmall_.loadFromFile("/fonts/classic3x5.font");
  fontMedium_.loadFromFile("/fonts/led7x5.font");
  fontLarge_.loadFromFile("/fonts/ledfull16.font");
}

void CountdownPage::update(uint32_t nowMs) {
  nowMs_ = nowMs;
}

void CountdownPage::setTarget(uint16_t year, uint8_t month, uint8_t day, uint8_t hour, uint8_t minute) {
  targetYear_ = year;
  targetMonth_ = month;
  targetDay_ = day;
  targetHour_ = hour;
  targetMinute_ = minute;
}

time_t CountdownPage::targetEpoch() const {
  struct tm targetTm = {};
  targetTm.tm_year = static_cast<int>(targetYear_) - 1900;
  targetTm.tm_mon = static_cast<int>(targetMonth_) - 1;
  targetTm.tm_mday = targetDay_;
  targetTm.tm_hour = targetHour_;
  targetTm.tm_min = targetMinute_;
  targetTm.tm_sec = 0;
  targetTm.tm_isdst = -1;
  return mktime(&targetTm);
}

String CountdownPage::formatRemaining(int64_t remainingSeconds, const Font** fontOut) const {
  char buffer[16];
  if (remainingSeconds >= 60LL * 60LL * 24LL * 60LL) {
    // 60+ days out: coarsest view, months + days.
    const int64_t days = remainingSeconds / 86400LL;
    const int64_t months = days / 30LL;
    snprintf(buffer, sizeof(buffer), "%lldMO %lldD", (long long)months, (long long)(days % 30LL));
    *fontOut = &fontSmall_;
  } else if (remainingSeconds >= 60LL * 60LL * 24LL * 2LL) {
    // 2-60 days out: days + hours.
    const int64_t days = remainingSeconds / 86400LL;
    const int64_t hours = (remainingSeconds % 86400LL) / 3600LL;
    snprintf(buffer, sizeof(buffer), "%lldD %lldH", (long long)days, (long long)hours);
    *fontOut = &fontSmall_;
  } else if (remainingSeconds >= 3600LL) {
    // 1 hour-2 days out: hours + minutes, bigger font since the string is
    // shorter now.
    const int64_t hours = remainingSeconds / 3600LL;
    const int64_t minutes = (remainingSeconds % 3600LL) / 60LL;
    snprintf(buffer, sizeof(buffer), "%02lldH%02lld", (long long)hours, (long long)minutes);
    *fontOut = &fontMedium_;
  } else if (remainingSeconds >= 60LL) {
    // 1-60 minutes out: MM:SS.
    const int64_t minutes = remainingSeconds / 60LL;
    const int64_t seconds = remainingSeconds % 60LL;
    snprintf(buffer, sizeof(buffer), "%02lld:%02lld", (long long)minutes, (long long)seconds);
    *fontOut = &fontMedium_;
  } else {
    // Final minute: bare seconds count, as large as the display allows.
    snprintf(buffer, sizeof(buffer), "%lld", (long long)remainingSeconds);
    *fontOut = &fontLarge_;
  }
  return String(buffer);
}

void CountdownPage::draw(Renderer& renderer) {
  renderer.clear(0);

  if (!clock_.synced()) {
    TextLayoutOptions options;
    options.maxWidth = 80;
    options.maxHeight = 16;
    options.align = HorizontalAlign::Center;
    options.valign = VerticalAlign::Middle;
    renderer.drawTextBox(0, 0, options, String("NO TIME SYNC"), GrayLevels::shade(1, 2));
    return;
  }

  const time_t now = clock_.epochSeconds();
  time_t target = targetEpoch();
  int64_t remaining = static_cast<int64_t>(target) - static_cast<int64_t>(now);

  // Recurring-target support (the stated "new year countdown" use case):
  // once a target is more than a day in the past, roll it forward a year
  // rather than sitting at a negative countdown forever until someone
  // reconfigures it by hand.
  while (remaining < -86400LL) {
    ++targetYear_;
    target = targetEpoch();
    remaining = static_cast<int64_t>(target) - static_cast<int64_t>(now);
  }

  if (remaining < 0) {
    remaining = 0;
  }

  const Font* font = &fontSmall_;
  const String text = formatRemaining(remaining, &font);

  const Font* previousFont = renderer.activeFont();
  renderer.setFont(font);

  TextLayoutOptions options;
  options.maxWidth = 80;
  options.maxHeight = 16;
  options.align = HorizontalAlign::Center;
  options.valign = VerticalAlign::Middle;
  renderer.drawTextBox(0, 0, options, text, GrayLevels::kFull);

  renderer.setFont(previousFont);
}
