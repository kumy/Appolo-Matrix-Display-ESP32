#pragma once

#include <Arduino.h>

#include "core/Page.h"

class ClockService;

class ClockPage : public Page {
public:
  explicit ClockPage(ClockService& clock);

  void update(uint32_t nowMs) override;
  void draw(Renderer& renderer) override;

private:
  ClockService& clock_;
  uint32_t nowMs_ = 0;
};
