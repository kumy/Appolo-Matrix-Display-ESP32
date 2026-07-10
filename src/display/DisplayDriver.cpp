#include "display/DisplayDriver.h"

#include "display/FrameBuffer4.h"

bool DisplayDriver::begin(const DisplayConfig& config) {
  config_ = config;
  lastBoundaryUs_ = micros();
  lastRefreshTickUs_ = lastBoundaryUs_;
  return true;
}

void DisplayDriver::setPower(bool enabled) {
  powerEnabled_ = enabled;
}

void DisplayDriver::setBrightness(uint8_t brightness) {
  brightness_ = brightness;
}

void DisplayDriver::present(const FrameBuffer4& frame) {
  pendingFrame_ = &frame;
  presentQueuedAtUs_ = micros();
}

void DisplayDriver::serviceScan(uint32_t nowMicros) {
  const uint32_t elapsed = nowMicros - lastRefreshTickUs_;
  if (elapsed > 0) {
    stats_.refreshHz = 1000000UL / elapsed;
  }
  lastRefreshTickUs_ = nowMicros;

  if ((nowMicros - lastBoundaryUs_) >= simulatedFramePeriodUs_) {
    lastBoundaryUs_ = nowMicros;
    ++frameCounter_;
    if (pendingFrame_ != nullptr) {
      activeFrame_ = pendingFrame_;
      pendingFrame_ = nullptr;
      stats_.publishLatencyUs = nowMicros - presentQueuedAtUs_;
      stats_.frameLatencyUs = stats_.publishLatencyUs;
    }
  }

  if (!powerEnabled_) {
    activeFrame_ = nullptr;
  }
}

const RuntimeStats& DisplayDriver::stats() const {
  return stats_;
}

bool DisplayDriver::powerEnabled() const {
  return powerEnabled_;
}

uint8_t DisplayDriver::brightness() const {
  return brightness_;
}
