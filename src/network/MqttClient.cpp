#include "network/MqttClient.h"

#include <cstring>

void MqttClient::begin() {
  state_ = "starting";
}

void MqttClient::poll() {
  if (strcmp(state_, "starting") == 0) {
    state_ = "idle";
  }
}

const char* MqttClient::state() const {
  return state_;
}
