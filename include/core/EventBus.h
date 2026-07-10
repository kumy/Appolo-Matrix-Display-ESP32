#pragma once

#include <Arduino.h>

#include "core/RuntimeStats.h"

enum class EventType {
  None,
  SetText,
  SetPage,
  SetEffect,
  SetBrightness,
  SetPower,
  SetConfig,
  Reboot,
  TimeSync,
  WifiStateChanged,
  MqttStateChanged,
};

struct Event {
  EventType type = EventType::None;
  int32_t value = 0;
  char text[64] = {0};
};

template <size_t Capacity>
class EventBus {
public:
  explicit EventBus(RuntimeStats& stats) : stats_(stats) {}

  bool publish(const Event& event) {
    if (size_ >= Capacity) {
      ++stats_.droppedEvents;
      return false;
    }
    queue_[tail_] = event;
    tail_ = (tail_ + 1U) % Capacity;
    ++size_;
    return true;
  }

  bool poll(Event& event) {
    if (size_ == 0) {
      return false;
    }
    event = queue_[head_];
    head_ = (head_ + 1U) % Capacity;
    --size_;
    return true;
  }

  size_t size() const { return size_; }

private:
  RuntimeStats& stats_;
  Event queue_[Capacity];
  size_t head_ = 0;
  size_t tail_ = 0;
  size_t size_ = 0;
};
