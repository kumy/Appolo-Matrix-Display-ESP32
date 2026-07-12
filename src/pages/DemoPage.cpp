#include "pages/DemoPage.h"

#include <cmath>
#include <cstring>

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

// Space Invaders sprites, 7x5, two walk frames (legs alternate).
const uint8_t kAlienFrameA[7 * 5] = {
    0, 0, 1, 1, 1, 0, 0,
    0, 1, 1, 1, 1, 1, 0,
    1, 1, 0, 1, 0, 1, 1,
    1, 1, 1, 1, 1, 1, 1,
    0, 1, 0, 0, 0, 1, 0,
};
const uint8_t kAlienFrameB[7 * 5] = {
    0, 0, 1, 1, 1, 0, 0,
    0, 1, 1, 1, 1, 1, 0,
    1, 1, 0, 1, 0, 1, 1,
    1, 1, 1, 1, 1, 1, 1,
    1, 0, 1, 0, 1, 0, 1,
};
const uint8_t kCannonSprite[3 * 3] = {
    0, 1, 0,
    1, 1, 1,
    1, 1, 1,
};

// Mario sprites, 6x7, two walk frames.
const uint8_t kMarioFrameA[6 * 7] = {
    0, 0, 1, 1, 1, 0,
    0, 1, 1, 1, 1, 1,
    1, 1, 0, 1, 0, 0,
    1, 1, 1, 1, 1, 0,
    0, 0, 1, 1, 1, 0,
    0, 1, 1, 0, 1, 1,
    1, 1, 0, 0, 0, 1,
};
const uint8_t kMarioFrameB[6 * 7] = {
    0, 0, 1, 1, 1, 0,
    0, 1, 1, 1, 1, 1,
    1, 1, 0, 1, 0, 0,
    1, 1, 1, 1, 1, 0,
    0, 0, 1, 1, 1, 0,
    1, 1, 0, 1, 1, 0,
    0, 0, 1, 0, 0, 1,
};
// Goomba sprites, 5x4, two walk frames.
const uint8_t kGoombaFrameA[5 * 4] = {
    0, 1, 1, 1, 0,
    1, 1, 1, 1, 1,
    1, 0, 1, 0, 1,
    0, 1, 0, 1, 0,
};
const uint8_t kGoombaFrameB[5 * 4] = {
    0, 1, 1, 1, 0,
    1, 1, 1, 1, 1,
    1, 0, 1, 0, 1,
    1, 0, 0, 0, 1,
};
const uint8_t kMushroomSprite[5 * 4] = {
    0, 1, 1, 1, 0,
    1, 1, 0, 1, 1,
    1, 1, 1, 1, 1,
    0, 1, 1, 1, 0,
};
const uint8_t kQuestionBlockSprite[5 * 5] = {
    1, 1, 1, 1, 1,
    1, 0, 0, 0, 1,
    1, 0, 1, 0, 1,
    1, 0, 0, 0, 1,
    1, 1, 1, 1, 1,
};

constexpr uint32_t kFireworkSparkLifetimeMs = 700;

// Tetromino rotation table: [pieceType][rotation][cell][0=fallOffset,
// 1=lane]. fallOffset grows away from the spawn edge (toward the floor);
// lane is the perpendicular axis a piece can be offset along. Pieces with
// fewer than 4 distinct rotations repeat entries so every piece uniformly
// has 4 rotation slots and callers never need to special-case rotation
// count. Order: I, O, T, S, Z, J, L.
constexpr int8_t kPieceShapes[7][4][4][2] = {
    // I
    {
        {{1, 0}, {1, 1}, {1, 2}, {1, 3}},
        {{0, 2}, {1, 2}, {2, 2}, {3, 2}},
        {{1, 0}, {1, 1}, {1, 2}, {1, 3}},
        {{0, 2}, {1, 2}, {2, 2}, {3, 2}},
    },
    // O
    {
        {{0, 1}, {0, 2}, {1, 1}, {1, 2}},
        {{0, 1}, {0, 2}, {1, 1}, {1, 2}},
        {{0, 1}, {0, 2}, {1, 1}, {1, 2}},
        {{0, 1}, {0, 2}, {1, 1}, {1, 2}},
    },
    // T
    {
        {{0, 1}, {1, 0}, {1, 1}, {1, 2}},
        {{0, 1}, {1, 1}, {1, 2}, {2, 1}},
        {{1, 0}, {1, 1}, {1, 2}, {2, 1}},
        {{0, 1}, {1, 0}, {1, 1}, {2, 1}},
    },
    // S
    {
        {{0, 1}, {0, 2}, {1, 0}, {1, 1}},
        {{0, 1}, {1, 1}, {1, 2}, {2, 2}},
        {{0, 1}, {0, 2}, {1, 0}, {1, 1}},
        {{0, 1}, {1, 1}, {1, 2}, {2, 2}},
    },
    // Z
    {
        {{0, 0}, {0, 1}, {1, 1}, {1, 2}},
        {{0, 2}, {1, 1}, {1, 2}, {2, 1}},
        {{0, 0}, {0, 1}, {1, 1}, {1, 2}},
        {{0, 2}, {1, 1}, {1, 2}, {2, 1}},
    },
    // J
    {
        {{0, 0}, {1, 0}, {1, 1}, {1, 2}},
        {{0, 1}, {0, 2}, {1, 1}, {2, 1}},
        {{1, 0}, {1, 1}, {1, 2}, {2, 2}},
        {{0, 1}, {1, 1}, {2, 0}, {2, 1}},
    },
    // L
    {
        {{0, 2}, {1, 0}, {1, 1}, {1, 2}},
        {{0, 1}, {1, 1}, {2, 1}, {2, 2}},
        {{1, 0}, {1, 1}, {1, 2}, {2, 0}},
        {{0, 0}, {0, 1}, {1, 1}, {2, 1}},
    },
};

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

void DemoPage::setAnimationSpeedPercent(uint16_t percent) {
  animationSpeedPercent_ = percent < 1U ? 1U : percent;
}

uint32_t DemoPage::scaleMs(uint32_t ms) const {
  return static_cast<uint32_t>((static_cast<uint64_t>(ms) * animationSpeedPercent_) / 100ULL);
}

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
    case DemoSceneId::Tetris:
      updateTetris(nowMs);
      break;
    case DemoSceneId::Pong:
      updatePong(nowMs);
      break;
    case DemoSceneId::Fireworks:
      updateFireworks(nowMs);
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
    case DemoSceneId::SpaceInvaders: return 8000UL;
    case DemoSceneId::Tetris: return 14000UL;
    case DemoSceneId::Pong: return 12000UL;
    case DemoSceneId::Mario: return 9000UL;
    case DemoSceneId::Fireworks: return 8000UL;
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
    case DemoSceneId::Tetris:
      resetTetris();
      break;
    case DemoSceneId::Pong:
      resetPong();
      break;
    case DemoSceneId::Fireworks:
      resetFireworks(nowMs);
      break;
    default:
      break;
  }
}

void DemoPage::draw(Renderer& renderer) {
  renderer.clear(0);
  const uint32_t nowMs = millis();
  // Single scaling point: every scene's continuous motion (pingPong,
  // modulo-cycle scrolling, etc.) reads elapsed time from this one
  // variable, so scaling it here speeds up/slows down all of them
  // together without touching individual scene code.
  const uint32_t elapsed = scaleMs(nowMs - sceneStartedAtMs_);

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
        renderer.drawText(labelX, 0, label, level);

        if (index > 0) {
          renderer.drawLine(x0, 7, x0, static_cast<int16_t>(kGradientY - 1), GrayLevels::kFull);
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
      // every line become readable in turn. FrameBuffer5's own per-pixel
      // bounds check does the vertical clipping, so no maxHeight is set.
      static const String kMessage("LINE1\nLINE2\nLINE3\nLINE4\nDONE");
      constexpr int16_t kLineHeightPx = 7;  // Renderer glyph height(6) + lineSpacing(1)
      constexpr int16_t kLineCount = 5;
      constexpr int16_t kTextHeight = kLineHeightPx * kLineCount;
      constexpr int16_t kScrollSpan = kTextHeight + 16;
      // 41ms/px (~24Hz) rather than the original 140ms (~7Hz) — content
      // updates below ~24Hz read as visibly juddery/blurred to the eye
      // (the renderer only places text at integer pixel positions, no
      // sub-pixel blending, so update FREQUENCY is the only lever here).
      constexpr uint32_t kMsPerPixel = 41UL;

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
      // 41ms/px (~24Hz) rather than the original 60ms (~17Hz) — see the
      // MultilineText scene's comment on why frequency is the only lever
      // available for integer-pixel-positioned text.
      constexpr uint32_t kMsPerPixel = 41UL;
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
      // Real animated sprites (not squares): two walk frames toggling as
      // the whole formation marches side to side, plus a slow sawtooth
      // descent (advance a little each "wave", snap back to the top for
      // the next one) standing in for a 2D display's version of "coming at
      // you". Back row rendered dimmer than the front row for a depth cue.
      constexpr uint8_t kCols = 6;
      constexpr uint8_t kRows = 2;
      constexpr int16_t kAlienW = 7;
      constexpr int16_t kAlienH = 5;
      constexpr int16_t kCellW = 10;
      constexpr int16_t kCellH = 5;
      constexpr int16_t kFormationWidth = kCols * kCellW;
      constexpr int16_t kMarchSpan = 80 - kFormationWidth;
      constexpr int16_t kDescendMax = 3;
      constexpr uint32_t kDescendCycleMs = 10000UL;

      // 41ms (~24Hz) rather than the original 90ms (~11Hz) — see the
      // MultilineText scene's comment on why update frequency is the
      // lever for motion smoothness here.
      const int16_t originX = pingPong(elapsed, 41UL, 0, kMarchSpan);
      const uint32_t descendPhase = elapsed % kDescendCycleMs;
      const int16_t originY = static_cast<int16_t>((static_cast<uint32_t>(kDescendMax) * descendPhase) / kDescendCycleMs);
      const uint8_t walkFrame = static_cast<uint8_t>((elapsed / 400UL) % 2UL);
      const uint8_t* alienBitmap = walkFrame == 0 ? kAlienFrameA : kAlienFrameB;

      for (uint8_t row = 0; row < kRows; ++row) {
        const uint8_t rowShade = GrayLevels::shade(row + 1U, kRows + 1U);
        for (uint8_t col = 0; col < kCols; ++col) {
          const int16_t ax = static_cast<int16_t>(originX + col * kCellW);
          const int16_t ay = static_cast<int16_t>(originY + row * kCellH);
          renderer.drawBitmap(ax, ay, alienBitmap, kAlienW, kAlienH, rowShade);
        }
      }

      constexpr int16_t kCannonSize = 3;
      constexpr int16_t kCannonY = 13;
      const int16_t cannonX = pingPong(elapsed, 40UL, 0, 80 - kCannonSize);
      renderer.drawBitmap(cannonX, kCannonY, kCannonSprite, kCannonSize, kCannonSize, GrayLevels::kFull);

      constexpr uint32_t kShotPeriodMs = 900UL;
      constexpr uint32_t kShotTravelMs = 500UL;
      const uint32_t shotPhase = elapsed % kShotPeriodMs;
      if (shotPhase < kShotTravelMs) {
        const int16_t shotY = static_cast<int16_t>(kCannonY - 1 - (shotPhase * (kCannonY - 1)) / kShotTravelMs);
        if (shotY >= 0) {
          renderer.drawPixel(static_cast<int16_t>(cannonX + kCannonSize / 2), shotY, GrayLevels::shade(2, 3));
        }
      }
      break;
    }
    case DemoSceneId::Tetris: {
      constexpr int16_t kCell = TetrisState::kCellSize;
      for (uint8_t d = 0; d < TetrisState::kDepth; ++d) {
        for (uint8_t lane = 0; lane < TetrisState::kLanes; ++lane) {
          const uint8_t shade = tetris_.board[d][lane];
          if (shade == 0) {
            continue;
          }
          renderer.fillRect(static_cast<int16_t>(d * kCell), static_cast<int16_t>(lane * kCell), kCell, kCell, shade);
        }
      }
      for (uint8_t i = 0; i < 4; ++i) {
        const int16_t d = static_cast<int16_t>(tetris_.pieceDepth - kPieceShapes[tetris_.pieceType][tetris_.pieceRotation][i][0]);
        const int16_t lane = static_cast<int16_t>(tetris_.pieceLane + kPieceShapes[tetris_.pieceType][tetris_.pieceRotation][i][1]);
        if (d < 0 || d >= static_cast<int16_t>(TetrisState::kDepth) || lane < 0 || lane >= static_cast<int16_t>(TetrisState::kLanes)) {
          continue;
        }
        renderer.fillRect(static_cast<int16_t>(d * kCell), static_cast<int16_t>(lane * kCell), kCell, kCell, tetris_.pieceShade);
      }
      break;
    }
    case DemoSceneId::Pong: {
      renderer.drawLine(0, 6, 79, 6, GrayLevels::shade(1, 4));
      const String score = String(pong_.leftScore) + String("-") + String(pong_.rightScore);
      const int16_t scoreX = static_cast<int16_t>(40 - static_cast<int16_t>(score.length()) * 2);
      renderer.drawText(scoreX, 0, score, GrayLevels::kFull);

      renderer.fillRect(2, static_cast<int16_t>(pong_.leftPaddleY), 2, 4, GrayLevels::kFull);
      renderer.fillRect(76, static_cast<int16_t>(pong_.rightPaddleY), 2, 4, GrayLevels::kFull);
      renderer.fillCircle(static_cast<int16_t>(pong_.ballX), static_cast<int16_t>(pong_.ballY), 1, GrayLevels::shade(2, 3));
      break;
    }
    case DemoSceneId::Mario: {
      constexpr uint32_t kCycleMs = 8000UL;
      const uint32_t t = elapsed % kCycleMs;

      renderer.drawLine(0, 15, 79, 15, GrayLevels::shade(1, 3));

      constexpr int16_t kBlockX = 40;
      constexpr int16_t kBlockY = 2;
      const int16_t bob = ((t / 500UL) % 2UL == 0UL) ? 0 : 1;
      renderer.drawBitmap(kBlockX, static_cast<int16_t>(kBlockY + bob), kQuestionBlockSprite, 5, 5, GrayLevels::shade(2, 3));

      constexpr int16_t kMarioStartX = -6;
      constexpr int16_t kMarioEndX = 66;
      const int16_t marioX = static_cast<int16_t>(kMarioStartX + (static_cast<int64_t>(kMarioEndX - kMarioStartX) * t) / kCycleMs);
      const uint8_t marioFrame = static_cast<uint8_t>((t / 200UL) % 2UL);
      renderer.drawBitmap(marioX, 8, marioFrame == 0 ? kMarioFrameA : kMarioFrameB, 6, 7, GrayLevels::kFull);

      constexpr uint32_t kMushroomStart = 3000UL;
      constexpr uint32_t kMushroomEnd = 5500UL;
      if (t >= kMushroomStart && t < kMushroomEnd) {
        const uint32_t mt = t - kMushroomStart;
        const int16_t mushroomX = static_cast<int16_t>(kBlockX + (mt * 24UL) / (kMushroomEnd - kMushroomStart));
        renderer.drawBitmap(mushroomX, 9, kMushroomSprite, 5, 4, GrayLevels::kFull);
      }

      constexpr int16_t kGoombaStartX = 80;
      constexpr int16_t kGoombaEndX = -5;
      const int16_t goombaX = static_cast<int16_t>(kGoombaStartX + (static_cast<int64_t>(kGoombaEndX - kGoombaStartX) * t) / kCycleMs);
      const uint8_t goombaFrame = static_cast<uint8_t>((t / 220UL) % 2UL);
      renderer.drawBitmap(goombaX, 11, goombaFrame == 0 ? kGoombaFrameA : kGoombaFrameB, 5, 4, GrayLevels::shade(1, 2));
      break;
    }
    case DemoSceneId::Fireworks: {
      for (const auto& rocket : fireworks_.rockets) {
        if (!rocket.active) {
          continue;
        }
        renderer.drawPixel(static_cast<int16_t>(rocket.x), static_cast<int16_t>(rocket.y), GrayLevels::kFull);
      }
      for (const auto& spark : fireworks_.sparks) {
        if (!spark.active) {
          continue;
        }
        const uint32_t age = nowMs - spark.bornMs;
        const uint32_t remaining = kFireworkSparkLifetimeMs > age ? kFireworkSparkLifetimeMs - age : 0UL;
        const uint8_t shade = GrayLevels::shade(remaining, kFireworkSparkLifetimeMs);
        if (shade == GrayLevels::kBlack) {
          continue;
        }
        renderer.drawPixel(static_cast<int16_t>(spark.x), static_cast<int16_t>(spark.y), shade);
      }
      break;
    }
  }
}

void DemoPage::advanceScene() {
  switch (scene_) {
    case DemoSceneId::Grayscale: scene_ = DemoSceneId::Transition; break;
    case DemoSceneId::Transition: scene_ = DemoSceneId::Tetris; break;
    case DemoSceneId::Tetris: scene_ = DemoSceneId::Checkerboard; break;
    case DemoSceneId::Checkerboard: scene_ = DemoSceneId::Marquee; break;
    case DemoSceneId::Marquee: scene_ = DemoSceneId::Grayscale; break;

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
    // case DemoSceneId::Diagnostics: scene_ = DemoSceneId::Marquee; break;
    // case DemoSceneId::Marquee: scene_ = DemoSceneId::BouncingBall; break;
    // case DemoSceneId::BouncingBall: scene_ = DemoSceneId::SparklingHearts; break;
    // case DemoSceneId::SparklingHearts: scene_ = DemoSceneId::Snake; break;
    // case DemoSceneId::Snake: scene_ = DemoSceneId::SpaceInvaders; break;
    // case DemoSceneId::SpaceInvaders: scene_ = DemoSceneId::Tetris; break;
    // case DemoSceneId::Tetris: scene_ = DemoSceneId::Pong; break;
    // case DemoSceneId::Pong: scene_ = DemoSceneId::Mario; break;
    // case DemoSceneId::Mario: scene_ = DemoSceneId::Fireworks; break;
    // case DemoSceneId::Fireworks: scene_ = DemoSceneId::Fill; break;
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
  // 41ms (~24Hz) rather than the original 140ms (~7Hz) — see the
  // MultilineText scene's comment on why update frequency is the lever
  // for motion smoothness here.
  constexpr uint32_t kStepIntervalMs = 41UL;
  if (scaleMs(nowMs - snake_.lastStepMs) < kStepIntervalMs) {
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

bool DemoPage::tetrisPlacementValid(uint8_t pieceType, uint8_t rotation, int8_t laneOffset, int16_t depthOffset) const {
  for (uint8_t i = 0; i < 4; ++i) {
    const int16_t d = static_cast<int16_t>(depthOffset - kPieceShapes[pieceType][rotation][i][0]);
    const int16_t lane = static_cast<int16_t>(laneOffset + kPieceShapes[pieceType][rotation][i][1]);
    if (d < 0 || d >= static_cast<int16_t>(TetrisState::kDepth) || lane < 0 || lane >= static_cast<int16_t>(TetrisState::kLanes)) {
      return false;
    }
    if (tetris_.board[d][lane] != 0) {
      return false;
    }
  }
  return true;
}

int16_t DemoPage::tetrisSimulateDrop(uint8_t pieceType, uint8_t rotation, int8_t laneOffset) const {
  int16_t depthOffset = static_cast<int16_t>(TetrisState::kDepth - 1);
  if (!tetrisPlacementValid(pieceType, rotation, laneOffset, depthOffset)) {
    return -1;
  }
  while (tetrisPlacementValid(pieceType, rotation, laneOffset, static_cast<int16_t>(depthOffset - 1))) {
    --depthOffset;
  }
  return depthOffset;
}

int32_t DemoPage::tetrisScorePlacement(uint8_t pieceType, uint8_t rotation, int8_t laneOffset, int16_t depthOffset) const {
  int16_t cellDepth[4];
  int16_t cellLane[4];
  for (uint8_t i = 0; i < 4; ++i) {
    cellDepth[i] = static_cast<int16_t>(depthOffset - kPieceShapes[pieceType][rotation][i][0]);
    cellLane[i] = static_cast<int16_t>(laneOffset + kPieceShapes[pieceType][rotation][i][1]);
  }

  auto occupied = [&](int16_t d, int16_t lane) {
    if (d < 0 || d >= static_cast<int16_t>(TetrisState::kDepth) || lane < 0 || lane >= static_cast<int16_t>(TetrisState::kLanes)) {
      return true;
    }
    if (tetris_.board[d][lane] != 0) {
      return true;
    }
    for (uint8_t i = 0; i < 4; ++i) {
      if (cellDepth[i] == d && cellLane[i] == lane) {
        return true;
      }
    }
    return false;
  };

  // Simple heuristic: penalize placements that stack tall (far from the
  // floor at depth 0) and placements that bury empty cells behind filled
  // ones (holes), same spirit as classic hobby Tetris bots.
  int32_t maxDepthReached = 0;
  int32_t holes = 0;
  for (int16_t lane = 0; lane < static_cast<int16_t>(TetrisState::kLanes); ++lane) {
    bool seenFilled = false;
    for (int16_t d = static_cast<int16_t>(TetrisState::kDepth - 1); d >= 0; --d) {
      if (occupied(d, lane)) {
        seenFilled = true;
        if (d > maxDepthReached) {
          maxDepthReached = d;
        }
      } else if (seenFilled) {
        ++holes;
      }
    }
  }
  return -(maxDepthReached * 2 + holes * 5);
}

void DemoPage::tetrisClearFullSlices() {
  for (int16_t d = 0; d < static_cast<int16_t>(TetrisState::kDepth); ++d) {
    bool full = true;
    for (uint8_t lane = 0; lane < TetrisState::kLanes; ++lane) {
      if (tetris_.board[d][lane] == 0) {
        full = false;
        break;
      }
    }
    if (!full) {
      continue;
    }
    for (int16_t dd = d; dd + 1 < static_cast<int16_t>(TetrisState::kDepth); ++dd) {
      for (uint8_t lane = 0; lane < TetrisState::kLanes; ++lane) {
        tetris_.board[dd][lane] = tetris_.board[dd + 1][lane];
      }
    }
    for (uint8_t lane = 0; lane < TetrisState::kLanes; ++lane) {
      tetris_.board[TetrisState::kDepth - 1][lane] = 0;
    }
    // Re-check this depth: it now holds what used to be one slice further
    // from the floor, which might itself be full.
    --d;
  }
}

void DemoPage::spawnTetrisPiece() {
  const uint8_t type = static_cast<uint8_t>(random(0, 7));
  uint8_t bestRotation = 0;
  int8_t bestLane = 0;
  int32_t bestScore = 0;
  bool found = false;

  for (uint8_t rotation = 0; rotation < 4; ++rotation) {
    for (int8_t laneOffset = -3; laneOffset < static_cast<int8_t>(TetrisState::kLanes); ++laneOffset) {
      const int16_t landing = tetrisSimulateDrop(type, rotation, laneOffset);
      if (landing < 0) {
        continue;
      }
      const int32_t score = tetrisScorePlacement(type, rotation, laneOffset, landing);
      if (!found || score > bestScore) {
        found = true;
        bestScore = score;
        bestRotation = rotation;
        bestLane = laneOffset;
      }
    }
  }

  if (!found) {
    // Jammed near the spawn edge in every orientation: reset the board
    // rather than freezing, so the demo stays alive indefinitely.
    memset(tetris_.board, 0, sizeof(tetris_.board));
    bestRotation = 0;
    bestLane = 0;
  }

  tetris_.pieceType = type;
  tetris_.pieceRotation = bestRotation;
  tetris_.pieceLane = bestLane;
  tetris_.pieceDepth = static_cast<int16_t>(TetrisState::kDepth - 1);
  tetris_.pieceShade = GrayLevels::shade(static_cast<uint32_t>(type % 4U) + 1U, 4U);
  tetris_.lastStepMs = millis();
}

void DemoPage::resetTetris() {
  memset(tetris_.board, 0, sizeof(tetris_.board));
  tetris_.lastStepMs = millis();
  spawnTetrisPiece();
}

void DemoPage::updateTetris(uint32_t nowMs) {
  // 41ms (~24Hz) rather than the original 90ms (~11Hz) — see the
  // MultilineText scene's comment on why update frequency is the lever
  // for motion smoothness here; falls proportionally faster as a result.
  constexpr uint32_t kStepMs = 41UL;
  if (scaleMs(nowMs - tetris_.lastStepMs) < kStepMs) {
    return;
  }
  tetris_.lastStepMs = nowMs;

  if (tetrisPlacementValid(tetris_.pieceType, tetris_.pieceRotation, tetris_.pieceLane, static_cast<int16_t>(tetris_.pieceDepth - 1))) {
    --tetris_.pieceDepth;
    return;
  }

  for (uint8_t i = 0; i < 4; ++i) {
    const int16_t d = static_cast<int16_t>(tetris_.pieceDepth - kPieceShapes[tetris_.pieceType][tetris_.pieceRotation][i][0]);
    const int16_t lane = static_cast<int16_t>(tetris_.pieceLane + kPieceShapes[tetris_.pieceType][tetris_.pieceRotation][i][1]);
    if (d >= 0 && d < static_cast<int16_t>(TetrisState::kDepth) && lane >= 0 && lane < static_cast<int16_t>(TetrisState::kLanes)) {
      tetris_.board[d][lane] = tetris_.pieceShade;
    }
  }
  tetrisClearFullSlices();
  spawnTetrisPiece();
}

void DemoPage::resetPongBall(float direction) {
  pong_.ballX = 40.0f;
  pong_.ballY = 10.0f;
  pong_.ballVX = 1.3f * direction;
  pong_.ballVY = (random(0, 2) == 0) ? 0.8f : -0.8f;
}

void DemoPage::resetPong() {
  pong_.leftScore = 0;
  pong_.rightScore = 0;
  pong_.leftPaddleY = 8.0f;
  pong_.rightPaddleY = 8.0f;
  pong_.lastStepMs = millis();
  resetPongBall(1.0f);
}

void DemoPage::updatePong(uint32_t nowMs) {
  constexpr uint32_t kStepMs = 35UL;
  if (scaleMs(nowMs - pong_.lastStepMs) < kStepMs) {
    return;
  }
  pong_.lastStepMs = nowMs;

  constexpr float kTop = 6.0f;
  constexpr float kBottom = 15.0f;
  constexpr float kPaddleHeight = 4.0f;
  constexpr float kLeftPaddleX = 3.0f;
  constexpr float kRightPaddleX = 76.0f;
  constexpr float kPaddleSpeed = 0.9f;

  pong_.ballX += pong_.ballVX;
  pong_.ballY += pong_.ballVY;

  if (pong_.ballY <= kTop) {
    pong_.ballY = kTop;
    pong_.ballVY = -pong_.ballVY;
  } else if (pong_.ballY >= kBottom) {
    pong_.ballY = kBottom;
    pong_.ballVY = -pong_.ballVY;
  }

  // Both paddles are AI-controlled (no input hardware exists on this
  // device): track the ball's y with a capped speed so rallies aren't
  // perfectly returned.
  auto trackPaddle = [&](float& paddleY) {
    const float target = pong_.ballY - kPaddleHeight / 2.0f;
    if (paddleY < target) {
      paddleY = min<float>(target, paddleY + kPaddleSpeed);
    } else if (paddleY > target) {
      paddleY = max<float>(target, paddleY - kPaddleSpeed);
    }
    paddleY = max<float>(kTop, min<float>(kBottom - kPaddleHeight, paddleY));
  };
  trackPaddle(pong_.leftPaddleY);
  trackPaddle(pong_.rightPaddleY);

  if (pong_.ballVX < 0.0f && pong_.ballX <= kLeftPaddleX + 2.0f && pong_.ballX >= kLeftPaddleX - 1.0f
      && pong_.ballY >= pong_.leftPaddleY - 1.0f && pong_.ballY <= pong_.leftPaddleY + kPaddleHeight + 1.0f) {
    pong_.ballVX = -pong_.ballVX;
    pong_.ballX = kLeftPaddleX + 2.0f;
  }
  if (pong_.ballVX > 0.0f && pong_.ballX >= kRightPaddleX - 2.0f && pong_.ballX <= kRightPaddleX + 1.0f
      && pong_.ballY >= pong_.rightPaddleY - 1.0f && pong_.ballY <= pong_.rightPaddleY + kPaddleHeight + 1.0f) {
    pong_.ballVX = -pong_.ballVX;
    pong_.ballX = kRightPaddleX - 2.0f;
  }

  if (pong_.ballX < 0.0f) {
    ++pong_.rightScore;
    resetPongBall(-1.0f);
  } else if (pong_.ballX > 80.0f) {
    ++pong_.leftScore;
    resetPongBall(1.0f);
  }
}

void DemoPage::resetFireworks(uint32_t nowMs) {
  for (auto& rocket : fireworks_.rockets) {
    rocket.active = false;
  }
  for (auto& spark : fireworks_.sparks) {
    spark.active = false;
  }
  fireworks_.nextLaunchMs = nowMs;
}

void DemoPage::updateFireworks(uint32_t nowMs) {
  constexpr float kGravity = 0.05f;

  for (auto& rocket : fireworks_.rockets) {
    if (!rocket.active) {
      continue;
    }
    rocket.y += rocket.vy;
    rocket.vy += kGravity;
    if (rocket.vy >= 0.0f || rocket.y <= 1.0f) {
      // Apex reached (or hit the top): burst into a share of the shared
      // spark pool.
      rocket.active = false;
      constexpr uint8_t kBurstSize = 8;
      uint8_t spawned = 0;
      for (auto& spark : fireworks_.sparks) {
        if (spawned >= kBurstSize) {
          break;
        }
        if (spark.active) {
          continue;
        }
        const float angle = static_cast<float>(random(0, 360)) * (3.14159265f / 180.0f);
        const float speed = 0.4f + static_cast<float>(random(0, 60)) / 100.0f;
        spark.active = true;
        spark.x = rocket.x;
        spark.y = rocket.y;
        spark.vx = cosf(angle) * speed;
        spark.vy = sinf(angle) * speed;
        spark.bornMs = nowMs;
        ++spawned;
      }
    }
  }

  for (auto& spark : fireworks_.sparks) {
    if (!spark.active) {
      continue;
    }
    if ((nowMs - spark.bornMs) >= kFireworkSparkLifetimeMs) {
      spark.active = false;
      continue;
    }
    spark.x += spark.vx;
    spark.y += spark.vy;
    spark.vy += kGravity * 0.5f;
    if (spark.x < 0.0f || spark.x >= 80.0f || spark.y < 0.0f || spark.y >= 16.0f) {
      spark.active = false;
    }
  }

  if (nowMs >= fireworks_.nextLaunchMs) {
    fireworks_.nextLaunchMs = nowMs + 500UL + static_cast<uint32_t>(random(0, 700));
    for (auto& rocket : fireworks_.rockets) {
      if (rocket.active) {
        continue;
      }
      rocket.active = true;
      rocket.x = static_cast<float>(random(10, 70));
      rocket.y = 15.0f;
      rocket.vy = -(1.1f + static_cast<float>(random(0, 40)) / 100.0f);
      break;
    }
  }
}
