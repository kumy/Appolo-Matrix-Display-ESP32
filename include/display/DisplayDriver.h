#pragma once

#include <Arduino.h>

#include "core/RuntimeStats.h"
#include "display/DisplayConfig.h"

class FrameBuffer4;

class DisplayDriver {
public:
  bool begin(const DisplayConfig& config);
  void setPower(bool enabled);
  void setBrightness(uint8_t brightness);
  void present(const FrameBuffer4& frame);
  void serviceScan(uint32_t nowMicros);
  const RuntimeStats& stats() const;
  bool powerEnabled() const;
  uint8_t brightness() const;

private:
  DisplayConfig config_;
  const FrameBuffer4* activeFrame_ = nullptr;
  const FrameBuffer4* pendingFrame_ = nullptr;
  uint32_t presentQueuedAtUs_ = 0;
  uint32_t lastBoundaryUs_ = 0;
  uint32_t lastRefreshTickUs_ = 0;
  uint32_t simulatedFramePeriodUs_ = 16000;
  uint32_t frameCounter_ = 0;
  bool powerEnabled_ = true;
  uint8_t brightness_ = 255;
  RuntimeStats stats_;
};
