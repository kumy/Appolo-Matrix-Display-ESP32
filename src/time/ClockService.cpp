#include "time/ClockService.h"

void ClockService::begin() {
  nowMs_ = 0;
  synced_ = false;
}

void ClockService::updateFromMillis(uint32_t nowMs) {
  nowMs_ = nowMs;
}

String ClockService::timeString() const {
  char buffer[9];
  snprintf(buffer, sizeof(buffer), "%02u:%02u:%02u", hours(), minutes(), seconds());
  return String(buffer);
}

String ClockService::dateString() const {
  return String("2026-07-10");
}

bool ClockService::synced() const {
  return synced_;
}

uint8_t ClockService::hours() const {
  return static_cast<uint8_t>(((nowMs_ / 1000UL) / 3600UL) % 24UL);
}

uint8_t ClockService::minutes() const {
  return static_cast<uint8_t>(((nowMs_ / 1000UL) / 60UL) % 60UL);
}

uint8_t ClockService::seconds() const {
  return static_cast<uint8_t>((nowMs_ / 1000UL) % 60UL);
}
