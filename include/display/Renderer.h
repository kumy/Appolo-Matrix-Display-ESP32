#pragma once

#include <Arduino.h>

#include "display/FrameBuffer4.h"

enum class HorizontalAlign {
  Left,
  Center,
  Right,
};

enum class TextWrapMode {
  None,
  Character,
  Word,
};

struct TextLayoutOptions {
  int16_t maxWidth = 0;
  int16_t maxHeight = 0;
  int8_t lineSpacing = 1;
  TextWrapMode wrapMode = TextWrapMode::None;
  HorizontalAlign align = HorizontalAlign::Left;
};

class Renderer {
public:
  void beginFrame(FrameBuffer4& target);
  void clear(uint8_t gray = 0);
  void drawPixel(int16_t x, int16_t y, uint8_t gray);
  void drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint8_t gray);
  void drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t gray);
  void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t gray);
  void drawBitmap(int16_t x, int16_t y, const uint8_t* bitmap, int16_t w, int16_t h, uint8_t gray);
  void drawText(int16_t x, int16_t y, const String& text, uint8_t gray);
  void drawTextBox(int16_t x, int16_t y, const TextLayoutOptions& options, const String& text, uint8_t gray);

private:
  void drawChar(int16_t x, int16_t y, char c, uint8_t gray);
  void drawGlyphRows(int16_t x, int16_t y, const uint8_t* rows, uint8_t gray);

  FrameBuffer4* target_ = nullptr;
};
