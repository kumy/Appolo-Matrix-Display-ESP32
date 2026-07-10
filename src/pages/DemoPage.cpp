#include "pages/DemoPage.h"

#include "display/Renderer.h"
#include "time/ClockService.h"

namespace {
const uint8_t kLogo[8 * 8] = {
    0, 1, 1, 0, 0, 1, 1, 0,
    1, 0, 0, 1, 1, 0, 0, 1,
    1, 0, 1, 0, 0, 1, 0, 1,
    0, 1, 0, 1, 1, 0, 1, 0,
    0, 1, 0, 1, 1, 0, 1, 0,
    1, 0, 1, 0, 0, 1, 0, 1,
    1, 0, 0, 1, 1, 0, 0, 1,
    0, 1, 1, 0, 0, 1, 1, 0,
};
}

DemoPage::DemoPage(ClockService& clock, RuntimeStats& stats) : clock_(clock), stats_(stats) {}

void DemoPage::enter() {
  sceneStartedAtMs_ = millis();
}

void DemoPage::update(uint32_t nowMs) {
  if ((nowMs - sceneStartedAtMs_) >= 2000UL) {
    sceneStartedAtMs_ = nowMs;
    advanceScene();
  }
}

void DemoPage::draw(Renderer& renderer) {
  renderer.clear(0);
  switch (scene_) {
    case DemoSceneId::Fill:
      renderer.clear(15);
      break;
    case DemoSceneId::Grayscale:
      for (int16_t x = 0; x < 80; ++x) {
        const uint8_t gray = static_cast<uint8_t>((x * 16) / 80);
        renderer.fillRect(x, 0, 1, 16, gray);
      }
      break;
    case DemoSceneId::Primitives:
      renderer.drawRect(0, 0, 80, 16, 15);
      renderer.drawLine(0, 0, 79, 15, 12);
      renderer.drawLine(79, 0, 0, 15, 12);
      renderer.fillRect(30, 4, 20, 8, 6);
      break;
    case DemoSceneId::Text:
      renderer.drawText(2, 5, String("DEMO TEXT"), 15);
      break;
    case DemoSceneId::MultilineText: {
      TextLayoutOptions options;
      options.maxWidth = 80;
      options.maxHeight = 16;
      options.lineSpacing = 1;
      renderer.drawTextBox(1, 1, options, String("LINE1\nLINE2\nLINE3"), 15);
      break;
    }
    case DemoSceneId::Clock:
      renderer.drawText(2, 2, clock_.timeString(), 15);
      renderer.drawText(2, 9, clock_.dateString(), 10);
      break;
    case DemoSceneId::Bitmap:
      renderer.drawBitmap(36, 4, kLogo, 8, 8, 15);
      break;
    case DemoSceneId::Transition: {
      const uint32_t phase = (millis() / 50UL) % 80UL;
      renderer.fillRect(0, 0, static_cast<int16_t>(phase), 16, 14);
      renderer.drawText(2, 5, String("SWEEP"), 2);
      break;
    }
    case DemoSceneId::Diagnostics: {
      renderer.drawText(0, 0, String("FPS ") + String(stats_.appFps), 15);
      renderer.drawText(0, 7, String("LAT ") + String(stats_.frameLatencyUs / 1000UL), 10);
      const uint8_t height = static_cast<uint8_t>(min<uint32_t>(15UL, stats_.appFps / 4UL));
      renderer.fillRect(79, 15 - height, 1, height, 15);
      break;
    }
  }
}

void DemoPage::advanceScene() {
  switch (scene_) {
    case DemoSceneId::Fill: scene_ = DemoSceneId::Grayscale; break;
    case DemoSceneId::Grayscale: scene_ = DemoSceneId::Primitives; break;
    case DemoSceneId::Primitives: scene_ = DemoSceneId::Text; break;
    case DemoSceneId::Text: scene_ = DemoSceneId::MultilineText; break;
    case DemoSceneId::MultilineText: scene_ = DemoSceneId::Clock; break;
    case DemoSceneId::Clock: scene_ = DemoSceneId::Bitmap; break;
    case DemoSceneId::Bitmap: scene_ = DemoSceneId::Transition; break;
    case DemoSceneId::Transition: scene_ = DemoSceneId::Diagnostics; break;
    case DemoSceneId::Diagnostics: scene_ = DemoSceneId::Fill; break;
  }
}
