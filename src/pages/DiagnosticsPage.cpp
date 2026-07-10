#include "pages/DiagnosticsPage.h"

#include "display/Renderer.h"

DiagnosticsPage::DiagnosticsPage(RuntimeStats& stats) : stats_(stats) {}

void DiagnosticsPage::update(uint32_t nowMs) {
  nowMs_ = nowMs;
}

void DiagnosticsPage::draw(Renderer& renderer) {
  renderer.clear(0);
  renderer.drawText(0, 0, String("FPS ") + String(stats_.appFps), 15);
  renderer.drawText(0, 7, String("LAT ") + String(stats_.renderLatencyUs / 1000UL), 10);
  const uint8_t latencyBar = static_cast<uint8_t>(min<uint32_t>(15UL, stats_.frameLatencyUs / 1000UL));
  renderer.fillRect(79, 15 - latencyBar, 1, latencyBar, 15);
  const uint8_t jitterBar = static_cast<uint8_t>(min<uint32_t>(15UL, stats_.frameTimeJitterUs / 1000UL));
  renderer.fillRect(78, 15 - jitterBar, 1, jitterBar, 8);
  (void)nowMs_;
}
