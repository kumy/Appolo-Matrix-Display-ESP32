#include "network/OtaService.h"

void OtaService::begin() {
  active_ = false;
}

void OtaService::poll() {}

bool OtaService::active() const {
  return active_;
}
