#pragma once

#include <Arduino.h>

class HttpServer {
public:
  void begin();
  void poll();
  bool running() const;

private:
  bool running_ = false;
};
