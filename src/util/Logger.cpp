#include "util/Logger.h"

namespace {
const char* labelFor(LogLevel level) {
  switch (level) {
    case LogLevel::Error: return "ERROR";
    case LogLevel::Warn: return "WARN";
    case LogLevel::Info: return "INFO";
    case LogLevel::Debug: return "DEBUG";
    case LogLevel::Trace: return "TRACE";
  }
  return "INFO";
}
}

void Logger::begin(unsigned long baudRate) {
  Serial.begin(baudRate);
}

void Logger::log(LogLevel level, const String& message) {
  Serial.print('[');
  Serial.print(labelFor(level));
  Serial.print("] ");
  Serial.println(message);
}

void Logger::info(const String& message) {
  log(LogLevel::Info, message);
}
