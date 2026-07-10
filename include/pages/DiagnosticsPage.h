#pragma once

#include <Arduino.h>

#include "core/Page.h"
#include "core/RuntimeStats.h"

class DiagnosticsPage : public Page {
public:
  explicit DiagnosticsPage(RuntimeStats& stats);

  void update(uint32_t nowMs) override;
  void draw(Renderer& renderer) override;

private:
  RuntimeStats& stats_;
  uint32_t nowMs_ = 0;
};
