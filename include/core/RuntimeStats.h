#pragma once

#include <Arduino.h>

struct RuntimeStats {
  uint32_t appFps = 0;
  uint32_t refreshHz = 0;
  uint32_t renderLatencyUs = 0;
  uint32_t frameLatencyUs = 0;
  uint32_t publishLatencyUs = 0;
  uint32_t frameTimeJitterUs = 0;
  uint32_t droppedPresents = 0;
  uint32_t scanUnderruns = 0;
  uint32_t droppedEvents = 0;
};
