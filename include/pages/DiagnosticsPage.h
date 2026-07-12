#pragma once

#include <Arduino.h>

#include "core/Page.h"
#include "core/RuntimeStats.h"

class DiagnosticsPage : public Page {
public:
  enum class View { Overview, Render, Scan };

  explicit DiagnosticsPage(RuntimeStats& stats);

  void update(uint32_t nowMs) override;
  void draw(Renderer& renderer) override;
  void setView(View view);

private:
  void drawOverview(Renderer& renderer) const;
  void drawRender(Renderer& renderer) const;
  void drawScan(Renderer& renderer) const;

  RuntimeStats& stats_;
  uint32_t nowMs_ = 0;
  View view_ = View::Overview;
};
