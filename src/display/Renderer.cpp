#include "display/Renderer.h"

#include <algorithm>
#include <cmath>

namespace {
constexpr uint8_t kGlyphSpace = 4;
constexpr uint8_t kGlyphHeight = 6;

const uint8_t* glyphRowsFor(char c) {
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
  int16_t cursorX = x;
  int16_t cursorY = y;
  const int16_t maxWidth = options.maxWidth > 0 ? options.maxWidth : 32767;
  const int16_t maxHeight = options.maxHeight > 0 ? options.maxHeight : 32767;

  for (size_t i = 0; i < static_cast<size_t>(text.length()); ++i) {
    const char c = static_cast<char>(toupper(text[i]));
    if (c == '\n') {
      cursorX = x;
      cursorY += kGlyphHeight + options.lineSpacing;
      if ((cursorY - y) >= maxHeight) {
        break;
      }
      continue;
    }
    if ((cursorX - x) + static_cast<int16_t>(kGlyphSpace) > maxWidth) {
      if (options.wrapMode == TextWrapMode::None) {
        break;
      }
      cursorX = x;
      cursorY += kGlyphHeight + options.lineSpacing;
      if ((cursorY - y) >= maxHeight) {
        break;
      }
    }
    drawChar(cursorX, cursorY, c, gray);
    cursorX += kGlyphSpace;
  }
}

void Renderer::drawChar(int16_t x, int16_t y, char c, uint8_t gray) {
  drawGlyphRows(x, y, glyphRowsFor(c), gray);
}

void Renderer::drawGlyphRows(int16_t x, int16_t y, const uint8_t* rows, uint8_t gray) {
  for (int16_t row = 0; row < 5; ++row) {
    for (int16_t col = 0; col < 3; ++col) {
      if ((rows[row] & (1 << (2 - col))) != 0) {
        drawPixel(x + col, y + row, gray);
      }
    }
  }
}
