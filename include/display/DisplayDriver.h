#pragma once

#include <Arduino.h>
#include <driver/gptimer.h>

#include <cstddef>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "core/RuntimeStats.h"
#include "display/DisplayConfig.h"

class FrameBuffer4;

// Scan timing is driven by a self-managed gptimer, whose ISR does nothing
// but wake a dedicated FreeRTOS task, which then runs performScanStep() —
// see the long comment on gpTimer_ for why this is a self-managed gptimer
// rather than esp_timer, and why the wake-up mechanism still needs to be a
// proper task (not the busy-loop this replaced, nor work done directly in
// the ISR). begin() starts the timer/task pair and they keep each other
// going from then on; there's no public per-slot "service" method to call
// from Application's own loop.
//
// The SPI transfer itself uses plain Arduino SPIClass::transfer() — a
// single blocking call per row — not the raw ESP-IDF spi_master driver.
// Two attempts at spi_master's transaction API were tried first (queued
// +polled, then blocking spi_device_transmit()) and both carried
// ~40-85us of fixed per-transaction overhead for this transfer size
// (~10 bytes/row): many times the ~4-5us the wire transfer itself takes
// at 20MHz, and enough by itself to force a large minimum slot duration.
// SPIClass::transfer() doesn't carry that overhead for small transfers.
//
// Frame computation (Application::tick(): draws new frames, calls
// present(), polls WiFi/HTTP) now runs on its own dedicated task pinned
// to core 0, alongside WiFi/lwIP/AsyncTCP — see Application.h — leaving
// core 1 dedicated entirely to this scan task. That, combined with the
// SPI change above, is what fixed an earlier "renders correctly but
// never animates" regression: when per-step processing chronically
// exceeded the requested slot duration, the scan task's timer kept
// re-arming to a point already in the past, so it never genuinely
// blocked and fully starved whatever else shared its core under strict
// FreeRTOS priority scheduling (confirmed live: 0 fps on the task that
// used to share core 1 with this one).
//
// Global brightness is applied in SOFTWARE, at buildScanPlanes() time
// (scaling each pixel's raw value before it's quantized to the
// GrayLevels palette), NOT as scan timing. Two earlier timing-based
// designs were tried and abandoned:
//  - Scaling the BAM slot duration itself by brightness: mixed two
//    unrelated concerns (per-pixel weighting and global dimming) into
//    one number, which made dim gray levels become extremely short,
//    timing-sensitive pulses at low brightness — visible live as those
//    levels blinking far worse than bright ones.
//  - A nested, dithered PWM window inside each already-scheduled BAM
//    slot (duration fixed, only the enabled portion scaled): this kept
//    per-pixel timing fixed, but every row is only refreshed once per
//    full scan cycle (an inherent property of row multiplexing), so a
//    dithered pulse representing a low duty cycle fires at a rate well
//    below the base refresh rate — e.g. "once every 5 cycles" at ~85Hz
//    is ~17Hz, squarely in visible-flicker territory. Confirmed live:
//    flicker below roughly brightness 110, plus non-monotonic gray
//    bands from the sub-microsecond windows this forced at low
//    brightness. No amount of dithering math fixes a rate that's
//    fundamentally bounded by the refresh cycle itself.
// Software pixel scaling sidesteps both: the scan loop's timing is 100%
// fixed and brightness-independent (so it can run as fast as hardware
// allows — see kBasePlaneQuantumUs), and dimming just picks among the
// SAME fast, already-flicker-free BAM slot durations for a dimmer
// palette entry. Trade-off: brightness resolution is limited to the
// discrete GrayLevels palette (coarser steps), and a dim pixel at low
// global brightness can round down to fully off rather than a faint
// glow.
//
// Per-pixel gray level BAM order is row-major/interleaved (all 4 planes
// for a row before moving to the next row — see advanceScanPosition()),
// not plane-major (all rows for plane 0, then all rows for plane 1,
// ...), which spread a single row's four plane-visits across the entire
// refresh cycle and was suspected of contributing to motion smear on
// fast-moving sprites.
class DisplayDriver {
public:
  ~DisplayDriver();
  bool begin(const DisplayConfig& config);
  void setPower(bool enabled);
  void setBrightness(uint8_t brightness);
  void present(const FrameBuffer4& frame);
  const RuntimeStats& stats() const;
  bool powerEnabled() const;
  uint8_t brightness() const;

private:
  static bool IRAM_ATTR onAlarm(gptimer_handle_t timer, const gptimer_alarm_event_data_t* edata, void* userCtx);
  void rearmTimer(uint32_t delayUs);
  static void scanTaskEntry(void* arg);
  void scanTaskLoop();
  void performScanStep();
  void transferCurrentRow();
  void blankOutput();
  void enableOutput();
  void setRowAddress(uint8_t row);
  void latchRow();
  void buildScanPlanes(const FrameBuffer4& frame, uint8_t* destination);
  void advanceScanPosition();

  DisplayConfig config_;
  portMUX_TYPE scanMux_ = portMUX_INITIALIZER_UNLOCKED;
  // esp_timer's ESP_TIMER_TASK dispatch method runs callbacks on a single
  // shared service task whose core affinity is fixed by the precompiled
  // framework — it defaults to core 0 (confirmed by a watchdog panic during
  // development, when the callback did the FULL scan step: "async_tcp did
  // not reset the watchdog in time... CPU 0: esp_timer" — ~41,000
  // invocations/sec of real GPIO/SPI-DMA work was enough to starve it,
  // recreating the exact WiFi-starvation problem this whole rewrite exists
  // to avoid). A hybrid was tried next: keep esp_timer but slim its
  // callback down to a bare xTaskNotifyGive(). That avoided the watchdog
  // panic, but esp_timer's service task is pinned to core 0 while the scan
  // task lives on core 1 — every single row/plane slot paid a cross-core
  // notify, and the resulting jitter was visible as flicker on a display
  // where each row is only lit 1/16 of the time to begin with (any
  // inconsistency in slot timing shows up immediately as brightness
  // modulation). This self-managed gptimer is created and started from
  // begin(), which runs on core 1 — its ISR therefore fires on core 1 too,
  // giving a same-core, low-jitter wake straight into scanTaskLoop_ with no
  // cross-core hop, and gives the steadiest rendering of the designs tried
  // so far.
  gptimer_handle_t gpTimer_ = nullptr;
  TaskHandle_t scanTaskHandle_ = nullptr;

  uint8_t* transferRowBuffer_ = nullptr;

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
  // Precomputed by setBrightness() (gamma-corrected): the fraction each
  // pixel's raw value is scaled by in buildScanPlanes() before
  // quantizing to the GrayLevels palette — see the class-level comment
  // on why this is software pixel scaling, not scan timing.
  float brightnessScale_ = 1.0f;
  uint32_t refreshFramesThisWindow_ = 0;
  uint8_t currentRow_ = 0;
  uint8_t currentSliceIndex_ = 0;
  bool powerEnabled_ = true;
  uint8_t brightness_ = 255;
  RuntimeStats stats_;
};
