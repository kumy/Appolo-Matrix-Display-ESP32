#pragma once

#include <Arduino.h>

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
#include "pages/DiagnosticsPage.h"
#include "pages/DemoPage.h"
#include "pages/TextPage.h"
#include "storage/Settings.h"
#include "time/ClockService.h"

class Application {
public:
  Application();

  void begin();
  void tick();

private:
  static void refreshTaskEntry(void* parameter);
  void refreshTaskLoop();

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
  Page* activePage_ = nullptr;
  TaskHandle_t refreshTaskHandle_ = nullptr;
  uint32_t lastFrameAtUs_ = 0;
  uint32_t lastRenderStartedUs_ = 0;
  uint32_t lastFrameWindowMs_ = 0;
  uint32_t framesThisWindow_ = 0;
  uint32_t targetFramePeriodUs_ = 16667;
};
