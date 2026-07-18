#include "pages/ClockPage.h"

#include <cmath>

#include "display/GrayLevels.h"
#include "time/ClockService.h"

ClockPage::ClockPage(ClockService& clock) : clock_(clock) {}

void ClockPage::update(uint32_t nowMs) {
  nowMs_ = nowMs;
}

void ClockPage::setFaceMode(ClockFaceMode mode) {
  faceMode_ = mode;
}

void ClockPage::setDisplayMode(ClockDisplayMode mode) {
  displayMode_ = mode;
}

void ClockPage::setAlternateIntervalMs(uint32_t intervalMs) {
  alternateIntervalMs_ = intervalMs < 250UL ? 250UL : intervalMs;
}

void ClockPage::setBlinkColon(bool blink) {
  blinkColon_ = blink;
}

void ClockPage::setAlign(HorizontalAlign align) {
  align_ = align;
}

void ClockPage::setVerticalAlign(VerticalAlign valign) {
  valign_ = valign;
}

String ClockPage::formattedTime() const {
  String text = clock_.timeString();
  // Blink at 1Hz (500ms on/off) by blanking the colon glyphs on the "off"
  // half of the cycle — cheaper and simpler than tracking a separate blink
  // state machine, and self-synchronizes across mode changes since it's a
  // pure function of nowMs_.
  if (blinkColon_ && ((nowMs_ / 500UL) % 2UL) == 1UL) {
    for (size_t i = 0; i < text.length(); ++i) {
      if (text[i] == ':') {
        text.setCharAt(i, ' ');
      }
    }
  }
  return text;
}

String ClockPage::currentText() const {
  switch (displayMode_) {
    case ClockDisplayMode::TimeOnly:
      return formattedTime();
    case ClockDisplayMode::DateOnly:
      return clock_.dateString();
    case ClockDisplayMode::Alternating: {
      const uint32_t phase = (nowMs_ / alternateIntervalMs_) % 2UL;
      return phase == 0UL ? formattedTime() : clock_.dateString();
    }
  }
  return formattedTime();
}

void ClockPage::draw(Renderer& renderer) {
  renderer.clear(0);

  if (faceMode_ == ClockFaceMode::Analog) {
    drawAnalog(renderer, 40, 7, 7);
    return;
  }

  if (faceMode_ == ClockFaceMode::AnalogWithText) {
    drawAnalog(renderer, 8, 7, 7);
    TextLayoutOptions options;
    options.maxWidth = 62;
    options.maxHeight = 16;
    options.align = align_;
    options.valign = valign_;
    renderer.drawTextBox(17, 0, options, currentText(), GrayLevels::kFull);
    return;
  }

  TextLayoutOptions options;
  options.maxWidth = 80;
  options.maxHeight = 16;
  options.align = align_;
  options.valign = valign_;
  renderer.drawTextBox(0, 0, options, currentText(), GrayLevels::kFull);
}

void ClockPage::drawAnalog(Renderer& renderer, int16_t centerX, int16_t centerY, int16_t radius) const {
  constexpr float kHourHandFrac = 0.55f;
  constexpr float kMinuteHandFrac = 0.9f;
  constexpr float kPi = 3.14159265f;

  renderer.drawCircle(centerX, centerY, radius, GrayLevels::shade(1, 4));

  const float hourHandLen = radius * kHourHandFrac;
  const float minuteHandLen = radius * kMinuteHandFrac;

  const float hourFrac = (static_cast<float>(clock_.hours() % 12U) + static_cast<float>(clock_.minutes()) / 60.0f) / 12.0f;
  const float minuteFrac = (static_cast<float>(clock_.minutes()) + static_cast<float>(clock_.seconds()) / 60.0f) / 60.0f;
  const float hourAngle = hourFrac * 2.0f * kPi - (kPi / 2.0f);
  const float minuteAngle = minuteFrac * 2.0f * kPi - (kPi / 2.0f);

  const int16_t hourX = static_cast<int16_t>(centerX + hourHandLen * cosf(hourAngle));
  const int16_t hourY = static_cast<int16_t>(centerY + hourHandLen * sinf(hourAngle));
  const int16_t minuteX = static_cast<int16_t>(centerX + minuteHandLen * cosf(minuteAngle));
  const int16_t minuteY = static_cast<int16_t>(centerY + minuteHandLen * sinf(minuteAngle));

  renderer.drawLine(centerX, centerY, hourX, hourY, GrayLevels::kFull);
  renderer.drawLine(centerX, centerY, minuteX, minuteY, GrayLevels::shade(3, 4));
  renderer.drawPixel(centerX, centerY, GrayLevels::kFull);
}
