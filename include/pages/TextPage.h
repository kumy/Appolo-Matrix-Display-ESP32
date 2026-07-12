#pragma once

#include <Arduino.h>

#include "core/Page.h"
#include "display/Renderer.h"

enum class TextAnimMode { Fixed, Marquee };
enum class TextScrollDirection { Left, Right };
// Extensible on purpose: only None and FadeIn are implemented today, but
// this is the single switch point (see draw()) future appearing/
// disappearing effects should plug into.
enum class TextEffect { None, FadeIn };

class TextPage : public Page {
public:
  explicit TextPage(String message);

  void enter() override;
  void update(uint32_t nowMs) override;
  void draw(Renderer& renderer) override;

  void setMessage(const String& message);
  void setAlign(HorizontalAlign align);
  void setAnimMode(TextAnimMode mode);
  void setDirection(TextScrollDirection direction);
  void setEffect(TextEffect effect);

private:
  String message_;
  uint32_t nowMs_ = 0;
  uint32_t enteredAtMs_ = 0;
  HorizontalAlign align_ = HorizontalAlign::Left;
  TextAnimMode animMode_ = TextAnimMode::Fixed;
  TextScrollDirection direction_ = TextScrollDirection::Left;
  TextEffect effect_ = TextEffect::None;
};
