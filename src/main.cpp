#include <Arduino.h>

#include "core/Application.h"

namespace {
Application app;
}

void setup() {
  app.begin();
}

void loop() {
  // Application runs its own dedicated task on core 0 (see
  // Application::begin()) — nothing left for the default loop() task to
  // do. A real delay (not an empty spin) so this otherwise-idle task on
  // core 1 doesn't busy-poll against DisplayDriver's scan task.
  delay(1000);
}
