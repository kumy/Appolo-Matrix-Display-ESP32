#pragma once

#include <Arduino.h>

class OtaService {
public:
  void begin();
  void poll();
  bool active() const;

private:
  bool active_ = false;
};
