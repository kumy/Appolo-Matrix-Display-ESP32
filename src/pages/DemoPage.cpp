#include "pages/DemoPage.h"

#include <cmath>

#include "display/GrayLevels.h"
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

const uint8_t kHeartBud[5 * 5] = {
    0, 0, 0, 0, 0,
    0, 0, 0, 0, 0,
    0, 0, 1, 0, 0,
    0, 0, 0, 0, 0,
    0, 0, 0, 0, 0,
};
const uint8_t kHeartSmall[5 * 5] = {
    0, 0, 0, 0, 0,
    0, 1, 0, 1, 0,
    0, 1, 1, 1, 0,
    0, 0, 1, 0, 0,
    0, 0, 0, 0, 0,
};
const uint8_t kHeartFull[5 * 5] = {
    0, 1, 0, 1, 0,
    1, 1, 1, 1, 1,
    1, 1, 1, 1, 1,
    0, 1, 1, 1, 0,
    0, 0, 1, 0, 0,
};
constexpr uint32_t kHeartLifetimeMs = 1800;

// Oscillates between [minValue, minValue+span], one unit per msPerStep,
// reflecting at both ends — a pure function of elapsed time, so every scene
// using it restarts from the same deterministic phase on each scene visit
// without needing any stored state.
int16_t pingPong(uint32_t elapsedMs, uint32_t msPerStep, int16_t minValue, int16_t span) {
  const uint32_t cycle = static_cast<uint32_t>(span) * 2UL;
  if (cycle == 0UL) {
    return minValue;
  }
  const uint32_t step = (elapsedMs / msPerStep) % cycle;
  const int16_t offset = step <= static_cast<uint32_t>(span) ? static_cast<int16_t>(step) : static_cast<int16_t>(cycle - step);
  return static_cast<int16_t>(minValue + offset);
}

// Ramps a shade up from black and back down once per periodMs, snapped to
// whatever the active palette actually offers.
uint8_t breathingShade(uint32_t elapsedMs, uint32_t periodMs) {
  const uint32_t half = periodMs / 2UL;
  const uint32_t phase = elapsedMs % periodMs;
  const uint32_t level = phase < half ? phase : (periodMs - phase);
  return GrayLevels::shade(level, half);
}
}

DemoPage::DemoPage(ClockService& clock, RuntimeStats& stats) : clock_(clock), stats_(stats) {}

void DemoPage::enter() {
  sceneStartedAtMs_ = millis();
  resetSceneState(sceneStartedAtMs_);
}

void DemoPage::update(uint32_t nowMs) {
  if ((nowMs - sceneStartedAtMs_) >= sceneDurationMs()) {
    sceneStartedAtMs_ = nowMs;
    advanceScene();
    resetSceneState(nowMs);
  }

  switch (scene_) {
    case DemoSceneId::Snake:
      updateSnake(nowMs);
      break;
    case DemoSceneId::SparklingHearts:
      updateHearts(nowMs);
      break;
    default:
      break;
  }
}

uint32_t DemoPage::sceneDurationMs() const {
  switch (scene_) {
    case DemoSceneId::Clock: return 3000UL;
    case DemoSceneId::MultilineText: return 7500UL;
    case DemoSceneId::Transition: return 6000UL;
    case DemoSceneId::Marquee: return 10000UL;
    case DemoSceneId::BouncingBall: return 5000UL;
    case DemoSceneId::SparklingHearts: return 6000UL;
    case DemoSceneId::Snake: return 10000UL;
    case DemoSceneId::SpaceInvaders: return 6000UL;
    default: return 2000UL;
  }
}

void DemoPage::resetSceneState(uint32_t nowMs) {
  switch (scene_) {
    case DemoSceneId::Snake:
      resetSnake();
      break;
    case DemoSceneId::SparklingHearts:
      for (auto& heart : hearts_) {
        heart.active = false;
      }
      heartsNextSpawnCheckMs_ = nowMs;
      break;
    default:
      break;
  }
}

void DemoPage::draw(Renderer& renderer) {
  renderer.clear(0);
  const uint32_t nowMs = millis();
  const uint32_t elapsed = nowMs - sceneStartedAtMs_;

  switch (scene_) {
    case DemoSceneId::Fill: {
      // Cycles through every configured level, one at a time, so this scene
      // and the Grayscale scene cover the palette in two complementary ways.
      constexpr uint32_t kMsPerLevel = 500UL;
      const size_t index = (elapsed / kMsPerLevel) % GrayLevels::kCount;
      renderer.clear(GrayLevels::kLevels[index]);
      break;
    }
    case DemoSceneId::Grayscale: {
      // Palette calibration view, fully driven by GrayLevels::kLevels so it
      // stays correct as the palette is tuned:
      //  - lower half: one solid band per non-black canonical level (black
      //    is indistinguishable from the cleared background, so skipped).
      //  - upper half: the numeric level value printed above its band, plus
      //    a vertical tick at each inter-band boundary reaching the top, so
      //    band edges are unambiguous even when two levels look close.
      constexpr size_t kBandCount = GrayLevels::kCount - 1;
      constexpr int16_t kGradientY = 8;
      constexpr int16_t kGradientHeight = 16 - kGradientY;
      constexpr int16_t kGlyphPitchPx = 4;  // must match Renderer's fixed glyph pitch

      for (size_t index = 0; index < kBandCount; ++index) {
        const int16_t x0 = static_cast<int16_t>((80U * index) / kBandCount);
        const int16_t x1 = static_cast<int16_t>((80U * (index + 1U)) / kBandCount);
        const int16_t bandWidth = static_cast<int16_t>(x1 - x0);
        const uint8_t level = GrayLevels::kLevels[index + 1];

        renderer.fillRect(x0, kGradientY, bandWidth, kGradientHeight, level);

        const String label(level);
        const int16_t labelWidth = static_cast<int16_t>(label.length() * kGlyphPitchPx);
        int16_t labelX = static_cast<int16_t>(x0 + (bandWidth - labelWidth) / 2);
        if (labelX < x0) {
          labelX = x0;
        }
        renderer.drawText(labelX, 0, label, GrayLevels::kFull);

        if (index > 0) {
          renderer.drawLine(x0, 0, x0, static_cast<int16_t>(kGradientY - 1), GrayLevels::kFull);
        }
      }
      break;
    }
    case DemoSceneId::Primitives:
      renderer.drawRect(0, 0, 80, 16, GrayLevels::kFull);
      renderer.drawLine(0, 0, 79, 15, GrayLevels::shade(2, 3));
      renderer.drawLine(79, 0, 0, 15, GrayLevels::shade(2, 3));
      renderer.fillRect(30, 4, 20, 8, GrayLevels::shade(1, 3));
      break;
    case DemoSceneId::Text:
      renderer.drawText(2, 5, String("DEMO TEXT"), breathingShade(elapsed, 1600UL));
      break;
    case DemoSceneId::MultilineText: {
      // Text is taller than the 16px display, so it scrolls upward from
      // below the bottom edge to above the top edge, then loops — letting
      // every line become readable in turn. FrameBuffer4's own per-pixel
      // bounds check does the vertical clipping, so no maxHeight is set.
      static const String kMessage("LINE1\nLINE2\nLINE3\nLINE4\nDONE");
      constexpr int16_t kLineHeightPx = 7;  // Renderer glyph height(6) + lineSpacing(1)
      constexpr int16_t kLineCount = 5;
      constexpr int16_t kTextHeight = kLineHeightPx * kLineCount;
      constexpr int16_t kScrollSpan = kTextHeight + 16;
      constexpr uint32_t kMsPerPixel = 140UL;

      const int16_t travel = static_cast<int16_t>((elapsed / kMsPerPixel) % static_cast<uint32_t>(kScrollSpan));
      const int16_t y = static_cast<int16_t>(16 - travel);

      TextLayoutOptions options;
      options.maxWidth = 80;
      options.lineSpacing = 1;
      renderer.drawTextBox(1, y, options, kMessage, GrayLevels::kFull);
      break;
    }
    case DemoSceneId::Clock: {
      renderer.drawText(1, 1, clock_.timeString(), GrayLevels::kFull);
      renderer.drawText(1, 9, clock_.dateString(), GrayLevels::shade(1, 2));

      // Analogue face alongside the digital readout — needs the numeric
      // hours()/minutes()/seconds() (added for this), not just the
      // formatted strings.
      constexpr int16_t kFaceCx = 61;
      constexpr int16_t kFaceCy = 8;
      constexpr int16_t kFaceRadius = 7;
      renderer.drawCircle(kFaceCx, kFaceCy, kFaceRadius, GrayLevels::shade(1, 3));

      constexpr float kPi = 3.14159265f;
      const uint8_t hour12 = static_cast<uint8_t>(clock_.hours() % 12U);
      const float hourAngle = (static_cast<float>(hour12) + static_cast<float>(clock_.minutes()) / 60.0f) * (kPi / 6.0f);
      const float minuteAngle = static_cast<float>(clock_.minutes()) * (kPi / 30.0f);
      const float secondAngle = static_cast<float>(clock_.seconds()) * (kPi / 30.0f);

      const int16_t hourLen = static_cast<int16_t>(kFaceRadius * 0.45f);
      const int16_t minuteLen = static_cast<int16_t>(kFaceRadius * 0.8f);
      const int16_t secondLen = static_cast<int16_t>(kFaceRadius * 0.9f);

      renderer.drawLine(kFaceCx, kFaceCy,
          static_cast<int16_t>(kFaceCx + hourLen * sinf(hourAngle)),
          static_cast<int16_t>(kFaceCy - hourLen * cosf(hourAngle)),
          GrayLevels::kFull);
      renderer.drawLine(kFaceCx, kFaceCy,
          static_cast<int16_t>(kFaceCx + minuteLen * sinf(minuteAngle)),
          static_cast<int16_t>(kFaceCy - minuteLen * cosf(minuteAngle)),
          GrayLevels::kFull);
      renderer.drawLine(kFaceCx, kFaceCy,
          static_cast<int16_t>(kFaceCx + secondLen * sinf(secondAngle)),
          static_cast<int16_t>(kFaceCy - secondLen * cosf(secondAngle)),
          GrayLevels::shade(1, 2));
      break;
    }
    case DemoSceneId::Bitmap:
      renderer.drawBitmap(36, 4, kLogo, 8, 8, breathingShade(elapsed, 1200UL));
      break;
    case DemoSceneId::Transition: {
      constexpr int16_t kSweepSpan = 79;
      constexpr int16_t kTrailLength = 28;
      // Overtravel the head past both edges by a full trail length so the
      // whole beam (head + trail) slides off-screen and disappears before
      // reversing, instead of snapping direction the instant the head
      // touches the border.
      constexpr int16_t kVirtualMin = -kTrailLength;
      constexpr int16_t kVirtualMax = kSweepSpan + kTrailLength;
      constexpr int16_t kVirtualSpan = kVirtualMax - kVirtualMin;
      constexpr int16_t kBandHeight = 4;
      constexpr int16_t kBandY = (16 - kBandHeight) / 2;

      const uint32_t step = (elapsed / 28UL) % (static_cast<uint32_t>(kVirtualSpan) * 2UL);
      int16_t headX = 0;
      int16_t direction = 1;
      if (step <= static_cast<uint32_t>(kVirtualSpan)) {
        headX = static_cast<int16_t>(kVirtualMin + static_cast<int16_t>(step));
        direction = 1;
      } else {
        headX = static_cast<int16_t>(kVirtualMax - static_cast<int16_t>(step - static_cast<uint32_t>(kVirtualSpan)));
        direction = -1;
      }

      renderer.fillRect(headX, kBandY, 1, kBandHeight, GrayLevels::kFull);

      // Trail fades procedurally through GrayLevels::shade() instead of a
      // fixed-size lookup array, so it automatically uses however many
      // levels the palette actually has (finer fade with a bigger palette,
      // coarser with a smaller one) instead of silently requantizing when
      // the palette changes.
      for (int16_t index = 0; index < kTrailLength; ++index) {
        const int16_t x = static_cast<int16_t>(headX - ((index + 1) * direction));
        if (x < 0 || x >= 80) {
          continue;
        }
        const uint8_t level = GrayLevels::shade(static_cast<uint32_t>(kTrailLength - index), static_cast<uint32_t>(kTrailLength + 1));
        if (level == GrayLevels::kBlack) {
          continue;
        }
        renderer.fillRect(x, kBandY, 1, kBandHeight, level);
      }

      renderer.drawRect(0, 1, 80, 14, GrayLevels::kLevels[1]);
      break;
    }
    case DemoSceneId::Checkerboard: {
      // Diagonal dithered blend cycling through every configured level, not
      // just a binary on/off pattern.
      const uint32_t phase = nowMs / 250UL;
      for (int16_t y = 0; y < 16; ++y) {
        for (int16_t x = 0; x < 80; ++x) {
          const uint32_t cell = (static_cast<uint32_t>(x + y) + phase) % GrayLevels::kCount;
          renderer.drawPixel(x, y, GrayLevels::kLevels[cell]);
        }
      }
      break;
    }
    case DemoSceneId::EdgeStress:
      renderer.drawRect(0, 0, 80, 16, GrayLevels::kFull);
      for (int16_t y = 0; y < 16; y += 2) {
        renderer.drawLine(0, y, 79, y, GrayLevels::shade(1, 2));
      }
      for (int16_t x = 0; x < 80; x += 4) {
        renderer.drawLine(x, 0, x, 15, (x % 8 == 0) ? GrayLevels::kFull : GrayLevels::shade(1, 4));
      }
      break;
    case DemoSceneId::Diagnostics: {
      renderer.drawText(0, 0, String("FPS ") + String(stats_.appFps), GrayLevels::kFull);
      renderer.drawText(0, 7, String("LAT ") + String(stats_.frameLatencyUs / 1000UL), GrayLevels::shade(1, 2));
      const uint8_t height = static_cast<uint8_t>(min<uint32_t>(15UL, stats_.appFps / 4UL));
      for (uint8_t row = 0; row < height; ++row) {
        renderer.drawPixel(79, static_cast<int16_t>(15 - row), GrayLevels::shade(row + 1U, height));
      }
      break;
    }
    case DemoSceneId::Marquee: {
      static const String kMessage("MATRIX DISPLAY 2026 -- ");
      constexpr int16_t kCharPitch = 4;
      const int16_t textWidth = static_cast<int16_t>(kMessage.length() * kCharPitch);
      constexpr uint32_t kMsPerPixel = 60UL;
      const int16_t cycle = static_cast<int16_t>(textWidth + 80);
      const int16_t travel = static_cast<int16_t>((elapsed / kMsPerPixel) % static_cast<uint32_t>(cycle));
      const int16_t x = static_cast<int16_t>(80 - travel);
      renderer.drawText(x, 5, kMessage, GrayLevels::kFull);
      break;
    }
    case DemoSceneId::BouncingBall: {
      // Closed-form ping-pong bounce (no stored velocity/position): each
      // axis is an independent triangle wave over elapsed scene time, so
      // the ball always restarts from the same deterministic spot.
      constexpr int16_t kRadius = 2;
      constexpr int16_t kSpanX = 79 - 2 * kRadius;
      constexpr int16_t kSpanY = 15 - 2 * kRadius;
      const int16_t ballX = pingPong(elapsed, 17UL, kRadius, kSpanX);
      const int16_t ballY = pingPong(elapsed, 29UL, kRadius, kSpanY);
      renderer.drawCircle(ballX, ballY, static_cast<int16_t>(kRadius + 1), GrayLevels::shade(1, 3));
      renderer.fillCircle(ballX, ballY, kRadius, GrayLevels::kFull);
      break;
    }
    case DemoSceneId::SparklingHearts: {
      for (size_t i = 0; i < kHeartSlotCount; ++i) {
        const HeartSlot& heart = hearts_[i];
        if (!heart.active) {
          continue;
        }
        const uint32_t age = nowMs - heart.spawnMs;
        const uint32_t bloomEnd = (kHeartLifetimeMs * 2UL) / 3UL;
        const uint8_t* bitmap = kHeartFull;
        if (age < kHeartLifetimeMs / 3UL) {
          bitmap = kHeartBud;
        } else if (age < bloomEnd) {
          bitmap = kHeartSmall;
        }

        uint32_t brightNum;
        uint32_t brightDen;
        if (age < bloomEnd) {
          // Grows in size (bud -> small -> full, above) and brightness
          // together, like a flower opening.
          brightNum = age;
          brightDen = bloomEnd;
        } else {
          // Holds full bloom, then fades out.
          const uint32_t fadeTotal = kHeartLifetimeMs - bloomEnd;
          const uint32_t fadeElapsed = age - bloomEnd;
          brightNum = fadeTotal > fadeElapsed ? fadeTotal - fadeElapsed : 0UL;
          brightDen = fadeTotal;
        }
        const uint8_t gray = GrayLevels::shade(brightNum, brightDen);
        if (gray != GrayLevels::kBlack) {
          renderer.drawBitmap(heart.x, heart.y, bitmap, 5, 5, gray);
        }
      }
      break;
    }
    case DemoSceneId::Snake: {
      constexpr int16_t kCell = SnakeState::kCellSize;
      const uint8_t foodShade = ((nowMs / 200UL) & 1U) ? GrayLevels::kFull : GrayLevels::shade(1, 2);
      renderer.fillRect(static_cast<int16_t>(snake_.foodX * kCell), static_cast<int16_t>(snake_.foodY * kCell), kCell, kCell, foodShade);

      for (uint8_t i = 0; i < snake_.length; ++i) {
        // Head brightest, fading toward the tail.
        const uint8_t bodyShade = GrayLevels::shade(static_cast<uint32_t>(snake_.length - i), static_cast<uint32_t>(snake_.length));
        renderer.fillRect(static_cast<int16_t>(snake_.bodyX[i] * kCell), static_cast<int16_t>(snake_.bodyY[i] * kCell), kCell, kCell, bodyShade);
      }
      break;
    }
    case DemoSceneId::SpaceInvaders: {
      constexpr uint8_t kCols = 6;
      constexpr uint8_t kRows = 2;
      constexpr int16_t kCellW = 8;
      constexpr int16_t kCellH = 5;
      constexpr int16_t kAlienW = 5;
      constexpr int16_t kAlienH = 3;
      constexpr int16_t kFormationWidth = kCols * kCellW;
      constexpr int16_t kMarchSpan = 80 - kFormationWidth;
      const int16_t originX = pingPong(elapsed, 90UL, 0, kMarchSpan);
      constexpr int16_t kOriginY = 0;

      for (uint8_t row = 0; row < kRows; ++row) {
        for (uint8_t col = 0; col < kCols; ++col) {
          const int16_t ax = static_cast<int16_t>(originX + col * kCellW);
          const int16_t ay = static_cast<int16_t>(kOriginY + row * kCellH);
          renderer.fillRect(ax, ay, kAlienW, kAlienH, GrayLevels::shade(row + 1U, kRows + 1U));
        }
      }

      constexpr int16_t kCannonW = 3;
      constexpr int16_t kCannonY = 13;
      const int16_t cannonX = pingPong(elapsed, 40UL, 0, 80 - kCannonW);
      renderer.fillRect(cannonX, kCannonY, kCannonW, 2, GrayLevels::kFull);

      constexpr uint32_t kShotPeriodMs = 900UL;
      constexpr uint32_t kShotTravelMs = 500UL;
      const uint32_t shotPhase = elapsed % kShotPeriodMs;
      if (shotPhase < kShotTravelMs) {
        const int16_t shotY = static_cast<int16_t>(kCannonY - 1 - (shotPhase * (kCannonY - 1)) / kShotTravelMs);
        if (shotY >= 0) {
          renderer.drawPixel(static_cast<int16_t>(cannonX + kCannonW / 2), shotY, GrayLevels::shade(2, 3));
        }
      }
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
    case DemoSceneId::Transition: scene_ = DemoSceneId::Checkerboard; break;
    case DemoSceneId::Checkerboard: scene_ = DemoSceneId::EdgeStress; break;
    case DemoSceneId::EdgeStress: scene_ = DemoSceneId::Diagnostics; break;
    case DemoSceneId::Diagnostics: scene_ = DemoSceneId::Marquee; break;
    case DemoSceneId::Marquee: scene_ = DemoSceneId::BouncingBall; break;
    case DemoSceneId::BouncingBall: scene_ = DemoSceneId::SparklingHearts; break;
    case DemoSceneId::SparklingHearts: scene_ = DemoSceneId::Snake; break;
    case DemoSceneId::Snake: scene_ = DemoSceneId::SpaceInvaders; break;
    case DemoSceneId::SpaceInvaders: scene_ = DemoSceneId::Fill; break;
  }
}

bool DemoPage::snakeBodyContains(uint8_t x, uint8_t y, uint8_t count) const {
  for (uint8_t i = 0; i < count && i < SnakeState::kMaxLength; ++i) {
    if (snake_.bodyX[i] == x && snake_.bodyY[i] == y) {
      return true;
    }
  }
  return false;
}

void DemoPage::spawnSnakeFood() {
  for (uint8_t attempt = 0; attempt < 50; ++attempt) {
    const uint8_t x = static_cast<uint8_t>(random(0, SnakeState::kGridCols));
    const uint8_t y = static_cast<uint8_t>(random(0, SnakeState::kGridRows));
    if (!snakeBodyContains(x, y, snake_.length)) {
      snake_.foodX = x;
      snake_.foodY = y;
      return;
    }
  }
}

void DemoPage::resetSnake() {
  snake_.length = 3;
  snake_.dirX = 1;
  snake_.dirY = 0;
  const uint8_t startX = static_cast<uint8_t>(SnakeState::kGridCols / 4);
  const uint8_t startY = static_cast<uint8_t>(SnakeState::kGridRows / 2);
  for (uint8_t i = 0; i < snake_.length; ++i) {
    snake_.bodyX[i] = static_cast<uint8_t>(startX - i);
    snake_.bodyY[i] = startY;
  }
  snake_.lastStepMs = millis();
  spawnSnakeFood();
}

void DemoPage::updateSnake(uint32_t nowMs) {
  constexpr uint32_t kStepIntervalMs = 140UL;
  if (nowMs - snake_.lastStepMs < kStepIntervalMs) {
    return;
  }
  snake_.lastStepMs = nowMs;

  const uint8_t headX = snake_.bodyX[0];
  const uint8_t headY = snake_.bodyY[0];
  const int16_t dxToFood = static_cast<int16_t>(snake_.foodX) - static_cast<int16_t>(headX);
  const int16_t dyToFood = static_cast<int16_t>(snake_.foodY) - static_cast<int16_t>(headY);

  // Candidate directions in priority order: toward food on the axis with
  // the larger gap first, then the other axis, then keep going straight,
  // then turn either way, then reverse as a last resort. First one that
  // doesn't hit a wall or its own body wins.
  int8_t candidateX[4];
  int8_t candidateY[4];
  uint8_t candidateCount = 0;
  auto addCandidate = [&](int8_t dx, int8_t dy) {
    if (dx == 0 && dy == 0) {
      return;
    }
    for (uint8_t i = 0; i < candidateCount; ++i) {
      if (candidateX[i] == dx && candidateY[i] == dy) {
        return;
      }
    }
    candidateX[candidateCount] = dx;
    candidateY[candidateCount] = dy;
    ++candidateCount;
  };

  if (abs(dxToFood) >= abs(dyToFood)) {
    if (dxToFood != 0) addCandidate(dxToFood > 0 ? 1 : -1, 0);
    if (dyToFood != 0) addCandidate(0, dyToFood > 0 ? 1 : -1);
  } else {
    if (dyToFood != 0) addCandidate(0, dyToFood > 0 ? 1 : -1);
    if (dxToFood != 0) addCandidate(dxToFood > 0 ? 1 : -1, 0);
  }
  addCandidate(snake_.dirX, snake_.dirY);
  addCandidate(static_cast<int8_t>(-snake_.dirY), static_cast<int8_t>(snake_.dirX));
  addCandidate(snake_.dirY, static_cast<int8_t>(-snake_.dirX));
  addCandidate(static_cast<int8_t>(-snake_.dirX), static_cast<int8_t>(-snake_.dirY));

  for (uint8_t i = 0; i < candidateCount; ++i) {
    const int16_t nextX = static_cast<int16_t>(headX) + candidateX[i];
    const int16_t nextY = static_cast<int16_t>(headY) + candidateY[i];
    if (nextX < 0 || nextX >= SnakeState::kGridCols || nextY < 0 || nextY >= SnakeState::kGridRows) {
      continue;
    }
    const bool eating = (static_cast<uint8_t>(nextX) == snake_.foodX && static_cast<uint8_t>(nextY) == snake_.foodY);
    // The tail cell vacates this step unless we're growing, so it's a safe
    // target square even though it's technically still "occupied" now.
    const uint8_t bodyCheckLength = eating ? snake_.length : static_cast<uint8_t>(snake_.length - 1U);
    if (snakeBodyContains(static_cast<uint8_t>(nextX), static_cast<uint8_t>(nextY), bodyCheckLength)) {
      continue;
    }

    snake_.dirX = candidateX[i];
    snake_.dirY = candidateY[i];
    const uint8_t newLength = eating
        ? static_cast<uint8_t>(min<uint8_t>(SnakeState::kMaxLength, static_cast<uint8_t>(snake_.length + 1U)))
        : snake_.length;
    for (uint8_t j = static_cast<uint8_t>(newLength - 1U); j > 0; --j) {
      snake_.bodyX[j] = snake_.bodyX[j - 1U];
      snake_.bodyY[j] = snake_.bodyY[j - 1U];
    }
    snake_.bodyX[0] = static_cast<uint8_t>(nextX);
    snake_.bodyY[0] = static_cast<uint8_t>(nextY);
    snake_.length = newLength;
    if (eating) {
      spawnSnakeFood();
    }
    return;
  }

  // No legal move in any candidate direction: fully boxed in by its own
  // body. Respawn rather than freezing, so the demo stays alive forever.
  resetSnake();
}

void DemoPage::updateHearts(uint32_t nowMs) {
  for (auto& heart : hearts_) {
    if (heart.active && (nowMs - heart.spawnMs) >= kHeartLifetimeMs) {
      heart.active = false;
    }
  }

  if (nowMs < heartsNextSpawnCheckMs_) {
    return;
  }
  heartsNextSpawnCheckMs_ = nowMs + 250UL;

  for (auto& heart : hearts_) {
    if (!heart.active && random(0, 100) < 35) {
      heart.active = true;
      heart.spawnMs = nowMs;
      heart.x = static_cast<int16_t>(random(0, 76));
      heart.y = static_cast<int16_t>(random(0, 12));
      break;
    }
  }
}
