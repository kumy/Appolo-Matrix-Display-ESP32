#include "display/Renderer.h"

#include <algorithm>
#include <cmath>

#include "display/Font.h"

namespace {
// Compiled-in fallback glyph set, used whenever no Font is set via
// setFont() or the set Font failed to load/doesn't cover a given
// character — see Font.h's class comment. Also the source data for
// data/fonts/classic3x5.font (extracted verbatim by the generator script,
// not retyped, so the two can never drift out of sync by transcription
// error).
constexpr uint8_t kBuiltinGlyphWidth = 3;
constexpr uint8_t kBuiltinGlyphHeight = 5;
constexpr uint8_t kBuiltinCharPitch = 4;
constexpr uint8_t kBuiltinLinePitch = 6;

const uint8_t* builtinGlyphRows(char c) {
  static const uint8_t space[5] = {0, 0, 0, 0, 0};
  static const uint8_t colon[5] = {0, 2, 0, 2, 0};
  static const uint8_t dash[5] = {0, 0, 7, 0, 0};
  static const uint8_t slash[5] = {1, 1, 2, 4, 4};
  static const uint8_t dot[5] = {0, 0, 0, 0, 2};
  static const uint8_t zero[5] = {7, 5, 5, 5, 7};
  static const uint8_t one[5] = {2, 6, 2, 2, 7};
  static const uint8_t two[5] = {7, 1, 7, 4, 7};
  static const uint8_t three[5] = {7, 1, 7, 1, 7};
  static const uint8_t four[5] = {5, 5, 7, 1, 1};
  static const uint8_t five[5] = {7, 4, 7, 1, 7};
  static const uint8_t six[5] = {7, 4, 7, 5, 7};
  static const uint8_t seven[5] = {7, 1, 1, 1, 1};
  static const uint8_t eight[5] = {7, 5, 7, 5, 7};
  static const uint8_t nine[5] = {7, 5, 7, 1, 7};
  static const uint8_t A[5] = {7, 5, 7, 5, 5};
  static const uint8_t B[5] = {6, 5, 6, 5, 6};
  static const uint8_t C[5] = {7, 4, 4, 4, 7};
  static const uint8_t D[5] = {6, 5, 5, 5, 6};
  static const uint8_t E[5] = {7, 4, 7, 4, 7};
  static const uint8_t F[5] = {7, 4, 7, 4, 4};
  static const uint8_t G[5] = {7, 4, 5, 5, 7};
  static const uint8_t H[5] = {5, 5, 7, 5, 5};
  static const uint8_t I[5] = {7, 2, 2, 2, 7};
  static const uint8_t J[5] = {1, 1, 1, 5, 7};
  static const uint8_t K[5] = {5, 5, 6, 5, 5};
  static const uint8_t L[5] = {4, 4, 4, 4, 7};
  static const uint8_t M[5] = {5, 7, 7, 5, 5};
  static const uint8_t N[5] = {5, 7, 7, 7, 5};
  static const uint8_t O[5] = {7, 5, 5, 5, 7};
  static const uint8_t P[5] = {7, 5, 7, 4, 4};
  static const uint8_t Q[5] = {7, 5, 5, 7, 1};
  static const uint8_t R[5] = {7, 5, 7, 6, 5};
  static const uint8_t S[5] = {7, 4, 7, 1, 7};
  static const uint8_t T[5] = {7, 2, 2, 2, 2};
  static const uint8_t U[5] = {5, 5, 5, 5, 7};
  static const uint8_t V[5] = {5, 5, 5, 5, 2};
  static const uint8_t W[5] = {5, 5, 7, 7, 5};
  static const uint8_t X[5] = {5, 5, 2, 5, 5};
  static const uint8_t Y[5] = {5, 5, 2, 2, 2};
  static const uint8_t Z[5] = {7, 1, 2, 4, 7};

  switch (c) {
    case ' ': return space;
    case ':': return colon;
    case '-': return dash;
    case '/': return slash;
    case '.': return dot;
    case '0': return zero;
    case '1': return one;
    case '2': return two;
    case '3': return three;
    case '4': return four;
    case '5': return five;
    case '6': return six;
    case '7': return seven;
    case '8': return eight;
    case '9': return nine;
    case 'A': return A;
    case 'B': return B;
    case 'C': return C;
    case 'D': return D;
    case 'E': return E;
    case 'F': return F;
    case 'G': return G;
    case 'H': return H;
    case 'I': return I;
    case 'J': return J;
    case 'K': return K;
    case 'L': return L;
    case 'M': return M;
    case 'N': return N;
    case 'O': return O;
    case 'P': return P;
    case 'Q': return Q;
    case 'R': return R;
    case 'S': return S;
    case 'T': return T;
    case 'U': return U;
    case 'V': return V;
    case 'W': return W;
    case 'X': return X;
    case 'Y': return Y;
    case 'Z': return Z;
    default: return space;
  }
}

// Mirrors drawTextBox()'s own line-splitting/wrap logic exactly (same
// break conditions) so the line count it returns matches what drawTextBox
// will actually render — used only to size the vertical-centering offset
// before the real draw pass runs.
size_t countTextLines(const String& text, int16_t maxWidth, TextWrapMode wrapMode, uint8_t charPitchPx) {
  const size_t textLen = static_cast<size_t>(text.length());
  size_t lines = 0;
  size_t i = 0;
  while (i < textLen) {
    ++lines;
    int16_t lineWidthPx = 0;
    while (i < textLen) {
      const char c = text[i];
      if (c == '\n') {
        break;
      }
      if ((lineWidthPx + static_cast<int16_t>(charPitchPx)) > maxWidth) {
        if (wrapMode == TextWrapMode::None) {
          while (i < textLen && text[i] != '\n') {
            ++i;
          }
        }
        break;
      }
      lineWidthPx += charPitchPx;
      ++i;
    }
    if (i < textLen && text[i] == '\n') {
      ++i;
    }
  }
  return lines;
}
}

void Renderer::beginFrame(FrameBuffer5& target) {
  target_ = &target;
}

void Renderer::clear(uint8_t gray) {
  if (target_) {
    target_->fill(gray);
  }
}

void Renderer::drawPixel(int16_t x, int16_t y, uint8_t gray) {
  if (target_) {
    target_->setPixel(x, y, gray);
  }
}

void Renderer::drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint8_t gray) {
  int16_t dx = abs(x1 - x0);
  int16_t sx = x0 < x1 ? 1 : -1;
  int16_t dy = -abs(y1 - y0);
  int16_t sy = y0 < y1 ? 1 : -1;
  int16_t err = dx + dy;

  while (true) {
    drawPixel(x0, y0, gray);
    if (x0 == x1 && y0 == y1) {
      break;
    }
    const int16_t e2 = static_cast<int16_t>(2 * err);
    if (e2 >= dy) {
      err += dy;
      x0 += sx;
    }
    if (e2 <= dx) {
      err += dx;
      y0 += sy;
    }
  }
}

void Renderer::drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t gray) {
  drawLine(x, y, x + w - 1, y, gray);
  drawLine(x, y, x, y + h - 1, gray);
  drawLine(x + w - 1, y, x + w - 1, y + h - 1, gray);
  drawLine(x, y + h - 1, x + w - 1, y + h - 1, gray);
}

void Renderer::fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t gray) {
  for (int16_t row = 0; row < h; ++row) {
    for (int16_t col = 0; col < w; ++col) {
      drawPixel(x + col, y + row, gray);
    }
  }
}

void Renderer::drawCircle(int16_t cx, int16_t cy, int16_t radius, uint8_t gray) {
  int16_t x = radius;
  int16_t y = 0;
  int16_t err = 0;
  while (x >= y) {
    drawPixel(cx + x, cy + y, gray);
    drawPixel(cx + y, cy + x, gray);
    drawPixel(cx - y, cy + x, gray);
    drawPixel(cx - x, cy + y, gray);
    drawPixel(cx - x, cy - y, gray);
    drawPixel(cx - y, cy - x, gray);
    drawPixel(cx + y, cy - x, gray);
    drawPixel(cx + x, cy - y, gray);
    ++y;
    if (err <= 0) {
      err += 2 * y + 1;
    }
    if (err > 0) {
      --x;
      err -= 2 * x + 1;
    }
  }
}

void Renderer::fillCircle(int16_t cx, int16_t cy, int16_t radius, uint8_t gray) {
  for (int16_t row = -radius; row <= radius; ++row) {
    const int16_t span = static_cast<int16_t>(sqrtf(static_cast<float>(radius * radius - row * row)));
    drawLine(static_cast<int16_t>(cx - span), static_cast<int16_t>(cy + row), static_cast<int16_t>(cx + span), static_cast<int16_t>(cy + row), gray);
  }
}

void Renderer::drawBitmap(int16_t x, int16_t y, const uint8_t* bitmap, int16_t w, int16_t h, uint8_t gray) {
  for (int16_t row = 0; row < h; ++row) {
    for (int16_t col = 0; col < w; ++col) {
      const size_t index = static_cast<size_t>(row) * static_cast<size_t>(w) + static_cast<size_t>(col);
      if (bitmap[index] != 0U) {
        drawPixel(x + col, y + row, gray);
      }
    }
  }
}

void Renderer::drawText(int16_t x, int16_t y, const String& text, uint8_t gray) {
  drawTextBox(x, y, TextLayoutOptions{}, text, gray);
}

void Renderer::drawTextBox(int16_t x, int16_t y, const TextLayoutOptions& options, const String& text, uint8_t gray) {
  const int16_t maxWidth = options.maxWidth > 0 ? options.maxWidth : 32767;
  const int16_t maxHeight = options.maxHeight > 0 ? options.maxHeight : 32767;
  const size_t textLen = static_cast<size_t>(text.length());

  int16_t cursorY = y;
  if (options.maxHeight > 0 && options.valign != VerticalAlign::Top) {
    const size_t lineCount = countTextLines(text, maxWidth, options.wrapMode, charPitch());
    if (lineCount > 0) {
      const int16_t totalHeight = static_cast<int16_t>(
          (lineCount - 1) * (linePitch() + options.lineSpacing) + glyphHeight());
      if (options.valign == VerticalAlign::Middle) {
        cursorY = static_cast<int16_t>(y + (options.maxHeight - totalHeight) / 2);
      } else if (options.valign == VerticalAlign::Bottom) {
        cursorY = static_cast<int16_t>(y + (options.maxHeight - totalHeight));
      }
    }
  }
  size_t i = 0;
  while (i < textLen) {
    if ((cursorY - y) >= maxHeight) {
      break;
    }

    // Collect one line's worth of characters — character-boundary wrap
    // (or truncate-at-overflow for TextWrapMode::None). Word vs character
    // wrap has never been distinguished here beyond that; both wrap at
    // whatever character hits maxWidth.
    const size_t lineStart = i;
    int16_t lineWidthPx = 0;
    while (i < textLen) {
      const char c = text[i];
      if (c == '\n') {
        break;
      }
      if ((lineWidthPx + static_cast<int16_t>(charPitch())) > maxWidth) {
        if (options.wrapMode == TextWrapMode::None) {
          // Truncate this line: skip any remaining characters up to the
          // next newline (or end) without drawing them.
          while (i < textLen && text[i] != '\n') {
            ++i;
          }
        }
        break;
      }
      lineWidthPx += charPitch();
      ++i;
    }

    // Alignment only makes sense against an explicit box width — with the
    // default (unset, effectively infinite) width this stays Left so
    // existing callers like drawText() are unaffected.
    int16_t startX = x;
    if (options.maxWidth > 0) {
      if (options.align == HorizontalAlign::Center) {
        startX = static_cast<int16_t>(x + (options.maxWidth - lineWidthPx) / 2);
      } else if (options.align == HorizontalAlign::Right) {
        startX = static_cast<int16_t>(x + (options.maxWidth - lineWidthPx));
      }
    }

    int16_t cursorX = startX;
    for (size_t j = lineStart; j < i; ++j) {
      drawChar(cursorX, cursorY, static_cast<char>(toupper(text[j])), gray);
      cursorX += charPitch();
    }

    if (i < textLen && text[i] == '\n') {
      ++i;
    }
    cursorY += linePitch() + options.lineSpacing;
  }
}

void Renderer::setFont(const Font* font) {
  font_ = font;
}

uint8_t Renderer::glyphWidth() const {
  return (font_ != nullptr && font_->isLoaded()) ? font_->glyphWidth() : kBuiltinGlyphWidth;
}

uint8_t Renderer::glyphHeight() const {
  return (font_ != nullptr && font_->isLoaded()) ? font_->glyphHeight() : kBuiltinGlyphHeight;
}

uint8_t Renderer::charPitch() const {
  return (font_ != nullptr && font_->isLoaded()) ? font_->charPitch() : kBuiltinCharPitch;
}

uint8_t Renderer::linePitch() const {
  return (font_ != nullptr && font_->isLoaded()) ? font_->linePitch() : kBuiltinLinePitch;
}

void Renderer::drawChar(int16_t x, int16_t y, char c, uint8_t gray) {
  const uint8_t* rows = nullptr;
  if (font_ != nullptr && font_->isLoaded()) {
    rows = font_->glyphRows(c);
  }
  if (rows != nullptr) {
    drawGlyphRows(x, y, rows, font_->glyphWidth(), font_->glyphHeight(), gray);
    return;
  }
  drawGlyphRows(x, y, builtinGlyphRows(c), kBuiltinGlyphWidth, kBuiltinGlyphHeight, gray);
}

void Renderer::drawGlyphRows(int16_t x, int16_t y, const uint8_t* rows, uint8_t width, uint8_t height, uint8_t gray) {
  for (int16_t row = 0; row < height; ++row) {
    for (int16_t col = 0; col < width; ++col) {
      if ((rows[row] & (1 << (width - 1 - col))) != 0) {
        drawPixel(x + col, y + row, gray);
      }
    }
  }
}
