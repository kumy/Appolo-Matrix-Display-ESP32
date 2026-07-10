#pragma once

#include <Arduino.h>

enum class LogLevel {
  Error,
  Warn,
  Info,
  Debug,
  Trace,
};

class Logger {
public:
  static void begin(unsigned long baudRate);
  static void log(LogLevel level, const String& message);
  static void info(const String& message);
};
