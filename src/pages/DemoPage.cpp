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
const uint8_t kGrayscaleRamp[] = {0, 3, 9, 12, 15};
constexpr size_t kGrayscaleRampCount = sizeof(kGrayscaleRamp) / sizeof(kGrayscaleRamp[0]);
const uint8_t kK2000TrailLevels[] = {
  13, 13,
  10, 10, 10,
  7, 7, 7, 7,
  4, 4, 4, 4, 4, 4,
  0, 0, 0,
};
constexpr size_t kK2000TrailLength = sizeof(kK2000TrailLevels) / sizeof(kK2000TrailLevels[0]);
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
      for (size_t index = 0; index < kGrayscaleRampCount; ++index) {
        const int16_t x0 = static_cast<int16_t>((80U * index) / kGrayscaleRampCount);
        const int16_t x1 = static_cast<int16_t>((80U * (index + 1U)) / kGrayscaleRampCount);
        renderer.fillRect(x0, 0, static_cast<int16_t>(x1 - x0), 16, kGrayscaleRamp[index]);
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
      constexpr int16_t kSweepSpan = 79;
      const uint32_t step = (millis() / 28UL) % (static_cast<uint32_t>(kSweepSpan) * 2UL);
      int16_t headX = 0;
      int16_t direction = 1;
      if (step <= static_cast<uint32_t>(kSweepSpan)) {
        headX = static_cast<int16_t>(step);
        direction = 1;
      } else {
        headX = static_cast<int16_t>((static_cast<uint32_t>(kSweepSpan) * 2UL) - step);
        direction = -1;
      }

      renderer.fillRect(headX, 2, 1, 12, 15);

      for (size_t index = 0; index < kK2000TrailLength; ++index) {
        const int16_t x = static_cast<int16_t>(headX - ((static_cast<int16_t>(index) + 1) * direction));
        if (x < 0 || x >= 80) {
          continue;
        }
        renderer.fillRect(x, 2, 1, 12, kK2000TrailLevels[index]);
      }

      renderer.drawRect(0, 1, 80, 14, 2);
      break;
    }
    case DemoSceneId::Checkerboard:
      for (int16_t y = 0; y < 16; ++y) {
        for (int16_t x = 0; x < 80; ++x) {
          renderer.drawPixel(x, y, ((x + y + (millis() / 250UL)) & 1U) == 0U ? 15 : 0);
        }
      }
      break;
    case DemoSceneId::EdgeStress:
      renderer.drawRect(0, 0, 80, 16, 15);
      for (int16_t y = 0; y < 16; y += 2) {
        renderer.drawLine(0, y, 79, y, 8);
      }
      for (int16_t x = 0; x < 80; x += 4) {
        renderer.drawLine(x, 0, x, 15, (x % 8 == 0) ? 15 : 4);
      }
      break;
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
    case DemoSceneId::Grayscale: scene_ = DemoSceneId::Transition; break;
    case DemoSceneId::Transition: scene_ = DemoSceneId::Fill; break;
    case DemoSceneId::EdgeStress: scene_ = DemoSceneId::Fill; break;
    // case DemoSceneId::Fill: scene_ = DemoSceneId::Grayscale; break;
    // case DemoSceneId::Grayscale: scene_ = DemoSceneId::Primitives; break;
    // case DemoSceneId::Primitives: scene_ = DemoSceneId::Text; break;
    // case DemoSceneId::Text: scene_ = DemoSceneId::MultilineText; break;
    // case DemoSceneId::MultilineText: scene_ = DemoSceneId::Clock; break;
    // case DemoSceneId::Clock: scene_ = DemoSceneId::Bitmap; break;
    // case DemoSceneId::Bitmap: scene_ = DemoSceneId::Transition; break;
    // case DemoSceneId::Transition: scene_ = DemoSceneId::Checkerboard; break;
    // case DemoSceneId::Checkerboard: scene_ = DemoSceneId::EdgeStress; break;
    // case DemoSceneId::EdgeStress: scene_ = DemoSceneId::Diagnostics; break;
    // case DemoSceneId::Diagnostics: scene_ = DemoSceneId::Fill; break;
  }
}
