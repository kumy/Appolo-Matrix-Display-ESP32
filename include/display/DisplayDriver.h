#pragma once

#include <Arduino.h>

#include <cstddef>

#include "core/RuntimeStats.h"
#include "display/DisplayConfig.h"

class FrameBuffer4;

class DisplayDriver {
public:
  ~DisplayDriver();
  bool begin(const DisplayConfig& config);
  void setPower(bool enabled);
  void setBrightness(uint8_t brightness);
  void present(const FrameBuffer4& frame);
  void serviceScan(uint32_t nowMicros);
  const RuntimeStats& stats() const;
  bool powerEnabled() const;
  uint8_t brightness() const;

private:
  void blankOutput();
  void enableOutput();
  void setRowAddress(uint8_t row);
  void latchRow();
  void buildScanPlanes(const FrameBuffer4& frame, uint8_t* destination);
  void advanceScanPosition();

  DisplayConfig config_;
  uint8_t* scanBuffers_[2] = {nullptr, nullptr};
  size_t rowBytes_ = 0;
  size_t planeBytes_ = 0;
  size_t scanBufferBytes_ = 0;
  int8_t activeScanBufferIndex_ = 0;
  int8_t pendingScanBufferIndex_ = -1;
  uint32_t presentQueuedAtUs_ = 0;
  uint32_t lastRefreshWindowStartedMs_ = 0;
  uint32_t currentSlotStartedUs_ = 0;
  uint32_t currentSlotDurationUs_ = 100;
  uint32_t refreshFramesThisWindow_ = 0;
  uint8_t currentRow_ = 0;
  uint8_t currentPlane_ = 0;
  bool powerEnabled_ = true;
  uint8_t brightness_ = 255;
  RuntimeStats stats_;
};
