#include "network/WifiManager.h"

#include <cstring>

void WifiManager::begin() {
  state_ = "starting";
}

void WifiManager::poll() {
  if (strcmp(state_, "starting") == 0) {
    state_ = "idle";
  }
}

const char* WifiManager::state() const {
  return state_;
}
