#pragma once

#include <Arduino.h>

class MqttClient {
public:
  void begin();
  void poll();
  const char* state() const;

private:
  const char* state_ = "disabled";
};
