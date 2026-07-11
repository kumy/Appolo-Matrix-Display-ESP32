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
};

class DemoPage : public Page {
public:
  DemoPage(ClockService& clock, RuntimeStats& stats);

  void enter() override;
  void update(uint32_t nowMs) override;
  void draw(Renderer& renderer) override;

private:
  void advanceScene();

  ClockService& clock_;
  RuntimeStats& stats_;
  DemoSceneId scene_ = DemoSceneId::Transition;
  uint32_t sceneStartedAtMs_ = 0;
};
