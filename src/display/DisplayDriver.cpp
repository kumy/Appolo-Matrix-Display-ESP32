#include "display/DisplayDriver.h"

#include <SPI.h>
#include <array>
#include <cmath>
#include <cstring>

#include <driver/gpio.h>

#include "display/FrameBuffer4.h"
#include "display/GrayLevels.h"
#include "util/Logger.h"

namespace {
constexpr uint8_t kPlaneCount = 4;
// Two earlier attempts at the raw ESP-IDF spi_master transaction API
// (queued+polled, then blocking spi_device_transmit()) both carried
// ~40-85us of fixed per-transaction overhead for this transfer size
// (~10 bytes/row) — many times the ~4-5us the wire transfer itself takes
// at 20MHz, and enough on its own to force a large kBasePlaneQuantumUs.
// Arduino's simpler SPIClass::transfer() (used here, matching the
// original pre-DMA-experiment design) doesn't carry that transaction-API
// overhead for small transfers. Frame computation now runs on its own
// core-0 task (see Application.h), so core 1 is dedicated entirely to
// this scan task — there's no other task left on this core that a too-low
// value could starve, unlike when kBasePlaneQuantumUs had to carry a
// large safety margin to protect a competing loop task (see git history /
// DisplayDriver.h's class comment for that saga).
//
// Measured on hardware, a single step costs ~20-50us (SPI.transfer() +
// gptimer_get_raw_count()/gptimer_set_alarm_action() + GPIO work). These
// constants sit comfortably above that so requested durations at
// different brightness levels produce genuinely different real on-times
// — see setBrightness() for why linear scaling between them still wasn't
// enough (gamma correction) on top of this floor being real.
// kMinSlotDurationUs=55 is the real achievable floor (measured per-step
// cost). kBasePlaneQuantumUs=120 first tried here only gave a 55:120
// (~46%) floor:base duty-cycle ratio — not narrow enough: even 46% duty
// still read as visually "full" to the eye (confirmed live: 25/50/100 all
// looked like 255). 190 widens that to ~29%, still within the refresh
// rate (240 slots x 190us =~ 22Hz) already confirmed flicker-free live at
// this exact value, so this isn't spending any new risk budget.
constexpr uint16_t kBasePlaneQuantumUs = 190;
constexpr uint16_t kMinSlotDurationUs = 55;
// Human brightness perception is roughly logarithmic, not linear with
// duty cycle — a linear PWM mapping spends most of its input range in the
// perceptually-saturated "looks full" zone. Gamma correction concentrates
// the visible dimming into the range that actually looks dim. Applied as
// an interpolation FRACTION across [kMinSlotDurationUs,
// kBasePlaneQuantumUs] in setBrightness(), not as a multiplier against
// kBasePlaneQuantumUs alone — the latter was the earlier bug: a
// gamma-shrunk value still under the real floor just clamps straight back
// to kMinSlotDurationUs, silently discarding the gamma shaping for most
// of the brightness range. 2.5 is a common starting point for LED PWM
// (sRGB-style displays typically use ~2.2).
constexpr float kBrightnessGamma = 2.5f;
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
  if (scanTaskHandle_ != nullptr) {
    vTaskDelete(scanTaskHandle_);
  }
  if (gpTimer_ != nullptr) {
    gptimer_stop(gpTimer_);
    gptimer_disable(gpTimer_);
    gptimer_del_timer(gpTimer_);
  }
  delete[] transferRowBuffer_;
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
  delete[] transferRowBuffer_;
  scanBuffers_[0] = new uint8_t[scanBufferBytes_];
  scanBuffers_[1] = new uint8_t[scanBufferBytes_];
  transferRowBuffer_ = new uint8_t[rowBytes_];
  if (scanBuffers_[0] == nullptr || scanBuffers_[1] == nullptr || transferRowBuffer_ == nullptr) {
    Logger::info(String("DisplayDriver: buffer allocation failed"));
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
  // Held open for the driver's entire lifetime: transferCurrentRow() calls
  // SPI.transfer() up to ~40,000 times/sec (15 BAM slices x 16 rows per
  // refresh), and re-doing SPI.beginTransaction()/endTransaction() on every
  // one of those was measurable dead time subtracted from the fixed
  // row-multiplexing duty budget. Safe only because nothing else in this
  // codebase shares this SPI peripheral instance.
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

  if (scanTaskHandle_ != nullptr) {
    vTaskDelete(scanTaskHandle_);
    scanTaskHandle_ = nullptr;
  }
  if (gpTimer_ != nullptr) {
    gptimer_stop(gpTimer_);
    gptimer_disable(gpTimer_);
    gptimer_del_timer(gpTimer_);
    gpTimer_ = nullptr;
  }

  // core=1: away from WiFi/lwIP (core 0). priority=2: one above the
  // default Arduino loop task (1), so this task always preempts it
  // promptly when woken instead of time-slicing against it — safe (unlike
  // the original busy-loop task this replaced, which caused exactly this
  // kind of starvation when raised above the loop task's priority) only
  // because this task actually blocks on ulTaskNotifyTake() between
  // firings instead of ever being continuously ready.
  if (xTaskCreatePinnedToCore(&DisplayDriver::scanTaskEntry, "display_scan", 4096, this, 2, &scanTaskHandle_, 1) != pdPASS) {
    Logger::info(String("DisplayDriver: xTaskCreatePinnedToCore failed"));
    return false;
  }

  const gptimer_config_t timerConfig = {
    .clk_src = GPTIMER_CLK_SRC_DEFAULT,
    .direction = GPTIMER_COUNT_UP,
    .resolution_hz = 1000000,  // 1 tick = 1us, matching currentSlotDurationUs_'s unit
    .intr_priority = 0,
    .flags = {},
  };
  esp_err_t err = gptimer_new_timer(&timerConfig, &gpTimer_);
  if (err != ESP_OK) {
    Logger::info(String("DisplayDriver: gptimer_new_timer failed, err=") + String(static_cast<int>(err)));
    return false;
  }
  const gptimer_event_callbacks_t callbacks = {
    .on_alarm = &DisplayDriver::onAlarm,
  };
  err = gptimer_register_event_callbacks(gpTimer_, &callbacks, this);
  if (err != ESP_OK) {
    Logger::info(String("DisplayDriver: gptimer_register_event_callbacks failed, err=") + String(static_cast<int>(err)));
    return false;
  }
  err = gptimer_enable(gpTimer_);
  if (err != ESP_OK) {
    Logger::info(String("DisplayDriver: gptimer_enable failed, err=") + String(static_cast<int>(err)));
    return false;
  }
  err = gptimer_start(gpTimer_);
  if (err != ESP_OK) {
    Logger::info(String("DisplayDriver: gptimer_start failed, err=") + String(static_cast<int>(err)));
    return false;
  }
  rearmTimer(currentSlotDurationUs_);

  Logger::info(String("DisplayDriver: scan pipeline started"));
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
  if (brightness_ == 0) {
    brightnessSlotDurationUs_ = 0;
    return;
  }
  // Interpolate the gamma-shaped fraction ACROSS the achievable range,
  // not as a multiplier against kBasePlaneQuantumUs alone — every nonzero
  // brightness lands somewhere in [kMinSlotDurationUs, kBasePlaneQuantumUs]
  // instead of most of the range clamping to the same floor value.
  const float normalized = powf(static_cast<float>(brightness_) / 255.0f, kBrightnessGamma);
  const float range = static_cast<float>(kBasePlaneQuantumUs - kMinSlotDurationUs);
  brightnessSlotDurationUs_ = static_cast<uint32_t>(kMinSlotDurationUs) + static_cast<uint32_t>(normalized * range);
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

bool IRAM_ATTR DisplayDriver::onAlarm(gptimer_handle_t timer, const gptimer_alarm_event_data_t* edata, void* userCtx) {
  (void)timer;
  (void)edata;
  DisplayDriver* self = static_cast<DisplayDriver*>(userCtx);
  BaseType_t highTaskAwoken = pdFALSE;
  vTaskNotifyGiveFromISR(self->scanTaskHandle_, &highTaskAwoken);
  return highTaskAwoken == pdTRUE;
}

void DisplayDriver::scanTaskEntry(void* arg) {
  static_cast<DisplayDriver*>(arg)->scanTaskLoop();
}

void DisplayDriver::scanTaskLoop() {
  for (;;) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    performScanStep();
  }
}

void DisplayDriver::rearmTimer(uint32_t delayUs) {
  uint64_t nowCount = 0;
  gptimer_get_raw_count(gpTimer_, &nowCount);
  gptimer_alarm_config_t alarmConfig = {};
  alarmConfig.alarm_count = nowCount + delayUs;
  alarmConfig.flags.auto_reload_on_alarm = 0;
  const esp_err_t err = gptimer_set_alarm_action(gpTimer_, &alarmConfig);
  if (err != ESP_OK) {
    // Not expected in steady state (this call can't itself throttle the
    // display any further than a missed frame), but silently dropping a
    // failed re-arm would freeze the scan loop with no diagnostic trail —
    // exactly the unexplained "stuck on one frame" bug from earlier in
    // development. Logger is safe to call from task context here (this
    // runs on scanTaskLoop_, not from the ISR).
    ++stats_.scanUnderruns;
  }
}

void DisplayDriver::transferCurrentRow() {
  const uint8_t planeIndex = kBamPlaneSchedule[currentSliceIndex_];
  const size_t rowOffset = static_cast<size_t>(planeIndex) * planeBytes_ + static_cast<size_t>(currentRow_) * rowBytes_;
  memcpy(transferRowBuffer_, scanBuffers_[activeScanBufferIndex_] + rowOffset, rowBytes_);

  // Latch stays low while data shifts in; latchRow() pulses it once this
  // (blocking, so already-complete-by-the-time-we-return) transfer is done.
  gpio_set_level(static_cast<gpio_num_t>(config_.pinLatch), 0);
  SPI.transfer(transferRowBuffer_, rowBytes_);
}

void DisplayDriver::performScanStep() {
  const uint32_t nowMicros = micros();

  if (!powerEnabled_) {
    blankOutput();
    currentSlotStartedUs_ = nowMicros;
    rearmTimer(kBasePlaneQuantumUs);
    return;
  }

  // The timer fires when it's actually time for the next slot, so this is
  // purely a diagnostic (did the scan task itself get delayed under load?),
  // not a "should I do anything yet" gate the way it was when this ran
  // from a busy-polled task loop.
  if ((nowMicros - currentSlotStartedUs_) > (currentSlotDurationUs_ * 2UL)) {
    ++stats_.scanUnderruns;
  }
  currentSlotStartedUs_ = nowMicros;
  const uint32_t stepStartedUs = micros();

  blankOutput();

  setRowAddress(currentRow_);

  const uint32_t transferStartedUs = micros();
  transferCurrentRow();
  stats_.waitStepLastUs = micros() - transferStartedUs;

  const uint32_t gpioStartedUs = micros();
  latchRow();
  // brightness_==0 stays blanked here rather than emitting the shortest
  // achievable pulse (~kMinSlotDurationUs, bounded by real per-step cost,
  // not by this constant) — otherwise "0" was still visibly, if dimly, lit.
  if (brightness_ > 0) {
    enableOutput();
  }
  stats_.gpioStepLastUs = micros() - gpioStartedUs;

  currentSlotDurationUs_ = brightness_ == 0 ? kMinSlotDurationUs : brightnessSlotDurationUs_;

  advanceScanPosition();

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

  const uint32_t stepElapsedUs = micros() - stepStartedUs;
  stats_.scanStepLastUs = stepElapsedUs;
  if (stepElapsedUs > stats_.scanStepMaxUs) {
    stats_.scanStepMaxUs = stepElapsedUs;
  }

  const uint32_t rearmStartedUs = micros();
  rearmTimer(currentSlotDurationUs_);
  stats_.rearmStepLastUs = micros() - rearmStartedUs;
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
