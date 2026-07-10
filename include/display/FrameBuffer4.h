#pragma once

#include <Arduino.h>

class FrameBuffer4 {
public:
  static constexpr size_t bytesFor(uint16_t width, uint16_t height) {
    return (static_cast<size_t>(width) * static_cast<size_t>(height) + 1U) / 2U;
  }

  FrameBuffer4(uint16_t width, uint16_t height, uint8_t* storage);

  void clear();
  void fill(uint8_t value);
  void setPixel(int16_t x, int16_t y, uint8_t gray);
  uint8_t getPixel(int16_t x, int16_t y) const;
  void copyFrom(const FrameBuffer4& other);
  void swap(FrameBuffer4& other);

  uint16_t width() const { return width_; }
  uint16_t height() const { return height_; }
  uint8_t* data() { return storage_; }
  const uint8_t* data() const { return storage_; }
  size_t sizeBytes() const { return bytesFor(width_, height_); }

private:
  size_t pixelIndex(int16_t x, int16_t y) const;

  uint16_t width_;
  uint16_t height_;
  uint8_t* storage_;
};
