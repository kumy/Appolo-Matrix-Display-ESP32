#include "display/FrameBuffer5.h"

#include <cstring>

FrameBuffer5::FrameBuffer5(uint16_t width, uint16_t height, uint8_t* storage)
    : width_(width), height_(height), storage_(storage) {
  clear();
}

void FrameBuffer5::clear() {
  fill(0);
}

void FrameBuffer5::fill(uint8_t value) {
  memset(storage_, value & 0x1F, sizeBytes());
}

void FrameBuffer5::setPixel(int16_t x, int16_t y, uint8_t gray) {
  if (x < 0 || y < 0 || x >= static_cast<int16_t>(width_) || y >= static_cast<int16_t>(height_)) {
    return;
  }
  storage_[pixelIndex(x, y)] = gray & 0x1F;
}

uint8_t FrameBuffer5::getPixel(int16_t x, int16_t y) const {
  if (x < 0 || y < 0 || x >= static_cast<int16_t>(width_) || y >= static_cast<int16_t>(height_)) {
    return 0;
  }
  return storage_[pixelIndex(x, y)];
}

void FrameBuffer5::copyFrom(const FrameBuffer5& other) {
  if (width_ != other.width_ || height_ != other.height_) {
    return;
  }
  memcpy(storage_, other.storage_, sizeBytes());
}

void FrameBuffer5::swap(FrameBuffer5& other) {
  if (width_ != other.width_ || height_ != other.height_) {
    return;
  }
  for (size_t i = 0; i < sizeBytes(); ++i) {
    const uint8_t tmp = storage_[i];
    storage_[i] = other.storage_[i];
    other.storage_[i] = tmp;
  }
}

size_t FrameBuffer5::pixelIndex(int16_t x, int16_t y) const {
  return static_cast<size_t>(y) * static_cast<size_t>(width_) + static_cast<size_t>(x);
}
