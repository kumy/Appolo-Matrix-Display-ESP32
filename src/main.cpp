#include <Arduino.h>

#include "core/Application.h"

namespace {
Application app;
}

void setup() {
  app.begin();
}

void loop() {
  app.tick();
}
