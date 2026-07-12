#pragma once

#include <cstddef>
#include <cstdint>

// Canonical physical brightness levels the display hardware can actually
// render (see DisplayDriver's buildScanPlanes()/quantizeGrayLevel()). Any
// raw 0-15 value written into a FrameBuffer4 gets snapped to the nearest of
// these at present() time, so drawing with one of these exact values is the
// only reliable way to hit a specific physical brightness — see
// DisplayDriver.cpp for why non-canonical values are not safe to assume are
// stable.
//
// This is the single source of truth for the palette size: DisplayDriver
// derives its scan-plane quantization table from it, and pages (e.g.
// DemoPage's Grayscale scene) derive band counts from it, so changing the
// number/values of levels here is sufficient on its own.
namespace GrayLevels {
constexpr uint8_t kLevels[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};
// constexpr uint8_t kLevels[] = {0, 1, 2, 5, 15};
// constexpr uint8_t kLevels[] = {0, 1, 4, 9,15};
constexpr size_t kCount = sizeof(kLevels) / sizeof(kLevels[0]);
constexpr uint8_t kBlack = kLevels[0];
constexpr uint8_t kFull = kLevels[kCount - 1];
// Levels strictly between black and full — the "dim" shades. Effects that
// want to enumerate distinct mid-tones (gradients, fades) without hardcoding
// how many exist should size against this.
constexpr size_t kDimCount = kCount > 2 ? kCount - 2 : 0;

constexpr uint8_t distance(uint8_t a, uint8_t b) {
  return a > b ? static_cast<uint8_t>(a - b) : static_cast<uint8_t>(b - a);
}

// Snaps an arbitrary 0-15 value to the nearest level actually present in the
// palette. This is the single mechanism both DisplayDriver's scan-plane
// quantization and any effect wanting a procedural shade (gradients, fades,
// growth animations) should go through, so behavior automatically tracks
// whatever palette is configured above instead of hardcoding level indices.
constexpr uint8_t nearest(uint8_t raw) {
  uint8_t best = kLevels[0];
  uint8_t bestDist = distance(raw, best);
  for (size_t i = 1; i < kCount; ++i) {
    const uint8_t candidate = kLevels[i];
    const uint8_t dist = distance(raw, candidate);
    if (dist < bestDist) {
      bestDist = dist;
      best = candidate;
    }
  }
  return best;
}

// Nearest palette level to a brightness fraction expressed as
// numerator/denominator of full scale, e.g. shade(3, 4) ~= 75% bright.
// Effects use this to fade/grow through whatever palette is configured
// without knowing how many levels it has.
inline uint8_t shade(uint32_t numerator, uint32_t denominator) {
  if (denominator == 0U) {
    return kBlack;
  }
  uint32_t raw = (15U * numerator) / denominator;
  if (raw > 15U) {
    raw = 15U;
  }
  return nearest(static_cast<uint8_t>(raw));
}
}
