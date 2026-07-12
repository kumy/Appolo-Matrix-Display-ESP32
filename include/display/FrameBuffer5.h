#pragma once

#include <Arduino.h>

// One byte per pixel (low 5 bits used, 0-31) rather than the previous
// 4-bit/nibble-packed format — 5 bits doesn't divide evenly into a byte,
// and the extra memory (1280 bytes vs 640 for an 80x16 panel) is trivial
// against available RAM, so simplicity won over packing density here.
class FrameBuffer5 {
public:
  static constexpr size_t bytesFor(uint16_t width, uint16_t height) {
    return static_cast<size_t>(width) * static_cast<size_t>(height);
  }

  FrameBuffer5(uint16_t width, uint16_t height, uint8_t* storage);

  void clear();
  void fill(uint8_t value);
  void setPixel(int16_t x, int16_t y, uint8_t gray);
  uint8_t getPixel(int16_t x, int16_t y) const;
  void copyFrom(const FrameBuffer5& other);
  void swap(FrameBuffer5& other);

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
