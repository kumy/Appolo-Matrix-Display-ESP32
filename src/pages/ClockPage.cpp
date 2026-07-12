#include "pages/ClockPage.h"

#include <cmath>

#include "display/GrayLevels.h"
#include "display/Renderer.h"
#include "time/ClockService.h"

ClockPage::ClockPage(ClockService& clock) : clock_(clock) {}

void ClockPage::update(uint32_t nowMs) {
  nowMs_ = nowMs;
}

void ClockPage::draw(Renderer& renderer) {
  renderer.clear(0);
  if (analogMode_) {
    drawAnalog(renderer);
  } else {
    renderer.drawText(2, 1, clock_.timeString(), 15);
    renderer.drawText(2, 8, clock_.dateString(), 8);
  }
  (void)nowMs_;
}

void ClockPage::setAnalogMode(bool analog) {
  analogMode_ = analog;
}

void ClockPage::drawAnalog(Renderer& renderer) const {
  // Radius is capped by the panel's 16px height, not its 80px width — 7px
  // radius (15px face) is the largest circle that fits with a 1px margin
  // top and bottom.
  constexpr int16_t kCenterX = 40;
  constexpr int16_t kCenterY = 7;
  constexpr int16_t kRadius = 7;
  constexpr float kHourHandLen = 4.0f;
  constexpr float kMinuteHandLen = 6.5f;
  constexpr float kPi = 3.14159265f;

  renderer.drawCircle(kCenterX, kCenterY, kRadius, GrayLevels::shade(1, 4));

  const float hourFrac = (static_cast<float>(clock_.hours() % 12U) + static_cast<float>(clock_.minutes()) / 60.0f) / 12.0f;
  const float minuteFrac = (static_cast<float>(clock_.minutes()) + static_cast<float>(clock_.seconds()) / 60.0f) / 60.0f;
  const float hourAngle = hourFrac * 2.0f * kPi - (kPi / 2.0f);
  const float minuteAngle = minuteFrac * 2.0f * kPi - (kPi / 2.0f);

  const int16_t hourX = static_cast<int16_t>(kCenterX + kHourHandLen * cosf(hourAngle));
  const int16_t hourY = static_cast<int16_t>(kCenterY + kHourHandLen * sinf(hourAngle));
  const int16_t minuteX = static_cast<int16_t>(kCenterX + kMinuteHandLen * cosf(minuteAngle));
  const int16_t minuteY = static_cast<int16_t>(kCenterY + kMinuteHandLen * sinf(minuteAngle));

  renderer.drawLine(kCenterX, kCenterY, hourX, hourY, GrayLevels::kFull);
  renderer.drawLine(kCenterX, kCenterY, minuteX, minuteY, GrayLevels::shade(3, 4));
  renderer.drawPixel(kCenterX, kCenterY, GrayLevels::kFull);
}
