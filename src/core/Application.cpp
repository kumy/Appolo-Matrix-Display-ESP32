#include "core/Application.h"

#include "util/Logger.h"

Application::Application()
    : eventBus_(stats_),
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
  wifi_.begin();
  http_.begin();
  mqtt_.begin();
  ota_.begin();

  display_.begin(settings_.values().display);
  display_.setBrightness(settings_.values().brightness);
  display_.setPower(settings_.values().powerOn);

  activePage_ = &demoPage_;
  activePage_->enter();
  lastFrameAtUs_ = micros();
  lastFrameWindowMs_ = millis();
}

void Application::tick() {
  const uint32_t nowMs = millis();
  const uint32_t renderStartedUs = micros();

  wifi_.poll();
  http_.poll();
  mqtt_.poll();
  ota_.poll();
  clock_.updateFromMillis(nowMs);

  activePage_->update(nowMs);
  renderer_.beginFrame(backBuffer_);
  activePage_->draw(renderer_);

  backBuffer_.swap(frontBuffer_);
  const uint32_t renderEndedUs = micros();
  stats_.renderLatencyUs = renderEndedUs - renderStartedUs;

  display_.present(frontBuffer_);
  display_.serviceScan(renderEndedUs);

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
  stats_.droppedPresents += driverStats.droppedPresents;
  stats_.scanUnderruns += driverStats.scanUnderruns;
}
