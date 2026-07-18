# Hardware

This describes the physical panel and wiring the current firmware
(`src/display/DisplayDriver.cpp`, configured via `include/display/DisplayConfig.h`)
drives. It targets a single board: an ESP32 dev module (PlatformIO
`board = esp32dev`) driving one HUB08-style row-multiplexed LED matrix panel.

## Panel geometry

- Canvas: `80 x 16` pixels
- Physical layout: `10` modules wide x `2` modules tall, `8 x 8` pixels per module
- Interface: single serial column-shift-register chain + 4-bit row address +
  latch + output-enable (standard HUB08-family wiring)
- The panel itself is electrically 1-bit (on/off) per pixel — grayscale is
  produced by the firmware, not the hardware. See
  [Grayscale: bit-angle modulation](#grayscale-bit-angle-modulation) below.

## ESP32 pin map

Defaults live in `DisplayConfig` (`include/display/DisplayConfig.h`) and can
be overridden per-instance if you wire a board differently.

| Signal | ESP32 GPIO | Purpose |
| --- | ---: | --- |
| `pinMosi` | `23` | SPI MOSI — serial column data |
| `pinClk` | `18` | SPI clock |
| `pinLatch` | `5` | Latches shifted column data into the panel's output registers |
| `pinRow[0]` | `13` | Row address bit 0 |
| `pinRow[1]` | `12` | Row address bit 1 |
| `pinRow[2]` | `14` | Row address bit 2 |
| `pinRow[3]` | `27` | Row address bit 3 |
| `pinEnable` | `26` | Output enable / global blanking (active low) |

Hardware SPI is used via the ESP32's default `SPI` bus pins (`MOSI=23`,
`SCK=18` above); no MISO/CS connection is needed since the panel is
write-only.

## Row addressing

Each scan step drives the row-select pins with the row's 4-bit index before
shifting that row's column data:

| Row | `pinRow[3] pinRow[2] pinRow[1] pinRow[0]` |
| ---: | --- |
| 0 | `0000` |
| 1 | `0001` |
| 2 | `0010` |
| 3 | `0011` |
| 4 | `0100` |
| 5 | `0101` |
| 6 | `0110` |
| 7 | `0111` |
| 8 | `1000` |
| 9 | `1001` |
| 10 | `1010` |
| 11 | `1011` |
| 12 | `1100` |
| 13 | `1101` |
| 14 | `1110` |
| 15 | `1111` |

## Column data path

For each row, the firmware shifts out `10` bytes over hardware SPI
(`80` columns / `8` bits per byte):

- SPI mode: `MODE0`
- Bit order: `MSBFIRST`
- Clock: `20 MHz` (`DisplayConfig::spiHz`)

A plain blocking `SPIClass::transfer()` call is used per row rather than the
ESP-IDF `spi_master` transaction API — at this transfer size (~10 bytes) the
fixed per-transaction overhead of `spi_master` (tens of microseconds)
dwarfed the actual ~4-5us wire time and would have forced a much longer
minimum scan slot.

## Latch and output enable

- After a row's column bytes are shifted out, `pinLatch` is pulsed
  (driven high then low) to commit that data into the panel's output
  latches.
- `pinEnable` is active low (`DisplayConfig::enableActiveLow = true`):
  driving it low lights the currently-latched row, driving it high blanks
  all rows. The firmware blanks output before changing row address/shifting
  new data, and re-enables it only once the new row is fully latched, to
  avoid ghosting between rows.

## Grayscale: bit-angle modulation

The panel has no native grayscale, so the firmware produces it entirely in
software using 5-bit Bit Angle Modulation (BAM):

- Every pixel is stored as a 5-bit raw value (0-31, `FrameBuffer5`).
- Each row is scanned once per bit-plane, 5 planes per full refresh, with
  each plane's on-time doubling: `24us x 2^planeIndex` (24, 48, 96, 192,
  384us). Over one full 16-row refresh this yields roughly ~70-72Hz on
  actual hardware — enough headroom above the flicker-fusion threshold even
  during fast on-panel animation.
- Global brightness is applied in software (scaling+requantizing each
  pixel's raw value with a gamma curve, `kBrightnessGamma = 2.5`) rather
  than by changing scan timing — this keeps the BAM slot durations fixed
  and flicker-free at every brightness level. See the class-level comment
  in `src/display/DisplayDriver.cpp` for the two timing-based designs that
  were tried and rejected before landing on this approach.
- The number of distinct gray steps actually used (2-32) is independently
  and live-adjustable from the web UI (`paletteLevelCount` — a runtime
  "posterize" control, separate from brightness).

## Task/core placement

- The BAM scan loop (row addressing, SPI transfer, latch/enable toggling)
  runs on a dedicated FreeRTOS task pinned to CPU core 1, woken by a
  same-core ESP-IDF `gptimer` for minimal wake-up jitter.
- All other firmware logic (page rendering, WiFi, the HTTP API, NTP,
  Settings) runs on a separate task pinned to core 0, alongside the
  WiFi/lwIP/AsyncTCP stack.
- This split is deliberate: letting scan timing and networking share a core
  caused visible starvation in one direction or the other during
  development (see `include/display/DisplayDriver.h` and
  `include/core/Application.h` for the full history).

## Build target

- Board: ESP32 dev module (`platform = espressif32`, `board = esp32dev`)
- Framework: Arduino, via PlatformIO
- Flash: 4MB, partitioned via `min_spiffs.csv` — two ~1.875MB OTA app
  slots plus a small (~128KB) LittleFS partition that holds the web UI and
  the bitmap font files (see `data/`).
