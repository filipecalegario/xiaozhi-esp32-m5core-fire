# M5Stack Core Fire 2018.2A

Board configuration for M5Stack Core Fire (version 2018.2A) running XiaoZhi AI assistant.

## Hardware Specifications

- **MCU**: ESP32-D0WDQ6 (Dual-core 240MHz)
- **Flash**: 16MB
- **PSRAM**: 8MB
- **Display**: ILI9342C 320x240 LCD (2 inch IPS)
- **Audio Input**: MEMS Microphone BSE3729 (ADC)
- **Audio Output**: 1W Speaker (PDM)
- **Buttons**: 3 physical buttons (A, B, C)
- **LED Strip**: 10x SK6812 RGB LEDs (M5GO Base)
- **Battery**: 500mAh Li-ion
- **Power Management**: IP5306 PMIC

## Pin Configuration

| Function | GPIO |
|----------|------|
| Mic (ADC) | GPIO34 (ADC1_CH6) |
| Speaker (PDM) | GPIO25 |
| Button A | GPIO39 |
| Button B | GPIO38 |
| Button C | GPIO37 |
| LED Strip | GPIO15 |
| LCD MOSI | GPIO23 |
| LCD CLK | GPIO18 |
| LCD CS | GPIO14 |
| LCD DC | GPIO27 |
| LCD RST | GPIO33 |
| LCD BL | GPIO32 |
| I2C SDA | GPIO21 |
| I2C SCL | GPIO22 |

**Note**: GPIO16 and GPIO17 are reserved for PSRAM - do not use!

## Features

- **Chat Activation**: Press Button A to toggle voice chat
- **Volume Control**: Button B (down) / Button C (up)
- **WiFi Config**: Long press Button A or press during startup
- **LED Feedback**: Visual states for listening, speaking, error, etc.
- **Battery Monitor**: Via IP5306 power management IC

## Build Instructions

```bash
# Set target to ESP32
idf.py set-target esp32

# Configure board
idf.py menuconfig
# Navigate to: Xiaozhi Assistant -> Board Type -> M5Stack Core Fire 2018.2A

# Build
idf.py build

# Flash
idf.py flash monitor
```

Or use the release script:
```bash
python scripts/release.py m5stack-core-fire
```

## Important Notes

1. **USB Cable**: Use USB-A to USB-C cable. Version 2018.2A does NOT support USB-C to USB-C.
2. **No Wake Word**: Wake word detection is disabled for ESP32 original due to performance limitations. Use Button A to activate.
3. **PSRAM Required**: This board requires PSRAM for audio buffers and display rendering.
