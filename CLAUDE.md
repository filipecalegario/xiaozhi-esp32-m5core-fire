# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

XiaoZhi is an MCP-based AI voice chatbot for ESP32. It features offline wake word detection (ESP-SR), streaming ASR/LLM/TTS, and supports 110+ hardware boards. The project uses ESP-IDF 5.4+ with LVGL for displays.

## Build Commands

```bash
# Set target chip (first time or when changing chips)
idf.py set-target esp32s3  # or esp32, esp32c3, esp32c6, esp32p4

# Configure board type
idf.py menuconfig  # Navigate: Xiaozhi Assistant -> Board Type

# Build
idf.py build

# Flash and monitor
idf.py flash monitor

# Full clean (needed when switching boards/targets)
idf.py fullclean

# Automated build for specific board (reads config.json from board directory)
python scripts/release.py <board-directory-name>
```

## Architecture

### Entry Point and Main Loop
- `main/main.cc`: Entry point calls `Application::Initialize()` then `Application::Run()`
- `main/application.cc`: Event-driven main loop using FreeRTOS EventGroup, handles state transitions

### Device State Machine (`main/device_state.h`)
States: `Starting` → `WifiConfiguring` → `Idle` → `Connecting` → `Listening` → `Speaking` → back to `Idle`

### Audio Pipeline (`main/audio/`)
```
Mic → AudioInputTask → WakeWord/AFE → Opus Encoder → Protocol → Server
Server → Protocol → Decode Queue → Opus Decoder → AudioOutputTask → Speaker
```

Key classes:
- `AudioService`: Orchestrates audio I/O with FreeRTOS tasks
- `audio/codecs/`: ES8311, ES8374, ES8388, ES8389 codec implementations
- `audio/processors/`: AFE (Audio Front End) and wake word detection
- `audio/wake_words/`: ESP-SR offline wake word models

### Board System (`main/boards/`)
Each board has its own directory containing:
- `config.h`: GPIO pin definitions (I2S, I2C, SPI, display, buttons)
- `config.json`: Target chip, flash size, partition table, sdkconfig overrides
- `*_board.cc`: Hardware initialization, inherits from `WifiBoard` or `Ml307Board`

Board selection is done via Kconfig (`CONFIG_BOARD_TYPE_*` flags) and CMakeLists.txt maps these to board directories.

Common utilities in `main/boards/common/`: button handling, backlight, battery monitor, WiFi/4G networking, camera support.

### Display System (`main/display/`)
Abstract `Display` interface with implementations: `LcdDisplay`, `OledDisplay`, `LVGLDisplay`, `EmoteDisplay`

### Communication Protocols (`main/protocols/`)
Abstract `Protocol` class with `WebsocketProtocol` and `MqttProtocol` implementations. Binary protocol v2/v3 for efficient audio streaming.

### Assets and Languages (`main/assets/`)
28+ language support. Assets (wake words, fonts, emojis) are stored in a separate partition and can be customized via [xiaozhi-assets-generator](https://github.com/78/xiaozhi-assets-generator).

## Creating a New Board

1. Create directory: `main/boards/my-board/`
2. Add `config.h` with GPIO definitions
3. Add `config.json` with target and sdkconfig overrides
4. Create `my_board.cc` inheriting from `WifiBoard` or `Ml307Board`
5. Add to `main/Kconfig.projbuild` (BOARD_TYPE choice)
6. Add to `main/CMakeLists.txt` (set BOARD_TYPE, fonts)
7. Use `DECLARE_BOARD(MyBoard)` macro to register

## Key Configuration Files

| File | Purpose |
|------|---------|
| `sdkconfig.defaults` | Global ESP-IDF settings (LVGL, partitions, optimization) |
| `main/Kconfig.projbuild` | Language, assets, board selection |
| `main/idf_component.yml` | ESP-IDF component dependencies |
| `partitions/v2/*.csv` | Flash partition layouts (4MB, 8MB, 16MB) |
| `main/boards/<board>/config.json` | Per-board build configuration |

## Code Style

Uses Google C++ code style. Source files use `.cc` extension for C++.

## Version Notes

- Current version: v2 (incompatible partition table with v1)
- v1 branch maintained until February 2026
- OTA upgrades between v1 and v2 are not supported
