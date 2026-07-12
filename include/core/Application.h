#pragma once

#include <Arduino.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "core/EventBus.h"
#include "core/RuntimeStats.h"
#include "display/DisplayDriver.h"
#include "display/FrameBuffer4.h"
#include "display/Renderer.h"
#include "network/HttpServer.h"
#include "network/MqttClient.h"
#include "network/OtaService.h"
#include "network/WifiManager.h"
#include "pages/ClockPage.h"
#include "pages/DeathDatesPage.h"
#include "pages/DiagnosticsPage.h"
#include "pages/DemoPage.h"
#include "pages/TextPage.h"
#include "storage/Settings.h"
#include "time/ClockService.h"

// Frame rendering (activePage_->update()/draw()) and network polling run
// on a dedicated task pinned to core 0 — the SAME core as WiFi/lwIP and
// AsyncTCP — rather than the default Arduino loop() task (which the
// framework pins to core 1). This is deliberate: core 1 is reserved
// exclusively for DisplayDriver's own scan task, which needs tight,
// consistent slot timing and was being starved when it had to share core 1
// with frame computation (see DisplayDriver.h's class comment for the
// full history). Putting frame computation on core 0 instead risks the
// opposite problem — WiFi/AsyncTCP work missing its own timing if this
// task were a greedy busy-loop — but appTaskLoop() genuinely blocks
// between iterations (vTaskDelay), and AsyncTCP/WiFi's own driver tasks
// run at higher priority than this one, so they still preempt it as
// needed rather than being starved by it.
class Application {
public:
  Application();

  void begin();

private:
  static void appTaskEntry(void* arg);
  void appTaskLoop();
  void tick();

  static constexpr uint16_t kWidth = 80;
  static constexpr uint16_t kHeight = 16;
  static constexpr size_t kBufferSize = FrameBuffer4::bytesFor(kWidth, kHeight);

  RuntimeStats stats_;
  EventBus<16> eventBus_;
  Settings settings_;
  ClockService clock_;
  WifiManager wifi_;
  HttpServer http_;
  MqttClient mqtt_;
  OtaService ota_;
  DisplayDriver display_;
  Renderer renderer_;
  uint8_t frontStorage_[kBufferSize] = {0};
  uint8_t backStorage_[kBufferSize] = {0};
  FrameBuffer4 frontBuffer_;
  FrameBuffer4 backBuffer_;
  DemoPage demoPage_;
  DiagnosticsPage diagnosticsPage_;
  TextPage textPage_;
  ClockPage clockPage_;
  DeathDatesPage deathDatesPage_;
  Page* activePage_ = nullptr;
  uint32_t lastFrameAtUs_ = 0;
  uint32_t lastRenderStartedUs_ = 0;
  uint32_t lastFrameWindowMs_ = 0;
  uint32_t framesThisWindow_ = 0;
  uint32_t targetFramePeriodUs_ = 16667;
  bool ntpStarted_ = false;
  TaskHandle_t appTaskHandle_ = nullptr;
};
