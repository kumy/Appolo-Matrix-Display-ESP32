#include "pages/TextPage.h"

#include "display/Renderer.h"

TextPage::TextPage(String message) : message_(message) {}

void TextPage::update(uint32_t nowMs) {
  nowMs_ = nowMs;
}

void TextPage::draw(Renderer& renderer) {
  renderer.clear(0);
  TextLayoutOptions options;
  options.maxWidth = 80;
  options.maxHeight = 16;
  options.lineSpacing = 1;
  renderer.drawTextBox(0, 0, options, message_, 15);
  (void)nowMs_;
}
