#include "pages/DiagnosticsPage.h"

#include "display/Renderer.h"

DiagnosticsPage::DiagnosticsPage(RuntimeStats& stats) : stats_(stats) {}

void DiagnosticsPage::update(uint32_t nowMs) {
  nowMs_ = nowMs;
}

void DiagnosticsPage::setView(View view) {
  view_ = view;
}

void DiagnosticsPage::draw(Renderer& renderer) {
  renderer.clear(0);
  switch (view_) {
    case View::Overview:
      drawOverview(renderer);
      break;
    case View::Render:
      drawRender(renderer);
      break;
    case View::Scan:
      drawScan(renderer);
      break;
  }
  (void)nowMs_;
}

void DiagnosticsPage::drawOverview(Renderer& renderer) const {
  renderer.drawText(0, 0, String("FPS ") + String(stats_.appFps), 15);
  renderer.drawText(0, 7, String("LAT ") + String(stats_.renderLatencyUs / 1000UL), 10);
  const uint8_t latencyBar = static_cast<uint8_t>(min<uint32_t>(15UL, stats_.frameLatencyUs / 1000UL));
  renderer.fillRect(79, 15 - latencyBar, 1, latencyBar, 15);
  const uint8_t jitterBar = static_cast<uint8_t>(min<uint32_t>(15UL, stats_.frameTimeJitterUs / 1000UL));
  renderer.fillRect(78, 15 - jitterBar, 1, jitterBar, 8);
}

void DiagnosticsPage::drawRender(Renderer& renderer) const {
  renderer.drawText(0, 0, String("FPS ") + String(stats_.appFps), 15);
  renderer.drawText(0, 7, String("JIT ") + String(stats_.frameTimeJitterUs), 12);
  renderer.drawText(40, 0, String("REN ") + String(stats_.renderLatencyUs), 10);
  renderer.drawText(40, 7, String("PUB ") + String(stats_.publishLatencyUs), 8);
}

void DiagnosticsPage::drawScan(Renderer& renderer) const {
  renderer.drawText(0, 0, String("HZ ") + String(stats_.refreshHz), 15);
  renderer.drawText(0, 7, String("UND ") + String(stats_.scanUnderruns), 10);
  renderer.drawText(40, 0, String("DRP ") + String(stats_.droppedPresents), 8);
  renderer.drawText(40, 7, String("EVT ") + String(stats_.droppedEvents), 6);
}
