# matrix-display-2026

ESP32 firmware for an 80x16 monochrome LED dot-matrix panel that produces
real grayscale in software (bit-angle modulation, not a 1-bit on/off
display), a page-based content system, and a self-hosted web control
panel — no cloud service, no app, just a browser pointed at the device's
IP address.

<p align="center"><em>10x 8x8 HUB08-style modules, driven by a single ESP32.</em></p>

## Backstory

The panel this firmware drives is a salvaged sign board labelled
**Appolo**, rescued from the trash. Its original controller board was
reverse-engineered from scratch (no documentation existed) to figure out
the row/column shift-register wiring and timing, then its original MCU was
removed and replaced with an ESP32 — the rest of the electronics,
including the shift registers actually driving the LEDs, were left
untouched. This repository is the resulting firmware, written for the
ESP32 against that reverse-engineered interface.

## Features

- **Real grayscale on 1-bit hardware** — 5-bit software Bit Angle
  Modulation (32 levels), ~70Hz+ refresh, tuned to stay flicker-free across
  the full brightness range. See [HARDWARE.md](HARDWARE.md) for how.
- **Live brightness & palette control** — gamma-corrected brightness and
  an independently adjustable gray-step count (2-32, a runtime "posterize"
  control), both adjustable from the web UI without reflashing.
- **Selectable pages**, each with its own live-editable settings:
  - **Demo** — 20 built-in attract-mode scenes: Snake, Tetris, Pong,
    Fireworks, Space Invaders, a Mario-style scroller, primitives,
    gradients, a marquee, and more. Auto-rotates, or pin it to one scene.
  - **Clock** — digital and/or analog, time-only / date-only / alternating
    display modes with a configurable interval, blinking colon, and
    alignment control.
  - **Text** — a custom message with horizontal/vertical alignment, fine
    pixel-offset positioning, marquee scrolling, and appear effects.
  - **Countdown** — counts down to any target date/time; both the
    displayed granularity (years -> months -> ... -> seconds) and the font
    size scale automatically as the target approaches.
  - **Diagnostics** — render/scan timing stats drawn directly on the panel.
  - **Death Dates** — a slideshow of notable historical death dates.
- **Multiple bitmap fonts** (3x5 up to a full-height 16px LED-display
  style), stored on a LittleFS partition and hot-swappable from the UI —
  see `data/fonts/` and its `generate_fonts.py` generator.
- **Self-hosted web control panel** — a small Bootstrap page served
  straight from the device's flash, backed by a JSON REST API
  (`/api/status`, `/api/settings`). No external server involved.
- **WiFi provisioning** — connects to a configured network, and falls back
  to its own access point (with a `/wifi` setup form) if it can't.
- **NTP time sync** with configurable server and UTC offset.
- **Settings persist to flash (NVS)** immediately on every change — no
  explicit "save" step, safe against power loss.

## Hardware

See **[HARDWARE.md](HARDWARE.md)** for the full pin map, timing, and wiring
details. In short: an ESP32 dev board driving an 80x16 (10x 8x8 module)
HUB08-style shift-register LED panel over hardware SPI, 4 GPIO row-address
lines, a latch line, and an output-enable line.

## Getting started

### Requirements

- [PlatformIO](https://platformio.org/) (CLI, or the VS Code extension)
- An ESP32 dev board, wired to the panel as described in
  [HARDWARE.md](HARDWARE.md)
- A USB cable

### Build and flash

```sh
# Firmware
pio run --target upload --upload-port /dev/ttyUSB0

# Filesystem image (web UI + fonts) — needed once, and again whenever
# anything under data/ changes
pio run --target uploadfs --upload-port /dev/ttyUSB0
```

### First boot

With no saved WiFi credentials, the device starts its own access point.
Connect to it and browse to `http://192.168.4.1/wifi` to enter your
network's SSID/password. Once it joins your network, find its IP (your
router's DHCP client list, or the serial monitor at 115200 baud) and open
that address in a browser for the control panel.

## Web UI & API

| Route | Method | Purpose |
| --- | --- | --- |
| `/` | GET | Control panel — brightness, palette, animation speed, font, active page and its settings |
| `/wifi` | GET/POST | WiFi provisioning form |
| `/api/status` | GET | JSON snapshot of live state (network, clock, per-page settings, render/scan diagnostics) |
| `/api/settings` | POST | JSON partial update — persists to flash and applies immediately |

## Project layout

```
include/, src/        firmware source (PlatformIO layout: headers in include/, matching .cpp in src/)
  core/                Application orchestration, the Page interface, runtime stats
  display/             DisplayDriver (the BAM scan engine), Renderer, FrameBuffer, Font loader, gray-level palette
  network/             WiFi manager, HTTP server/API, MQTT and OTA scaffolding
  pages/               the selectable Page implementations (Demo, Clock, Text, Countdown, Diagnostics, Death Dates)
  storage/             Settings, backed by NVS flash
  time/                ClockService (NTP, with a millis()-based fallback before sync)
data/                  LittleFS filesystem image contents: the web UI (index.html, wifi.html) and .font files
data/fonts/generate_fonts.py   generator script for the shipped .font binary files
```

## Known limitations

- MQTT and OTA update support are scaffolded (`network/MqttClient`,
  `network/OtaService`) but not yet wired up to anything functional — open
  items if you want to pick them up.
- Text rendering only covers space/colon/dash/slash/dot/0-9/A-Z; other
  characters fall back to a blank glyph.

## License

MIT — see [LICENSE](LICENSE).
