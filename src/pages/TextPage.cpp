#include "pages/TextPage.h"

#include "display/GrayLevels.h"

namespace {
constexpr int16_t kCharPitch = 4;
// Matches the other scroll-driven DemoPage scenes (~24Hz) — see their
// comments on why update frequency, not step size, is the smoothness
// lever for integer-pixel text positioning.
constexpr uint32_t kMsPerPixel = 41UL;
constexpr uint32_t kFadeInMs = 800UL;
}

TextPage::TextPage(String message) : message_(message) {}

void TextPage::enter() {
  enteredAtMs_ = millis();
}

void TextPage::update(uint32_t nowMs) {
  nowMs_ = nowMs;
}

void TextPage::setMessage(const String& message) {
  message_ = message;
}

void TextPage::setAlign(HorizontalAlign align) {
  align_ = align;
}

void TextPage::setVerticalAlign(VerticalAlign valign) {
  valign_ = valign;
}

void TextPage::setOffsetX(int16_t offsetX) {
  offsetX_ = offsetX;
}

void TextPage::setOffsetY(int16_t offsetY) {
  offsetY_ = offsetY;
}

void TextPage::setAnimMode(TextAnimMode mode) {
  animMode_ = mode;
  // Restart cleanly rather than resuming mid-scroll/mid-fade from
  // whatever elapsed time had already accumulated under the old mode.
  enteredAtMs_ = millis();
}

void TextPage::setDirection(TextScrollDirection direction) {
  direction_ = direction;
}

void TextPage::setEffect(TextEffect effect) {
  effect_ = effect;
  enteredAtMs_ = millis();
}

void TextPage::draw(Renderer& renderer) {
  renderer.clear(0);
  const uint32_t elapsed = nowMs_ - enteredAtMs_;

  // Single switch point for appearing/disappearing effects — only None
  // and FadeIn exist today, but new TextEffect values plug in here.
  uint8_t gray = GrayLevels::kFull;
  if (effect_ == TextEffect::FadeIn) {
    gray = elapsed >= kFadeInMs ? GrayLevels::kFull : GrayLevels::shade(elapsed, kFadeInMs);
  }

  if (animMode_ == TextAnimMode::Marquee) {
    const int16_t textWidth = static_cast<int16_t>(message_.length() * kCharPitch);
    const int16_t cycle = static_cast<int16_t>(textWidth + 80);
    const int16_t travel = static_cast<int16_t>((elapsed / kMsPerPixel) % static_cast<uint32_t>(cycle));
    const int16_t x = direction_ == TextScrollDirection::Left
        ? static_cast<int16_t>(80 - travel)
        : static_cast<int16_t>(travel - textWidth);
    renderer.drawText(static_cast<int16_t>(x + offsetX_), static_cast<int16_t>(5 + offsetY_), message_, gray);
    return;
  }

  TextLayoutOptions options;
  options.maxWidth = 80;
  options.maxHeight = 16;
  options.lineSpacing = 1;
  options.align = align_;
  options.valign = valign_;
  renderer.drawTextBox(offsetX_, offsetY_, options, message_, gray);
}
