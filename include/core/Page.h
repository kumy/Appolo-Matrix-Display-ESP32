#pragma once

#include <Arduino.h>

class Renderer;
struct Event;

class Page {
public:
  virtual ~Page() = default;
  virtual void enter() {}
  virtual void leave() {}
  virtual void handleEvent(const Event&) {}
  virtual void update(uint32_t nowMs) = 0;
  virtual void draw(Renderer& renderer) = 0;
};
