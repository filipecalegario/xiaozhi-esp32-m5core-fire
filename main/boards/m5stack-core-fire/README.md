# M5Stack Core Fire 2018.2A + Node Base

Board configuration for M5Stack Core Fire (version 2018.2A) with M5Stack Node Base, running XiaoZhi AI assistant.

## Hardware Specifications

### M5Stack Core Fire
- **MCU**: ESP32-D0WDQ6 (Dual-core 240MHz)
- **Flash**: 16MB
- **PSRAM**: 8MB (4MB usable due to ESP32 limitations)
- **Display**: ILI9342C 320x240 LCD (2 inch IPS)
- **Buttons**: 3 physical buttons (A, B, C)
- **Battery**: 500mAh Li-ion (in M5GO Base)
- **Power Management**: IP5306 PMIC

### M5Stack Node Base (Audio)
- **Audio Codec**: WM8978 Hi-Fi codec
- **Audio Quality**: 24-bit, up to 48kHz sample rate
- **Audio Mode**: Full-duplex I2S (simultaneous input/output)
- **Microphone**: Integrated MEMS microphone
- **Speaker Output**: Stereo output (LOUT1/ROUT1 for headphones, LOUT2/ROUT2 for speaker)
- **I2C Address**: 0x1A

## Pin Configuration

### Display (SPI)
| Function | GPIO |
|----------|------|
| LCD MOSI | GPIO23 |
| LCD CLK | GPIO18 |
| LCD CS | GPIO14 |
| LCD DC | GPIO27 |
| LCD RST | GPIO33 |
| LCD Backlight | GPIO32 |

### Audio (I2S - Node Base)
| Function | GPIO | Description |
|----------|------|-------------|
| MCLK | GPIO0 | Master clock (256x sample rate) |
| BCK | GPIO5 | Bit clock |
| WS | GPIO13 | Word select (LRCLK) |
| DOUT | GPIO2 | Data out to codec (speaker) |
| DIN | GPIO34 | Data in from codec (microphone) |

### Control
| Function | GPIO |
|----------|------|
| Button A | GPIO39 |
| Button B | GPIO38 |
| Button C | GPIO37 |
| I2C SDA | GPIO21 |
| I2C SCL | GPIO22 |

**Note**: GPIO16 and GPIO17 are reserved for PSRAM - do not use!

## Audio Configuration

### WM8978 Codec Features
The WM8978 is a high-quality audio codec that provides:
- **Full-duplex operation**: Simultaneous recording and playback
- **High-fidelity audio**: 24-bit ADC/DAC with low noise
- **Integrated microphone preamplifier**: With +20dB boost
- **Multiple outputs**: Headphone and speaker drivers
- **Flexible clocking**: Supports external MCLK or internal PLL

### Sample Rates
```c
#define AUDIO_INPUT_SAMPLE_RATE  16000   // Microphone
#define AUDIO_OUTPUT_SAMPLE_RATE 16000   // Speaker
```

Both input and output use 16kHz for full-duplex operation. The server may use 24kHz, which triggers automatic resampling (with a warning in logs).

### Key Register Configuration
| Register | Value | Purpose |
|----------|-------|---------|
| R1 (POWER1) | 0x01B | Enable MICBIAS, BIASEN, BUFIOEN, VMID=5K |
| R2 (POWER2) | 0x1BF | Enable all: outputs, boost, PGA, ADC |
| R3 (POWER3) | 0x06F | Enable LOUT2, ROUT2, mixers, DAC |
| R4 (AUDIO_IF) | 0x010 | I2S format, 16-bit |
| R43 (BEEP_CTRL) | 0x010 | Invert ROUT2 for speaker drive |
| R50/R51 (MIXER) | 0x001 | DAC to mixer routing |

## Button Functions

| Button | Action | Function |
|--------|--------|----------|
| A | Click | Toggle voice chat (start/stop listening) |
| A | Long Press | Enter WiFi configuration mode |
| B | Click | Volume down |
| B | Double Click | Send "Olá" text message |
| C | Click | Play test tone (audio test) |

## Build Instructions

### Prerequisites
- ESP-IDF v5.4+
- USB-A to USB-C cable (USB-C to USB-C NOT supported on 2018.2A)

### Build Commands
```bash
# Set target to ESP32
idf.py set-target esp32

# Configure board
idf.py menuconfig
# Navigate to: Xiaozhi Assistant -> Board Type -> M5Stack Core Fire 2018.2A

# Build
idf.py build

# Flash and monitor
idf.py flash monitor
```

Or use the release script:
```bash
python scripts/release.py m5stack-core-fire
```

## Implementation Details

### File Structure
```
main/boards/m5stack-core-fire/
├── config.h                    # Pin definitions and hardware config
├── config.json                 # Build configuration
├── m5stack_core_fire.cc        # Board initialization
├── wm8978_audio_codec.h        # WM8978 driver header
├── wm8978_audio_codec.cc       # WM8978 driver implementation
├── fire_audio_codec.h          # Legacy PDM audio (unused with Node Base)
├── fire_audio_codec.cc         # Legacy PDM audio (unused with Node Base)
└── README.md                   # This file
```

### WM8978 Driver Architecture
```
                    ┌─────────────────────────────────────┐
                    │         Wm8978AudioCodec            │
                    │  (inherits from AudioCodec)         │
                    └─────────────────────────────────────┘
                                    │
           ┌────────────────────────┼────────────────────────┐
           │                        │                        │
    ┌──────▼──────┐         ┌───────▼───────┐        ┌───────▼───────┐
    │   I2C       │         │     I2S       │        │    Audio      │
    │  Control    │         │   Duplex      │        │   Control     │
    │  (config)   │         │  (data)       │        │  (volume)     │
    └─────────────┘         └───────────────┘        └───────────────┘
           │                        │
           ▼                        ▼
    ┌─────────────┐         ┌───────────────┐
    │   WM8978    │◄────────│    ESP32      │
    │   Codec     │         │   I2S+I2C     │
    └─────────────┘         └───────────────┘
```

### Audio Data Flow
```
Recording (Microphone → Server):
  WM8978 ADC → I2S DIN → ESP32 RX → Stereo→Mono → Opus Encode → Server

Playback (Server → Speaker):
  Server → Opus Decode → Mono→Stereo → ESP32 TX → I2S DOUT → WM8978 DAC
```

### Key Implementation Notes

1. **Heap Allocation for Buffers**
   - Audio buffers use `heap_caps_malloc()` instead of stack allocation
   - Prevents stack overflow in audio tasks (stack size: 2048 bytes on ESP32)

2. **Stereo/Mono Conversion**
   - WM8978 operates in stereo mode
   - Application uses mono audio
   - Driver converts: mono→stereo (output), stereo→mono (input)

3. **MCLK on GPIO0**
   - GPIO0 is a strapping pin (boot mode selection)
   - Safe to use after boot completes
   - WM8978 requires MCLK (256x sample rate = 4.096MHz at 16kHz)

4. **I2S Full-Duplex**
   - Single I2S peripheral with both TX and RX channels
   - Same sample rate required for both directions

## Troubleshooting

### No Audio Output
1. Check Node Base connection (all pins must be seated properly)
2. Verify I2C communication: `I2C device added at address 0x1A`
3. Check MCLK generation: `MCLK=0` in logs (not MCLK=-1)
4. Verify power registers: DAC must be enabled in R3

### Stack Overflow in audio_output Task
- Symptom: Device resets when playing audio
- Cause: Large buffers on stack
- Solution: Use heap allocation (`heap_caps_malloc`)

### Device Enters Download Mode on Boot
- Symptom: `boot:0x7 (DOWNLOAD_BOOT)` in logs
- Cause: GPIO0 held low during boot
- Solution: Ensure Node Base doesn't pull GPIO0 low at power-on

### No Microphone Input
1. Check R2 register: ADC and PGA must be enabled
2. Verify boost: R47/R48 should be 0x100 for +20dB
3. Check I2S DIN connection (GPIO34)

### Distorted Audio
- Warning: `Server sample rate 24000 does not match device output sample rate 16000`
- This is normal - resampling occurs automatically
- For better quality, both rates should match

## Hardware Notes

### Why Node Base Instead of M5GO Base?
The M5GO Base uses PDM output and ADC input, which limits audio quality and doesn't support full-duplex. The Node Base with WM8978 codec provides:
- Full-duplex I2S audio
- Higher quality (24-bit codec vs 12-bit ADC)
- Lower latency
- Better noise performance

### Power Considerations
- Node Base: Powered through M-Bus connector
- Battery: Located in M5GO Base bottom
- If using Node Base, battery charging requires M5GO Base or external power

### M5GO Base Compatibility Issues
Some M5GO Base units may have hardware issues causing the Fire to shut down when connected. Symptoms:
- Fire powers off when M5GO Base is fully inserted
- Works when only upper pins of connector touch
- Indicates possible short circuit in M5GO Base

## References

- [M5Stack Node Base Documentation](https://docs.m5stack.com/en/base/node_base)
- [WM8978 Datasheet](https://www.mouser.com/datasheet/2/76/WM8978_v4.5-1141768.pdf)
- [ESP-IDF I2S Driver](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/i2s.html)
- [wm8978-esp32 Arduino Library](https://github.com/CelliesProjects/wm8978-esp32)

## Version History

| Date | Changes |
|------|---------|
| 2025-12-25 | Added WM8978 Node Base support, full-duplex I2S audio |
| 2024-xx-xx | Initial M5Stack Core Fire support with PDM audio |
