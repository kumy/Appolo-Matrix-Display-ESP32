#include "display/FrameBuffer4.h"

#include <cstring>

FrameBuffer4::FrameBuffer4(uint16_t width, uint16_t height, uint8_t* storage)
    : width_(width), height_(height), storage_(storage) {
  clear();
}

void FrameBuffer4::clear() {
  fill(0);
}

void FrameBuffer4::fill(uint8_t value) {
  value &= 0x0F;
  const uint8_t packed = static_cast<uint8_t>((value << 4) | value);
  memset(storage_, packed, sizeBytes());
}

void FrameBuffer4::setPixel(int16_t x, int16_t y, uint8_t gray) {
  if (x < 0 || y < 0 || x >= static_cast<int16_t>(width_) || y >= static_cast<int16_t>(height_)) {
    return;
  }
  gray &= 0x0F;
  const size_t index = pixelIndex(x, y);
  const size_t byteIndex = index / 2U;
  if ((index & 1U) == 0U) {
    storage_[byteIndex] = static_cast<uint8_t>((storage_[byteIndex] & 0x0F) | (gray << 4));
  } else {
    storage_[byteIndex] = static_cast<uint8_t>((storage_[byteIndex] & 0xF0) | gray);
  }
}

uint8_t FrameBuffer4::getPixel(int16_t x, int16_t y) const {
  if (x < 0 || y < 0 || x >= static_cast<int16_t>(width_) || y >= static_cast<int16_t>(height_)) {
    return 0;
  }
  const size_t index = pixelIndex(x, y);
  const uint8_t value = storage_[index / 2U];
  return (index & 1U) == 0U ? static_cast<uint8_t>((value >> 4) & 0x0F) : static_cast<uint8_t>(value & 0x0F);
}

void FrameBuffer4::copyFrom(const FrameBuffer4& other) {
  if (width_ != other.width_ || height_ != other.height_) {
    return;
  }
  memcpy(storage_, other.storage_, sizeBytes());
}

void FrameBuffer4::swap(FrameBuffer4& other) {
  if (width_ != other.width_ || height_ != other.height_) {
    return;
  }
  for (size_t i = 0; i < sizeBytes(); ++i) {
    const uint8_t tmp = storage_[i];
    storage_[i] = other.storage_[i];
    other.storage_[i] = tmp;
  }
}

size_t FrameBuffer4::pixelIndex(int16_t x, int16_t y) const {
  return static_cast<size_t>(y) * static_cast<size_t>(width_) + static_cast<size_t>(x);
}
