#pragma once

#include <Arduino.h>

#include "core/Page.h"
#include "display/Renderer.h"

class ClockService;

// TimeOnly/DateOnly always show the same string; Alternating switches
// between them every alternateIntervalMs_ (see currentText()).
enum class ClockDisplayMode { TimeOnly, DateOnly, Alternating };
// AnalogWithText draws both simultaneously (analog face on the left, text
// in the remaining width) rather than only ever switching between the two.
enum class ClockFaceMode { Digital, Analog, AnalogWithText };

class ClockPage : public Page {
public:
  explicit ClockPage(ClockService& clock);

  void update(uint32_t nowMs) override;
  void draw(Renderer& renderer) override;

  void setFaceMode(ClockFaceMode mode);
  void setDisplayMode(ClockDisplayMode mode);
  void setAlternateIntervalMs(uint32_t intervalMs);
  void setBlinkColon(bool blink);
  void setAlign(HorizontalAlign align);
  void setVerticalAlign(VerticalAlign valign);

private:
  void drawAnalog(Renderer& renderer, int16_t centerX, int16_t centerY, int16_t radius) const;
  String formattedTime() const;
  String currentText() const;

  ClockService& clock_;
  uint32_t nowMs_ = 0;
  ClockFaceMode faceMode_ = ClockFaceMode::Digital;
  ClockDisplayMode displayMode_ = ClockDisplayMode::TimeOnly;
  uint32_t alternateIntervalMs_ = 3000UL;
  bool blinkColon_ = false;
  HorizontalAlign align_ = HorizontalAlign::Center;
  VerticalAlign valign_ = VerticalAlign::Middle;
};
