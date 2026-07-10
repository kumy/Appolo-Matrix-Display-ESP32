#pragma once

#include <Arduino.h>

#include "core/Page.h"

class TextPage : public Page {
public:
  explicit TextPage(String message);

  void update(uint32_t nowMs) override;
  void draw(Renderer& renderer) override;

private:
  String message_;
  uint32_t nowMs_ = 0;
};
