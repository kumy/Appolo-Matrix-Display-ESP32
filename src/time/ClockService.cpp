#include "time/ClockService.h"

#include <time.h>

namespace {
// Sometime in 2023 — anything before this means the system clock is still
// at its post-boot epoch default (1970), i.e. SNTP hasn't landed yet.
constexpr time_t kSaneEpochLowerBound = 1700000000;
}

void ClockService::begin() {
  nowMs_ = 0;
}

void ClockService::beginNtp(const String& server, int16_t utcOffsetMinutes) {
  configTime(static_cast<long>(utcOffsetMinutes) * 60L, 0, server.c_str());
  ntpConfigured_ = true;
}

void ClockService::updateFromMillis(uint32_t nowMs) {
  nowMs_ = nowMs;
}

bool ClockService::synced() const {
  return ntpConfigured_ && time(nullptr) >= kSaneEpochLowerBound;
}

uint8_t ClockService::hours() const {
  if (synced()) {
    const time_t now = time(nullptr);
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);
    return static_cast<uint8_t>(timeinfo.tm_hour);
  }
  return static_cast<uint8_t>(((nowMs_ / 1000UL) / 3600UL) % 24UL);
}

uint8_t ClockService::minutes() const {
  if (synced()) {
    const time_t now = time(nullptr);
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);
    return static_cast<uint8_t>(timeinfo.tm_min);
  }
  return static_cast<uint8_t>(((nowMs_ / 1000UL) / 60UL) % 60UL);
}

uint8_t ClockService::seconds() const {
  if (synced()) {
    const time_t now = time(nullptr);
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);
    return static_cast<uint8_t>(timeinfo.tm_sec);
  }
  return static_cast<uint8_t>((nowMs_ / 1000UL) % 60UL);
}

time_t ClockService::epochSeconds() const {
  return synced() ? time(nullptr) : 0;
}

String ClockService::timeString() const {
  char buffer[9];
  snprintf(buffer, sizeof(buffer), "%02u:%02u:%02u", hours(), minutes(), seconds());
  return String(buffer);
}

String ClockService::dateString() const {
  if (!synced()) {
    return String("not synced");
  }
  const time_t now = time(nullptr);
  struct tm timeinfo;
  localtime_r(&now, &timeinfo);
  char buffer[11];
  snprintf(buffer, sizeof(buffer), "%04u-%02u-%02u", timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday);
  return String(buffer);
}
