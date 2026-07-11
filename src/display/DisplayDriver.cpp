#include "display/DisplayDriver.h"

#include <SPI.h>
#include <array>
#include <cstring>

#include <driver/gpio.h>

#include "display/FrameBuffer4.h"
#include "display/GrayLevels.h"

namespace {
constexpr uint8_t kPlaneCount = 4;
constexpr uint16_t kBasePlaneQuantumUs = 24;
constexpr uint8_t kBamSliceCount = 15;
constexpr uint8_t kBamPlaneSchedule[kBamSliceCount] = {
  3, 2, 3, 1,
  3, 2, 3, 0,
  3, 2, 3, 1,
  3, 2, 3,
};

// Expands GrayLevels::kLevels (the small, hand-written canonical palette)
// into a direct-index table covering all 16 raw 4-bit pixel values, so the
// per-pixel hot path in buildScanPlanes() stays an O(1) array lookup. Built
// at compile time from GrayLevels::kLevels alone via GrayLevels::nearest()
// (the same nearest-match logic pages use for procedural shading), so
// growing or shrinking the palette there is sufficient, nothing here needs
// to change. By construction every canonical value maps to itself (its own
// distance is always the unique minimum of 0), so the palette values are
// always stable fixed points of this table.
constexpr std::array<uint8_t, 16> buildGrayLevelLut() {
  std::array<uint8_t, 16> table{};
  for (uint16_t raw = 0; raw < 16; ++raw) {
    table[raw] = GrayLevels::nearest(static_cast<uint8_t>(raw));
  }
  return table;
}

constexpr std::array<uint8_t, 16> kGrayLevelLut = buildGrayLevelLut();

uint8_t quantizeGrayLevel(uint8_t level) {
  return kGrayLevelLut[level];
}
}

DisplayDriver::~DisplayDriver() {
  delete[] scanBuffers_[0];
  delete[] scanBuffers_[1];
  delete[] transferRowBuffer_;
}

bool DisplayDriver::begin(const DisplayConfig& config) {
  config_ = config;

  rowBytes_ = (static_cast<size_t>(config_.width) + 7U) / 8U;
  planeBytes_ = rowBytes_ * static_cast<size_t>(config_.height);
  scanBufferBytes_ = planeBytes_ * kPlaneCount;

  delete[] scanBuffers_[0];
  delete[] scanBuffers_[1];
  delete[] transferRowBuffer_;
  scanBuffers_[0] = new uint8_t[scanBufferBytes_];
  scanBuffers_[1] = new uint8_t[scanBufferBytes_];
  transferRowBuffer_ = new uint8_t[rowBytes_];
  if (scanBuffers_[0] == nullptr || scanBuffers_[1] == nullptr || transferRowBuffer_ == nullptr) {
    return false;
  }
  memset(scanBuffers_[0], 0, scanBufferBytes_);
  memset(scanBuffers_[1], 0, scanBufferBytes_);
  memset(transferRowBuffer_, 0, rowBytes_);

  pinMode(config_.pinLatch, OUTPUT);
  pinMode(config_.pinEnable, OUTPUT);
  for (uint8_t index = 0; index < config_.rowAddressBits && index < 4; ++index) {
    pinMode(config_.pinRow[index], OUTPUT);
  }

  SPI.begin(config_.pinClk, -1, config_.pinMosi, -1);
  // Held open for the driver's entire lifetime: serviceScan() calls
  // SPI.transfer() up to ~40,000 times/sec (15 BAM slices x 16 rows per
  // refresh), and re-doing SPI.beginTransaction()/endTransaction() on every
  // one of those was measurable dead time subtracted from the fixed
  // row-multiplexing duty budget (see DisplayDriver.h). Safe only because
  // nothing else in this codebase shares this SPI peripheral instance.
  SPI.beginTransaction(SPISettings(config_.spiHz, MSBFIRST, SPI_MODE0));
  blankOutput();
  digitalWrite(config_.pinLatch, HIGH);

  activeScanBufferIndex_ = 0;
  pendingScanBufferIndex_ = -1;
  currentRow_ = 0;
  currentSliceIndex_ = 0;
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
  int8_t stagedBufferIndex = 0;
  portENTER_CRITICAL(&scanMux_);
  stagedBufferIndex = activeScanBufferIndex_ == 0 ? 1 : 0;
  portEXIT_CRITICAL(&scanMux_);

  buildScanPlanes(frame, scanBuffers_[stagedBufferIndex]);

  portENTER_CRITICAL(&scanMux_);
  if (pendingScanBufferIndex_ >= 0) {
    ++stats_.droppedPresents;
  }
  pendingScanBufferIndex_ = stagedBufferIndex;
  presentQueuedAtUs_ = micros();
  portEXIT_CRITICAL(&scanMux_);
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

  if (currentRow_ == 0 && currentSliceIndex_ == 0) {
    portENTER_CRITICAL(&scanMux_);
    if (pendingScanBufferIndex_ >= 0) {
      activeScanBufferIndex_ = pendingScanBufferIndex_;
      pendingScanBufferIndex_ = -1;
      stats_.publishLatencyUs = nowMicros - presentQueuedAtUs_;
      stats_.frameLatencyUs = stats_.publishLatencyUs;
    }
    portEXIT_CRITICAL(&scanMux_);

    ++refreshFramesThisWindow_;
    const uint32_t nowMs = millis();
    if ((nowMs - lastRefreshWindowStartedMs_) >= 1000UL) {
      stats_.refreshHz = refreshFramesThisWindow_;
      refreshFramesThisWindow_ = 0;
      lastRefreshWindowStartedMs_ = nowMs;
    }
  }

  setRowAddress(currentRow_);

  gpio_set_level(static_cast<gpio_num_t>(config_.pinLatch), 0);
  const uint8_t planeIndex = kBamPlaneSchedule[currentSliceIndex_];
  const size_t rowOffset = static_cast<size_t>(planeIndex) * planeBytes_ + static_cast<size_t>(currentRow_) * rowBytes_;
  memcpy(transferRowBuffer_, scanBuffers_[activeScanBufferIndex_] + rowOffset, rowBytes_);
  SPI.transfer(transferRowBuffer_, rowBytes_);
  latchRow();

  enableOutput();
  currentSlotStartedUs_ = nowMicros;

  currentSlotDurationUs_ = max<uint32_t>(1UL, (static_cast<uint32_t>(kBasePlaneQuantumUs) * max<uint8_t>(brightness_, 1U)) / 255UL);
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
  gpio_set_level(static_cast<gpio_num_t>(config_.pinEnable), config_.enableActiveLow ? 1 : 0);
}

void DisplayDriver::enableOutput() {
  gpio_set_level(static_cast<gpio_num_t>(config_.pinEnable), config_.enableActiveLow ? 0 : 1);
}

void DisplayDriver::setRowAddress(uint8_t row) {
  for (uint8_t bit = 0; bit < config_.rowAddressBits && bit < 4; ++bit) {
    gpio_set_level(static_cast<gpio_num_t>(config_.pinRow[bit]), (row >> bit) & 0x01U);
  }
}

void DisplayDriver::latchRow() {
  gpio_set_level(static_cast<gpio_num_t>(config_.pinLatch), 1);
  gpio_set_level(static_cast<gpio_num_t>(config_.pinLatch), 0);
}

void DisplayDriver::buildScanPlanes(const FrameBuffer4& frame, uint8_t* destination) {
  memset(destination, 0, scanBufferBytes_);
  for (uint16_t y = 0; y < config_.height; ++y) {
    for (uint16_t x = 0; x < config_.width; ++x) {
      const uint8_t level = quantizeGrayLevel(frame.getPixel(x, y) & 0x0FU);
      if (level == 0U) {
        continue;
      }

      const size_t byteIndex = x / 8U;
      const uint8_t bitMask = static_cast<uint8_t>(0x80U >> (x % 8U));
      for (uint8_t planeIndex = 0; planeIndex < kPlaneCount; ++planeIndex) {
        if ((level & (1U << planeIndex)) == 0U) {
          continue;
        }

        uint8_t* planeRowBytes = destination
          + static_cast<size_t>(planeIndex) * planeBytes_
          + static_cast<size_t>(y) * rowBytes_;
        planeRowBytes[byteIndex] = static_cast<uint8_t>(planeRowBytes[byteIndex] | bitMask);
      }
    }
  }
}

void DisplayDriver::advanceScanPosition() {
  ++currentRow_;
  if (currentRow_ >= config_.height) {
    currentRow_ = 0;
    currentSliceIndex_ = static_cast<uint8_t>((currentSliceIndex_ + 1U) % kBamSliceCount);
  }
}
