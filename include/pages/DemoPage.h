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
  Tetris,
  Pong,
  Mario,
  Fireworks,
};

class DemoPage : public Page {
public:
  DemoPage(ClockService& clock, RuntimeStats& stats);

  void enter() override;
  void update(uint32_t nowMs) override;
  void draw(Renderer& renderer) override;

  // Live-adjustable multiplier on scene animation timing (100 = normal
  // speed, 200 = 2x, 50 = half). Applied to the elapsed-time values used
  // throughout draw() (continuous motion: pingPong/modulo-cycle scenes)
  // and to the step-gate checks in updateSnake()/updateTetris()/
  // updatePong() (discrete step timers) — see their call sites for how.
  void setAnimationSpeedPercent(uint16_t percent);

private:
  uint32_t scaleMs(uint32_t ms) const;


  void advanceScene();
  uint32_t sceneDurationMs() const;
  void resetSceneState(uint32_t nowMs);

  void updateSnake(uint32_t nowMs);
  void resetSnake();
  void spawnSnakeFood();
  bool snakeBodyContains(uint8_t x, uint8_t y, uint8_t count) const;

  void updateHearts(uint32_t nowMs);

  void updateTetris(uint32_t nowMs);
  void resetTetris();
  void spawnTetrisPiece();
  bool tetrisPlacementValid(uint8_t pieceType, uint8_t rotation, int8_t laneOffset, int16_t depthOffset) const;
  int16_t tetrisSimulateDrop(uint8_t pieceType, uint8_t rotation, int8_t laneOffset) const;
  int32_t tetrisScorePlacement(uint8_t pieceType, uint8_t rotation, int8_t laneOffset, int16_t depthOffset) const;
  void tetrisClearFullSlices();

  void updatePong(uint32_t nowMs);
  void resetPong();
  void resetPongBall(float direction);

  void updateFireworks(uint32_t nowMs);
  void resetFireworks(uint32_t nowMs);

  ClockService& clock_;
  RuntimeStats& stats_;
  DemoSceneId scene_ = DemoSceneId::Grayscale;
  uint32_t sceneStartedAtMs_ = 0;
  uint16_t animationSpeedPercent_ = 100;

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

  // Autonomous Tetris, rotated 90°: pieces spawn at the right edge (the
  // "depth" axis, kDepth-1) and fall toward the left edge (depth 0), which
  // is the floor. "Lanes" (kLanes) is the axis a piece can be offset along,
  // corresponding to classic Tetris's horizontal position. A completed
  // "depth slice" (all lanes filled at one depth) clears like a classic row.
  // No lateral movement during the fall: a simple heuristic AI picks the
  // best (rotation, laneOffset) for each piece at spawn time (minimizing
  // resulting stack height and holes) and it falls straight into place —
  // this is a deliberate simplification for an unattended attract-mode
  // game, not a limitation of the board representation.
  struct TetrisState {
    static constexpr uint8_t kDepth = 40;  // 80px / kCellSize
    static constexpr uint8_t kLanes = 8;   // 16px / kCellSize
    static constexpr uint8_t kCellSize = 2;

    uint8_t board[kDepth][kLanes] = {};  // 0 = empty, else the locked piece's shade
    uint8_t pieceType = 0;
    uint8_t pieceRotation = 0;
    int8_t pieceLane = 0;
    uint8_t pieceShade = 0;
    int16_t pieceDepth = 0;
    uint32_t lastStepMs = 0;
  } tetris_;

  // Live Pong: two paddles, both AI-controlled (no input hardware exists),
  // tracking the ball's y with a capped speed so rallies aren't perfectly
  // returned. Score is genuine running state.
  struct PongState {
    float ballX = 40.0f;
    float ballY = 10.0f;
    float ballVX = 1.3f;
    float ballVY = 0.8f;
    float leftPaddleY = 8.0f;
    float rightPaddleY = 8.0f;
    uint8_t leftScore = 0;
    uint8_t rightScore = 0;
    uint32_t lastStepMs = 0;
  } pong_;

  // Live fireworks: rockets launch from the bottom, arc under gravity, and
  // burst into a shared pool of fading sparks at their apex.
  struct FireworkState {
    struct Rocket {
      bool active = false;
      float x = 0.0f;
      float y = 15.0f;
      float vy = 0.0f;
    };
    struct Spark {
      bool active = false;
      float x = 0.0f;
      float y = 0.0f;
      float vx = 0.0f;
      float vy = 0.0f;
      uint32_t bornMs = 0;
    };
    static constexpr uint8_t kRocketCount = 2;
    static constexpr uint8_t kSparkCount = 24;
    Rocket rockets[kRocketCount];
    Spark sparks[kSparkCount];
    uint32_t nextLaunchMs = 0;
  } fireworks_;
};
