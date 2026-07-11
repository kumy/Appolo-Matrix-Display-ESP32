#pragma once

#include <Arduino.h>

#include "core/Page.h"
#include "core/RuntimeStats.h"

class ClockService;
class Renderer;

enum class DemoSceneId {
  Fill,
  Grayscale,
  Primitives,
  Text,
  MultilineText,
  Clock,
  Bitmap,
  Transition,
  Checkerboard,
  EdgeStress,
  Diagnostics,
  Marquee,
  BouncingBall,
  SparklingHearts,
  Snake,
  SpaceInvaders,
};

class DemoPage : public Page {
public:
  DemoPage(ClockService& clock, RuntimeStats& stats);

  void enter() override;
  void update(uint32_t nowMs) override;
  void draw(Renderer& renderer) override;

private:
  void advanceScene();
  uint32_t sceneDurationMs() const;
  void resetSceneState(uint32_t nowMs);

  void updateSnake(uint32_t nowMs);
  void resetSnake();
  void spawnSnakeFood();
  bool snakeBodyContains(uint8_t x, uint8_t y, uint8_t count) const;

  void updateHearts(uint32_t nowMs);

  ClockService& clock_;
  RuntimeStats& stats_;
  DemoSceneId scene_ = DemoSceneId::Transition;
  uint32_t sceneStartedAtMs_ = 0;

  // Autonomous snake: no input hardware exists on this device, so it plays
  // itself (greedy pathing toward food, self-collision avoidance, respawns
  // if it ever fully boxes itself in). Grid cells are kCellSize screen
  // pixels square.
  struct SnakeState {
    static constexpr uint8_t kGridCols = 40;  // 80px / kCellSize
    static constexpr uint8_t kGridRows = 8;   // 16px / kCellSize
    static constexpr uint8_t kCellSize = 2;
    static constexpr uint8_t kMaxLength = 32;

    uint8_t bodyX[kMaxLength] = {0};
    uint8_t bodyY[kMaxLength] = {0};
    uint8_t length = 3;
    int8_t dirX = 1;
    int8_t dirY = 0;
    uint8_t foodX = 0;
    uint8_t foodY = 0;
    uint32_t lastStepMs = 0;
  } snake_;

  struct HeartSlot {
    bool active = false;
    int16_t x = 0;
    int16_t y = 0;
    uint32_t spawnMs = 0;
  };
  static constexpr uint8_t kHeartSlotCount = 3;
  HeartSlot hearts_[kHeartSlotCount];
  uint32_t heartsNextSpawnCheckMs_ = 0;
};
