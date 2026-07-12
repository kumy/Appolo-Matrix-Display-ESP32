#include "core/Application.h"

#include "util/Logger.h"

Application::Application()
    : eventBus_(stats_),
      http_(settings_, wifi_, display_, clock_, stats_, *this),
      frontBuffer_(kWidth, kHeight, frontStorage_),
      backBuffer_(kWidth, kHeight, backStorage_),
      demoPage_(clock_, stats_),
      diagnosticsPage_(stats_),
      textPage_(String("LINE1\nLINE2")),
      clockPage_(clock_) {}

void Application::begin() {
  Logger::begin(115200);
  Logger::info(String("matrix-display-2026 scaffold boot"));

  settings_.begin();
  clock_.begin();
  wifi_.begin(settings_.values().hostname, settings_.values().wifiSsid, settings_.values().wifiPassword);
  http_.begin();
  mqtt_.begin();
  ota_.begin();

  const bool displayOk = display_.begin(settings_.values().display);
  Logger::info(String("Display init ") + (displayOk ? "OK" : "FAILED"));
  display_.setBrightness(settings_.values().brightness);
  display_.setPower(settings_.values().powerOn);
  display_.setPaletteLevelCount(settings_.values().paletteLevelCount);

  demoPage_.setAnimationSpeedPercent(settings_.values().animationSpeedPercent);
  const uint8_t storedScene = settings_.values().demoFixedScene;
  demoPage_.setFixedScene(storedScene == 255U ? -1 : static_cast<int8_t>(storedScene));

  clockPage_.setAnalogMode(settings_.values().clockAnalogMode);

  textPage_.setMessage(settings_.values().textMessage);
  textPage_.setAlign(static_cast<HorizontalAlign>(settings_.values().textAlign));
  textPage_.setAnimMode(static_cast<TextAnimMode>(settings_.values().textAnimMode));
  textPage_.setDirection(static_cast<TextScrollDirection>(settings_.values().textDirection));
  textPage_.setEffect(static_cast<TextEffect>(settings_.values().textEffect));

  diagnosticsPage_.setView(static_cast<DiagnosticsPage::View>(settings_.values().diagView));

  activePage_ = pageById(settings_.values().activePageId);
  activePageId_ = settings_.values().activePageId;
  if (activePage_ == nullptr) {
    activePage_ = &demoPage_;
    activePageId_ = 0;
  }
  activePage_->enter();
  lastFrameAtUs_ = micros();
  lastRenderStartedUs_ = lastFrameAtUs_;
  lastFrameWindowMs_ = millis();

  // core=0: alongside WiFi/lwIP/AsyncTCP, deliberately off of core 1 (which
  // DisplayDriver's scan task now has exclusively) — see the class-level
  // comment in Application.h. priority=1: matches AsyncTCP's/WiFi's usual
  // relative ranking (below the driver-level tasks, above idle), and this
  // task blocks every iteration (vTaskDelay), so it never competes for CPU
  // the way a busy-loop would.
  xTaskCreatePinnedToCore(&Application::appTaskEntry, "app_task", 8192, this, 1, &appTaskHandle_, 0);
}

Page* Application::pageById(uint8_t id) {
  switch (id) {
    case 0: return &demoPage_;
    case 1: return &clockPage_;
    case 2: return &textPage_;
    case 3: return &diagnosticsPage_;
    case 4: return &deathDatesPage_;
    default: return nullptr;
  }
}

void Application::setActivePage(uint8_t pageId) {
  Page* next = pageById(pageId);
  if (next == nullptr || next == activePage_) {
    return;
  }
  if (activePage_ != nullptr) {
    activePage_->leave();
  }
  activePage_ = next;
  activePageId_ = pageId;
  activePage_->enter();
}

void Application::appTaskEntry(void* arg) {
  static_cast<Application*>(arg)->appTaskLoop();
}

void Application::appTaskLoop() {
  for (;;) {
    tick();
    vTaskDelay(pdMS_TO_TICKS(1));
  }
}

void Application::tick() {
  const uint32_t nowMs = millis();
  const uint32_t nowUs = micros();

  wifi_.poll();
  http_.poll();
  mqtt_.poll();
  ota_.poll();
  clock_.updateFromMillis(nowMs);

  if (!ntpStarted_ && wifi_.isConnected()) {
    ntpStarted_ = true;
    clock_.beginNtp(settings_.values().ntpServer, settings_.values().utcOffsetMinutes);
  }

  if ((nowUs - lastRenderStartedUs_) < targetFramePeriodUs_) {
    return;
  }

  const uint32_t renderStartedUs = micros();
  lastRenderStartedUs_ = renderStartedUs;

  activePage_->update(nowMs);
  renderer_.beginFrame(backBuffer_);
  activePage_->draw(renderer_);

  backBuffer_.swap(frontBuffer_);
  const uint32_t renderEndedUs = micros();
  stats_.renderLatencyUs = renderEndedUs - renderStartedUs;

  display_.present(frontBuffer_);

  const uint32_t frameDeltaUs = renderEndedUs - lastFrameAtUs_;
  stats_.frameTimeJitterUs = frameDeltaUs > 16667UL ? (frameDeltaUs - 16667UL) : (16667UL - frameDeltaUs);
  lastFrameAtUs_ = renderEndedUs;
  ++framesThisWindow_;

  if ((nowMs - lastFrameWindowMs_) >= 1000UL) {
    stats_.appFps = framesThisWindow_;
    framesThisWindow_ = 0;
    lastFrameWindowMs_ = nowMs;
  }

  const RuntimeStats& driverStats = display_.stats();
  stats_.refreshHz = driverStats.refreshHz;
  stats_.publishLatencyUs = driverStats.publishLatencyUs;
  stats_.frameLatencyUs = driverStats.frameLatencyUs;
  stats_.droppedPresents = driverStats.droppedPresents;
  stats_.scanUnderruns = driverStats.scanUnderruns;
}
