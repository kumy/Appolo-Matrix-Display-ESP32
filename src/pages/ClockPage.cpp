#include "pages/ClockPage.h"

#include "display/Renderer.h"
#include "time/ClockService.h"

ClockPage::ClockPage(ClockService& clock) : clock_(clock) {}

void ClockPage::update(uint32_t nowMs) {
  nowMs_ = nowMs;
}

void ClockPage::draw(Renderer& renderer) {
  renderer.clear(0);
  renderer.drawText(2, 1, clock_.timeString(), 15);
  renderer.drawText(2, 8, clock_.dateString(), 8);
  (void)nowMs_;
}
