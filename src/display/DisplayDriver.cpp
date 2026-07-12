#include "display/DisplayDriver.h"

#include <SPI.h>
#include <cmath>
#include <cstring>

#include <driver/gpio.h>

#include "display/FrameBuffer5.h"
#include "display/GrayLevels.h"
#include "util/Logger.h"

namespace {
// 5 bit-planes (32 raw levels, 0-31) — raised from 4 (16 levels)
// specifically to get more usable grayscale/dimming resolution. Each
// added plane roughly DOUBLES total scan time per frame (true BAM's
// per-plane weight is 2^planeIndex, so total weighted time per row is
// 2^kPlaneCount - 1 quanta) — this is a hard property of weighted binary
// PWM, not a tuning knob, so bit depth and refresh rate trade off
// directly. 5 was chosen as the balance: ~2x the 4-bit design's total
// scan time, refresh still comfortably fast. A frame-to-frame temporal
// dithering layer was tried on top of this (in buildScanPlanes()) to
// recover resolution beyond these 32 raw levels for brightness scaling,
// but was reverted — see buildScanPlanes()'s comment — it reintroduced
// flicker via low-frequency alternation between adjacent levels.
constexpr uint8_t kPlaneCount = 5;
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
// gptimer_get_raw_count()/gptimer_set_alarm_action() + GPIO work). This
// FIXED base quantum sits comfortably above that, and — unlike earlier
// designs — is completely independent of brightness (see the
// class-level comment in DisplayDriver.h): brightness is now applied by
// scaling pixel values in software before quantizeGrayLevel(), not by
// touching scan timing at all. That means this constant can be tuned
// purely for refresh rate/motion clarity without any brightness-related
// trade-off pulling the other way.
constexpr uint16_t kBasePlaneQuantumUs = 24;
// Human brightness perception is roughly logarithmic, not linear with
// duty cycle — a linear PWM mapping spends most of its input range in the
// perceptually-saturated "looks full" zone (confirmed live: 25/50/100
// all looked like 255 under a linear mapping). Gamma correction
// concentrates the visible dimming into the range that actually looks
// dim. 2.5 is a common starting point for LED PWM (sRGB-style displays
// typically use ~2.2).
constexpr float kBrightnessGamma = 2.5f;
// True Bit Angle Modulation: one slot per bit-plane per row, with slot
// DURATION weighted 1:2:4:8 by plane — not repetition-based BCM (the
// design tried first used kPlaneCount*uniform-duration slots repeated at
// different FREQUENCIES: plane 3 appearing 8x in a 15-slot schedule,
// plane 0 only once, to fake the same weighting). That repetition scheme
// meant the dimmest plane (weight 1, the only bit lit for the faintest
// visible gray level) showed up as a single, sparse pulse per full
// refresh cycle — clearly visible as a "blink" distinct from brighter,
// more-frequently-repeated planes, especially at refresh rates well
// below the ~200-400Hz where classic BCM shimmer stops being perceptible
// (confirmed live: dim levels blinking worse than bright ones). True BAM
// gives every plane exactly ONE slot per cycle (same visit frequency for
// all gray levels), and cuts total per-frame row-visits from
// kPlaneCount*(sum of old repeat counts) down to just kPlaneCount*rows —
// here 4x16=64 instead of 15x16=240 — which directly buys back
// refresh-rate headroom by paying the fixed per-step transfer overhead
// far fewer times per frame.
constexpr uint8_t kBamSliceCount = kPlaneCount;
constexpr uint8_t kBamPlaneSchedule[kBamSliceCount] = {0, 1, 2, 3, 4};
constexpr uint16_t kRawLevelCount = 1U << kPlaneCount;  // 32 for 5 planes
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
  rebuildGrayLevelLut();

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
  // Gamma-corrected fraction each pixel's raw value is scaled by in
  // buildScanPlanes() before quantizing to the GrayLevels palette — a
  // linear brightness/255 fraction would spend most of its input range
  // still looking "full" to the eye (confirmed live under the earlier
  // timing-based design, which used the same gamma curve for the same
  // reason).
  brightnessScale_ = powf(static_cast<float>(brightness_) / 255.0f, kBrightnessGamma);
}

void DisplayDriver::setPaletteLevelCount(uint8_t count) {
  if (count < 2U) {
    count = 2U;
  } else if (count > kRawLevelCount) {
    count = static_cast<uint8_t>(kRawLevelCount);
  }
  paletteLevelCount_ = count;
  rebuildGrayLevelLut();
}

void DisplayDriver::rebuildGrayLevelLut() {
  // Snaps every raw 0-(kRawLevelCount-1) value to the nearest of
  // paletteLevelCount_ evenly-spaced steps spanning the full hardware
  // range — a live "posterize" control over the gray gradient,
  // independent of GrayLevels::kLevels (which stays the full,
  // unconstrained hardware palette; procedural shading via
  // GrayLevels::shade()/nearest() is unaffected by this setting).
  for (uint16_t raw = 0; raw < kRawLevelCount; ++raw) {
    uint8_t best = 0;
    uint16_t bestDist = 0xFFFFU;
    for (uint8_t i = 0; i < paletteLevelCount_; ++i) {
      const uint8_t candidate = static_cast<uint8_t>(
          (static_cast<uint32_t>(kRawLevelCount - 1) * i) / (paletteLevelCount_ - 1));
      const uint16_t dist = raw > candidate ? static_cast<uint16_t>(raw - candidate) : static_cast<uint16_t>(candidate - raw);
      if (dist < bestDist) {
        bestDist = dist;
        best = candidate;
      }
    }
    grayLevelLut_[raw] = best;
  }
}

uint8_t DisplayDriver::quantizeGrayLevel(uint8_t raw) const {
  return grayLevelLut_[raw];
}

void DisplayDriver::present(const FrameBuffer5& frame) {
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

  // True BAM slot duration: FIXED, weighted by plane, never scaled by
  // brightness — see the class-level comment on the two-layer split.
  const uint8_t planeIndex = kBamPlaneSchedule[currentSliceIndex_];
  currentSlotDurationUs_ = static_cast<uint32_t>(kBasePlaneQuantumUs) << planeIndex;

  blankOutput();

  setRowAddress(currentRow_);

  const uint32_t transferStartedUs = micros();
  transferCurrentRow();
  stats_.waitStepLastUs = micros() - transferStartedUs;

  // Global brightness no longer touches this at all — see the
  // class-level comment on software pixel scaling. A pixel/plane is
  // either lit for its full, fixed BAM slot duration or not lit; there's
  // nothing to dim here, so every slot's timing is uniform and fast.
  const uint32_t gpioStartedUs = micros();
  latchRow();
  enableOutput();
  stats_.gpioStepLastUs = micros() - gpioStartedUs;

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

void DisplayDriver::buildScanPlanes(const FrameBuffer5& frame, uint8_t* destination) {
  memset(destination, 0, scanBufferBytes_);
  // Global brightness applied here, in software, before quantization —
  // see the class-level comment for why (flicker/precision problems with
  // scan-timing-based dimming). Runs on the core-0 app task (via
  // present()), not the timing-critical core-1 scan task, so there's no
  // cost to doing float math per pixel here.
  for (uint16_t y = 0; y < config_.height; ++y) {
    for (uint16_t x = 0; x < config_.width; ++x) {
      const uint8_t raw = frame.getPixel(x, y) & 0x1FU;
      // Temporal (frame-to-frame) dithering was tried here to recover
      // resolution beyond what the brightness scaling can represent
      // directly, but alternating a pixel's rendered level between two
      // adjacent palette entries across frames reintroduces the exact
      // "low toggle frequency reads as flicker" problem this whole
      // design was meant to avoid — for uneven splits the alternation
      // rate can drop well below a comfortable flicker-free frequency
      // (confirmed live: flicker specifically in the brightness range
      // where interpolation was active, not at the extremes where
      // rounding is exact). Plain per-frame rounding instead — no
      // memory between frames.
      uint8_t scaledRaw = static_cast<uint8_t>(lroundf(static_cast<float>(raw) * brightnessScale_));
      // Floor any nonzero source pixel to at least the dimmest palette
      // level rather than letting rounding collapse it to 0 — without
      // this, low-but-nonzero brightness settings would render every
      // pixel black until the scale climbs high enough to round up on
      // its own (confirmed live under the pre-dithering version of this
      // code). This keeps brightness_==0 as true black (scale is exactly
      // 0 there) while giving every other setting a visible, if faint,
      // floor. Applied after dithering, not folded into the error term,
      // since this is a deliberate visibility override rather than a
      // rounding correction.
      if (raw > 0U && brightness_ > 0U && scaledRaw == 0U) {
        scaledRaw = 1U;
      }
      const uint8_t level = quantizeGrayLevel(scaledRaw);
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
  // Row-major/interleaved: cycle through all kBamSliceCount planes for
  // the CURRENT row before moving to the next row, rather than the
  // plane-major order this replaced (all rows for plane 0, then all rows
  // for plane 1, ...). Plane-major spread a single row's four
  // plane-visits across the entire refresh cycle (e.g. row 0's plane-3
  // visit could be ~48 steps / most of a cycle later than its plane-0
  // visit); row-major resolves a row's complete weighted brightness in
  // one tight, consecutive burst instead, which is the more standard
  // BAM/BCM order and was suspected of contributing to the motion-smear
  // artifact reported on fast-moving sprites.
  ++currentSliceIndex_;
  if (currentSliceIndex_ >= kBamSliceCount) {
    currentSliceIndex_ = 0;
    ++currentRow_;
    if (currentRow_ >= config_.height) {
      currentRow_ = 0;
    }
  }
}
