#include "display/Font.h"

#include <FS.h>
#include <LittleFS.h>

#include "util/Logger.h"

Font::~Font() {
  delete[] glyphData_;
}

bool Font::loadFromFile(const String& path) {
  delete[] glyphData_;
  glyphData_ = nullptr;

  File file = LittleFS.open(path, "r");
  if (!file || file.isDirectory()) {
    Logger::info(String("Font: could not open ") + path);
    return false;
  }

  uint8_t header[8];
  if (file.read(header, sizeof(header)) != static_cast<int>(sizeof(header))) {
    Logger::info(String("Font: ") + path + " too short for header");
    file.close();
    return false;
  }
  if (header[0] != 0x46U || header[1] != 1U) {
    Logger::info(String("Font: ") + path + " bad magic/version");
    file.close();
    return false;
  }

  glyphWidth_ = header[2];
  glyphHeight_ = header[3];
  charPitch_ = header[4];
  linePitch_ = header[5];
  firstChar_ = header[6];
  charCount_ = header[7];

  if (glyphWidth_ == 0U || glyphHeight_ == 0U || charCount_ == 0U) {
    Logger::info(String("Font: ") + path + " zero-sized glyph/charCount");
    file.close();
    return false;
  }

  const size_t dataSize = static_cast<size_t>(glyphHeight_) * static_cast<size_t>(charCount_);
  glyphData_ = new uint8_t[dataSize];
  const int readBytes = file.read(glyphData_, dataSize);
  file.close();

  if (readBytes != static_cast<int>(dataSize)) {
    Logger::info(String("Font: ") + path + " truncated glyph data");
    delete[] glyphData_;
    glyphData_ = nullptr;
    return false;
  }

  Logger::info(String("Font: loaded ") + path + " (" + String(glyphWidth_) + "x" + String(glyphHeight_) + ", " + String(charCount_) + " glyphs)");
  return true;
}

const uint8_t* Font::glyphRows(char c) const {
  if (glyphData_ == nullptr) {
    return nullptr;
  }
  const uint8_t code = static_cast<uint8_t>(c);
  if (code < firstChar_ || code >= (static_cast<uint16_t>(firstChar_) + charCount_)) {
    return nullptr;
  }
  const size_t index = static_cast<size_t>(code - firstChar_) * static_cast<size_t>(glyphHeight_);
  return glyphData_ + index;
}
