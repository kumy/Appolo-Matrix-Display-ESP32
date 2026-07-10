#include "display/DisplayDriver.h"

#include <SPI.h>
#include <cstring>

#include "display/FrameBuffer4.h"

namespace {
constexpr uint8_t kPlaneCount = 4;
constexpr uint16_t kBasePlaneQuantumUs = 50;
}

DisplayDriver::~DisplayDriver() {
  delete[] scanBuffers_[0];
  delete[] scanBuffers_[1];
}

bool DisplayDriver::begin(const DisplayConfig& config) {
  config_ = config;

  rowBytes_ = (static_cast<size_t>(config_.width) + 7U) / 8U;
  planeBytes_ = rowBytes_ * static_cast<size_t>(config_.height);
  scanBufferBytes_ = planeBytes_ * kPlaneCount;

  delete[] scanBuffers_[0];
  delete[] scanBuffers_[1];
  scanBuffers_[0] = new uint8_t[scanBufferBytes_];
  scanBuffers_[1] = new uint8_t[scanBufferBytes_];
  if (scanBuffers_[0] == nullptr || scanBuffers_[1] == nullptr) {
    return false;
  }
  memset(scanBuffers_[0], 0, scanBufferBytes_);
  memset(scanBuffers_[1], 0, scanBufferBytes_);

  pinMode(config_.pinLatch, OUTPUT);
  pinMode(config_.pinEnable, OUTPUT);
  for (uint8_t index = 0; index < config_.rowAddressBits && index < 4; ++index) {
    pinMode(config_.pinRow[index], OUTPUT);
  }

  SPI.begin(config_.pinClk, -1, config_.pinMosi, -1);
  blankOutput();
  digitalWrite(config_.pinLatch, HIGH);

  activeScanBufferIndex_ = 0;
  pendingScanBufferIndex_ = -1;
  currentRow_ = 0;
  currentPlane_ = 0;
  currentSlotStartedUs_ = micros();
  currentSlotDurationUs_ = kBasePlaneQuantumUs;
  lastRefreshWindowStartedMs_ = millis();
  return true;
}

void DisplayDriver::setPower(bool enabled) {
  powerEnabled_ = enabled;
  if (!powerEnabled_) {
    blankOutput();
  }
}

void DisplayDriver::setBrightness(uint8_t brightness) {
  brightness_ = brightness;
}

void DisplayDriver::present(const FrameBuffer4& frame) {
  const int8_t stagedBufferIndex = activeScanBufferIndex_ == 0 ? 1 : 0;
  buildScanPlanes(frame, scanBuffers_[stagedBufferIndex]);
  if (pendingScanBufferIndex_ >= 0) {
    ++stats_.droppedPresents;
  }
  pendingScanBufferIndex_ = stagedBufferIndex;
  presentQueuedAtUs_ = micros();
}

void DisplayDriver::serviceScan(uint32_t nowMicros) {
  if (!powerEnabled_) {
    blankOutput();
    return;
  }

  if ((nowMicros - currentSlotStartedUs_) > (currentSlotDurationUs_ * 2UL)) {
    ++stats_.scanUnderruns;
  }

  if ((nowMicros - currentSlotStartedUs_) < currentSlotDurationUs_) {
    return;
  }

  blankOutput();

  if (currentRow_ == 0 && currentPlane_ == 0) {
    if (pendingScanBufferIndex_ >= 0) {
      activeScanBufferIndex_ = pendingScanBufferIndex_;
      pendingScanBufferIndex_ = -1;
      stats_.publishLatencyUs = nowMicros - presentQueuedAtUs_;
      stats_.frameLatencyUs = stats_.publishLatencyUs;
    }
    ++refreshFramesThisWindow_;
    const uint32_t nowMs = millis();
    if ((nowMs - lastRefreshWindowStartedMs_) >= 1000UL) {
      stats_.refreshHz = refreshFramesThisWindow_;
      refreshFramesThisWindow_ = 0;
      lastRefreshWindowStartedMs_ = nowMs;
    }
  }

  setRowAddress(currentRow_);

  SPI.beginTransaction(SPISettings(config_.spiHz, MSBFIRST, SPI_MODE0));
  digitalWrite(config_.pinLatch, LOW);
  const size_t rowOffset = static_cast<size_t>(currentPlane_) * planeBytes_ + static_cast<size_t>(currentRow_) * rowBytes_;
  SPI.transfer(scanBuffers_[activeScanBufferIndex_] + rowOffset, rowBytes_);
  latchRow();
  SPI.endTransaction();

  enableOutput();
  currentSlotStartedUs_ = nowMicros;

  const uint32_t weightedDuration = kBasePlaneQuantumUs << currentPlane_;
  currentSlotDurationUs_ = max<uint32_t>(1UL, (weightedDuration * max<uint8_t>(brightness_, 1U)) / 255UL);
  if (brightness_ == 0) {
    currentSlotDurationUs_ = 1;
  }

  advanceScanPosition();
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

void DisplayDriver::blankOutput() {
  digitalWrite(config_.pinEnable, config_.enableActiveLow ? HIGH : LOW);
}

void DisplayDriver::enableOutput() {
  digitalWrite(config_.pinEnable, config_.enableActiveLow ? LOW : HIGH);
}

void DisplayDriver::setRowAddress(uint8_t row) {
  for (uint8_t bit = 0; bit < config_.rowAddressBits && bit < 4; ++bit) {
    digitalWrite(config_.pinRow[bit], (row >> bit) & 0x01U);
  }
}

void DisplayDriver::latchRow() {
  digitalWrite(config_.pinLatch, HIGH);
  digitalWrite(config_.pinLatch, LOW);
}

void DisplayDriver::buildScanPlanes(const FrameBuffer4& frame, uint8_t* destination) {
  memset(destination, 0, scanBufferBytes_);
  for (uint8_t plane = 0; plane < kPlaneCount; ++plane) {
    const size_t planeOffset = static_cast<size_t>(plane) * planeBytes_;
    for (uint16_t y = 0; y < config_.height; ++y) {
      uint8_t* rowBytes = destination + planeOffset + static_cast<size_t>(y) * rowBytes_;
      for (uint16_t x = 0; x < config_.width; ++x) {
        const uint8_t gray = frame.getPixel(x, y);
        if (((gray >> plane) & 0x01U) == 0U) {
          continue;
        }
        const size_t byteIndex = x / 8U;
        const uint8_t bitMask = static_cast<uint8_t>(0x80U >> (x % 8U));
        rowBytes[byteIndex] = static_cast<uint8_t>(rowBytes[byteIndex] | bitMask);
      }
    }
  }
}

void DisplayDriver::advanceScanPosition() {
  ++currentRow_;
  if (currentRow_ >= config_.height) {
    currentRow_ = 0;
    currentPlane_ = static_cast<uint8_t>((currentPlane_ + 1U) % kPlaneCount);
  }
}
