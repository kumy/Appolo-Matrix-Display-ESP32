#pragma once

#include <Arduino.h>

struct DisplayConfig {
  uint16_t width = 80;
  uint16_t height = 16;
  uint8_t bitDepth = 4;
  uint8_t rowAddressBits = 4;
  uint32_t spiHz = 20000000;
  uint8_t pinMosi = 23;
  uint8_t pinClk = 18;
  uint8_t pinLatch = 5;
  uint8_t pinEnable = 26;
  uint8_t pinRow[4] = {13, 12, 14, 27};
  bool enableActiveLow = true;
};
