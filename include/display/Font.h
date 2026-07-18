#pragma once

#include <Arduino.h>

// Loads a .font binary file from LittleFS (see data/fonts/*.font and the
// generator at data/fonts/ — format documented there) into a heap buffer,
// and answers per-glyph bitmap lookups for Renderer. Deliberately NOT the
// only font source: Renderer keeps a small compiled-in fallback (the
// original hand-authored glyph table) so text rendering still works even
// if LittleFS fails to mount or a font file is missing/corrupt — a Font
// that failed to load (isLoaded()==false) just means Renderer falls back
// to that default rather than the whole device losing all text.
class Font {
public:
  ~Font();

  bool loadFromFile(const String& path);
  bool isLoaded() const { return glyphData_ != nullptr; }

  uint8_t glyphWidth() const { return glyphWidth_; }
  uint8_t glyphHeight() const { return glyphHeight_; }
  uint8_t charPitch() const { return charPitch_; }
  uint8_t linePitch() const { return linePitch_; }

  // Returns a pointer to glyphHeight() bytes (row-major, MSB = leftmost
  // column) for the given character, or nullptr if this Font isn't
  // loaded or the character is outside its range — callers must fall
  // back to the built-in font in either case.
  const uint8_t* glyphRows(char c) const;

private:
  uint8_t* glyphData_ = nullptr;
  uint8_t glyphWidth_ = 0;
  uint8_t glyphHeight_ = 0;
  uint8_t charPitch_ = 0;
  uint8_t linePitch_ = 0;
  uint8_t firstChar_ = 0;
  uint8_t charCount_ = 0;
};
