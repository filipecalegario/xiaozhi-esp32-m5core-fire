# M5Stack Core Fire + Node Base - XiaoZhi Implementation

## Overview

This document describes the XiaoZhi AI assistant implementation for **M5Stack Core Fire (2018.2A)** with **M5Stack Node Base** (WM8978 audio codec).

The Node Base provides high-quality full-duplex I2S audio, enabling simultaneous voice input (microphone) and output (speaker) - essential for a responsive AI voice assistant.

## Hardware Configuration

### M5Stack Core Fire
| Component | Specification |
|-----------|---------------|
| MCU | ESP32-D0WDQ6 (Dual-core 240MHz) |
| Flash | 16MB |
| PSRAM | 8MB (4MB usable due to ESP32 limitations) |
| Display | ILI9342C 320x240 IPS LCD |
| Buttons | 3 physical buttons (A, B, C) |
| I2C | GPIO21 (SDA), GPIO22 (SCL) |

### M5Stack Node Base (Audio)
| Component | Specification |
|-----------|---------------|
| Audio Codec | WM8978 Hi-Fi codec |
| I2C Address | 0x1A |
| Audio Quality | 24-bit ADC/DAC, up to 48kHz |
| Audio Mode | Full-duplex I2S |
| Microphone | Integrated MEMS microphone |
| Speaker Output | Stereo (LOUT2/ROUT2) |

## Pin Configuration

### I2S Audio (Node Base)
| Function | GPIO | Description |
|----------|------|-------------|
| MCLK | GPIO0 | Master clock (256x sample rate = 4.096MHz @ 16kHz) |
| BCK | GPIO5 | Bit clock |
| WS | GPIO13 | Word select (LRCLK) |
| DOUT | GPIO2 | Data out to codec (speaker) |
| DIN | GPIO34 | Data in from codec (microphone) |

### Display (SPI)
| Function | GPIO |
|----------|------|
| MOSI | GPIO23 |
| CLK | GPIO18 |
| CS | GPIO14 |
| DC | GPIO27 |
| RST | GPIO33 |
| Backlight | GPIO32 |

### Buttons
| Button | GPIO | Function |
|--------|------|----------|
| A | GPIO39 | Toggle voice chat / Long press: WiFi config |
| B | GPIO38 | Volume down / Double click: Send "Ola" |
| C | GPIO37 | Volume up |

## WM8978 Audio Codec Driver

### Key Features

1. **Full-Duplex I2S Operation**
   - Simultaneous recording and playback
   - Single I2S peripheral with shared clock for TX and RX
   - Sample rate: 16kHz (configurable)

2. **Pre-allocated Audio Buffers**
   - Avoids heap fragmentation during streaming
   - DMA-capable memory allocation
   - Buffer size: 1024 stereo samples

3. **Mono-Stereo Conversion**
   - Application uses mono audio
   - WM8978 operates in stereo
   - Automatic conversion in Read/Write functions

### Register Configuration

| Register | Value | Purpose |
|----------|-------|---------|
| R1 (POWER1) | 0x01B | MICBIAS, BIASEN, BUFIOEN, VMID=5K |
| R2 (POWER2) | 0x1BF | All outputs, boost, PGA, ADC enabled |
| R3 (POWER3) | 0x06F | LOUT2, ROUT2, mixers, DAC enabled |
| R4 (AUDIO_IF) | 0x010 | I2S format, 16-bit |
| R10 (DAC_CTRL) | 0x008 | Soft mute disabled, 128x oversampling |
| R11-R12 (DAC_VOL) | 0x1FF | Max DAC volume |
| R14 (ADC_CTRL) | 0x108 | 128x oversampling, HPF at 3.7Hz |
| R15-R16 (ADC_VOL) | 0x1FF | Max ADC volume |
| R43 (BEEP_CTRL) | 0x010 | ROUT2 inverted for speaker drive |
| R44 (INPUT_CTRL) | 0x033 | L2/R2 connected to PGA |
| R45-R46 (INP_PGA) | 0x13F | Input PGA unmuted, max gain |
| R47-R48 (ADC_BOOST) | 0x100 | +20dB microphone boost |
| R50-R51 (MIXER) | 0x001 | DAC to mixer routing |
| R52-R55 (OUT_VOL) | 0x13F | Max output volumes |

## Critical Implementation Details

### Full-Duplex Clock Sharing

**Problem**: In full-duplex mode, TX and RX channels share the same I2S clock. Disabling one channel stops the clock and breaks the other channel.

**Solution**:
- Enable both I2S channels once at startup
- Never call `i2s_channel_disable()` during operation
- Track enable/disable state logically without touching I2S hardware
- Only control PA (power amplifier) GPIO for muting

```cpp
// In EnableInput/EnableOutput - DO NOT disable I2S channels
// Just track state and control PA
void EnableOutput(bool enable) {
    if (enable) {
        gpio_set_level(pa_pin_, 1);  // Enable PA
    } else {
        gpio_set_level(pa_pin_, 0);  // Disable PA only
    }
    // I2S channel stays running
}
```

### DMA Buffer Configuration

```cpp
i2s_chan_config_t chan_cfg = {
    .dma_desc_num = 12,    // Number of DMA descriptors
    .dma_frame_num = 480,  // Frames per descriptor
    // Total: 12 * 480 = 5760 frames = ~360ms buffer at 16kHz
};
```

Larger DMA buffers prevent underruns during long audio streaming.

### I2S Write Timeout

Use a reasonable timeout instead of `portMAX_DELAY` to detect issues:

```cpp
esp_err_t ret = i2s_channel_write(tx_handle_, buffer, size,
                                  &bytes_written, pdMS_TO_TICKS(500));
if (ret != ESP_OK) {
    ESP_LOGE(TAG, "I2S write failed: %s", esp_err_to_name(ret));
}
```

## Troubleshooting

### Audio Stops During Long Playback

**Symptom**: Audio plays for ~20-30 seconds then stops with `ESP_ERR_TIMEOUT` errors.

**Cause**: Microphone disabled → I2S RX channel disabled → Shared clock stops → TX fails.

**Solution**: Keep both I2S channels always enabled (see Full-Duplex Clock Sharing above).

### No Audio Output

1. Check Node Base connection (all pins seated properly)
2. Verify I2C: Look for `I2C device added at address 0x1A` in logs
3. Check MCLK: Should show `MCLK=0` (not -1)
4. Verify power registers are enabled

### Stack Overflow in audio_output Task

**Cause**: Large buffers allocated on stack.

**Solution**: Use `heap_caps_malloc()` with pre-allocated buffers.

### Device Enters Download Mode on Boot

**Cause**: GPIO0 (MCLK) held low during boot.

**Solution**: Ensure Node Base doesn't pull GPIO0 low at power-on.

### Choppy/Distorted Audio

**Warning**: `Server sample rate 24000 does not match device output sample rate 16000`

This is normal - resampling occurs automatically. For best quality, server and device rates should match.

## Build Instructions

```bash
# Set target
idf.py set-target esp32

# Configure board
idf.py menuconfig
# Navigate to: Xiaozhi Assistant -> Board Type -> M5Stack Core Fire 2018.2A

# Build and flash
idf.py build
idf.py flash monitor
```

## File Structure

```
main/boards/m5stack-core-fire/
├── config.h              # Pin definitions
├── config.json           # Build configuration
├── m5stack_core_fire.cc  # Board initialization
├── wm8978_audio_codec.h  # WM8978 driver header
├── wm8978_audio_codec.cc # WM8978 driver implementation
└── README.md             # Board-specific README
```

## Audio Data Flow

```
Recording (Microphone → Server):
  WM8978 ADC → I2S DIN → ESP32 RX → Stereo→Mono → Opus Encode → Server

Playback (Server → Speaker):
  Server → Opus Decode → Resample 24k→16k → Mono→Stereo → ESP32 TX → I2S DOUT → WM8978 DAC
```

## Version History

| Date | Changes |
|------|---------|
| 2025-12-25 | Fixed full-duplex audio streaming (keep I2S channels always enabled) |
| 2025-12-25 | Added DAC/ADC volume registers, fixed input PGA configuration |
| 2025-12-25 | Pre-allocated DMA buffers to prevent heap fragmentation |
| 2025-12-25 | Initial WM8978 Node Base support with full-duplex I2S |

## References

- [M5Stack Node Base Documentation](https://docs.m5stack.com/en/base/node_base)
- [WM8978 Datasheet](https://www.mouser.com/datasheet/2/76/WM8978_v4.5-1141768.pdf)
- [ESP-IDF I2S Driver](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/i2s.html)
- [M5Stack Core Fire Documentation](https://docs.m5stack.com/en/core/fire)
