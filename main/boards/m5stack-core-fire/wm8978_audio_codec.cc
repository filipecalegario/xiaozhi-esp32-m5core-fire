#include "wm8978_audio_codec.h"

#include <esp_log.h>
#include <esp_heap_caps.h>
#include <string.h>
#include "settings.h"

static const char TAG[] = "Wm8978AudioCodec";

// WM8978 Register Definitions
#define WM8978_RESET            0x00
#define WM8978_POWER1           0x01
#define WM8978_POWER2           0x02
#define WM8978_POWER3           0x03
#define WM8978_AUDIO_IF         0x04
#define WM8978_COMPANDING       0x05
#define WM8978_CLK_GEN          0x06
#define WM8978_ADD_CTRL         0x07
#define WM8978_GPIO             0x08
#define WM8978_JACK_DET1        0x09
#define WM8978_DAC_CTRL         0x0A
#define WM8978_LEFT_DAC_VOL     0x0B
#define WM8978_RIGHT_DAC_VOL    0x0C
#define WM8978_JACK_DET2        0x0D
#define WM8978_ADC_CTRL         0x0E
#define WM8978_LEFT_ADC_VOL     0x0F
#define WM8978_RIGHT_ADC_VOL    0x10
#define WM8978_EQ1              0x12
#define WM8978_EQ2              0x13
#define WM8978_EQ3              0x14
#define WM8978_EQ4              0x15
#define WM8978_EQ5              0x16
#define WM8978_DAC_LIMITER1     0x18
#define WM8978_DAC_LIMITER2     0x19
#define WM8978_NOTCH1           0x1B
#define WM8978_NOTCH2           0x1C
#define WM8978_NOTCH3           0x1D
#define WM8978_NOTCH4           0x1E
#define WM8978_ALC1             0x20
#define WM8978_ALC2             0x21
#define WM8978_ALC3             0x22
#define WM8978_NOISE_GATE       0x23
#define WM8978_PLL_N            0x24
#define WM8978_PLL_K1           0x25
#define WM8978_PLL_K2           0x26
#define WM8978_PLL_K3           0x27
#define WM8978_3D_CTRL          0x29
#define WM8978_BEEP_CTRL        0x2B
#define WM8978_INPUT_CTRL       0x2C
#define WM8978_LEFT_INP_PGA     0x2D
#define WM8978_RIGHT_INP_PGA    0x2E
#define WM8978_LEFT_ADC_BOOST   0x2F
#define WM8978_RIGHT_ADC_BOOST  0x30
#define WM8978_OUTPUT_CTRL      0x31
#define WM8978_LEFT_MIXER       0x32
#define WM8978_RIGHT_MIXER      0x33
#define WM8978_LOUT1_VOL        0x34
#define WM8978_ROUT1_VOL        0x35
#define WM8978_LOUT2_VOL        0x36
#define WM8978_ROUT2_VOL        0x37
#define WM8978_OUT3_MIXER       0x38
#define WM8978_OUT4_MIXER       0x39

Wm8978AudioCodec::Wm8978AudioCodec(i2c_master_bus_handle_t i2c_bus,
                                   int input_sample_rate, int output_sample_rate,
                                   gpio_num_t mclk, gpio_num_t bclk, gpio_num_t ws,
                                   gpio_num_t dout, gpio_num_t din,
                                   gpio_num_t pa_pin) {
    duplex_ = true;
    input_reference_ = false;
    input_channels_ = 1;
    input_sample_rate_ = input_sample_rate;
    output_sample_rate_ = output_sample_rate;
    pa_pin_ = pa_pin;
    i2c_bus_ = i2c_bus;

    ESP_LOGI(TAG, "Initializing WM8978 Audio Codec...");
    ESP_LOGI(TAG, "  Sample rates: in=%d, out=%d", input_sample_rate, output_sample_rate);
    ESP_LOGI(TAG, "  Pins: MCLK=%d, BCK=%d, WS=%d, DOUT=%d, DIN=%d",
             mclk, bclk, ws, dout, din);

    // Initialize register cache
    memset(reg_cache_, 0, sizeof(reg_cache_));

    // Setup I2C device for WM8978
    i2c_device_config_t i2c_dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = WM8978_ADDR,
        .scl_speed_hz = 100000,
        .scl_wait_us = 0,
        .flags = {
            .disable_ack_check = 0,
        },
    };
    esp_err_t ret = i2c_master_bus_add_device(i2c_bus_, &i2c_dev_cfg, &i2c_dev_);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add I2C device: %s", esp_err_to_name(ret));
        i2c_dev_ = nullptr;
    } else {
        ESP_LOGI(TAG, "I2C device added at address 0x%02X", WM8978_ADDR);
    }

    // Initialize WM8978 codec (only if I2C device added successfully)
    bool codec_ok = false;
    if (i2c_dev_) {
        codec_ok = InitCodec();
        if (!codec_ok) {
            ESP_LOGW(TAG, "WM8978 codec init failed - Node Base may not be connected");
        }
    }

    // Create I2S channels (even if codec failed, for testing)
    CreateDuplexChannels(mclk, bclk, ws, dout, din);

    // Pre-allocate stereo buffers to avoid heap fragmentation during streaming
    // Using DMA-capable memory for better performance
    tx_stereo_buffer_ = (int16_t*)heap_caps_malloc(
        STEREO_BUFFER_SAMPLES * 2 * sizeof(int16_t), MALLOC_CAP_8BIT | MALLOC_CAP_DMA);
    rx_stereo_buffer_ = (int16_t*)heap_caps_malloc(
        STEREO_BUFFER_SAMPLES * 2 * sizeof(int16_t), MALLOC_CAP_8BIT | MALLOC_CAP_DMA);

    if (!tx_stereo_buffer_ || !rx_stereo_buffer_) {
        ESP_LOGE(TAG, "Failed to allocate stereo buffers!");
    } else {
        ESP_LOGI(TAG, "Pre-allocated stereo buffers: %d samples each", STEREO_BUFFER_SAMPLES);
    }

    // Setup PA pin if specified
    if (pa_pin_ != GPIO_NUM_NC) {
        gpio_config_t io_conf = {};
        io_conf.pin_bit_mask = (1ULL << pa_pin_);
        io_conf.mode = GPIO_MODE_OUTPUT;
        io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
        io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
        io_conf.intr_type = GPIO_INTR_DISABLE;
        gpio_config(&io_conf);
        gpio_set_level(pa_pin_, 0);
    }

    if (codec_ok) {
        ESP_LOGI(TAG, "WM8978 Audio Codec initialized successfully");
    } else {
        ESP_LOGW(TAG, "WM8978 Audio Codec initialized with warnings");
    }
}

Wm8978AudioCodec::~Wm8978AudioCodec() {
    if (tx_handle_) {
        i2s_channel_disable(tx_handle_);
        i2s_del_channel(tx_handle_);
    }
    if (rx_handle_) {
        i2s_channel_disable(rx_handle_);
        i2s_del_channel(rx_handle_);
    }
    if (i2c_dev_) {
        i2c_master_bus_rm_device(i2c_dev_);
    }
    // Free pre-allocated stereo buffers
    if (tx_stereo_buffer_) {
        heap_caps_free(tx_stereo_buffer_);
    }
    if (rx_stereo_buffer_) {
        heap_caps_free(rx_stereo_buffer_);
    }
}

bool Wm8978AudioCodec::WriteReg(uint8_t reg, uint16_t value) {
    if (reg > 57) {
        return false;
    }

    // WM8978 uses 7-bit register address + 9-bit data
    // Format: [reg(7bit) | data_high(1bit)] [data_low(8bit)]
    uint8_t data[2];
    data[0] = (reg << 1) | ((value >> 8) & 0x01);
    data[1] = value & 0xFF;

    esp_err_t ret = i2c_master_transmit(i2c_dev_, data, 2, 100);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write reg 0x%02X: %s", reg, esp_err_to_name(ret));
        return false;
    }

    // Update cache
    reg_cache_[reg] = value;
    return true;
}

uint16_t Wm8978AudioCodec::ReadReg(uint8_t reg) {
    // WM8978 doesn't support read, return cached value
    if (reg > 57) {
        return 0;
    }
    return reg_cache_[reg];
}

bool Wm8978AudioCodec::InitCodec() {
    ESP_LOGI(TAG, "Initializing WM8978 codec...");

    // Software reset
    if (!WriteReg(WM8978_RESET, 0x000)) {
        ESP_LOGE(TAG, "Software reset failed - WM8978 not responding");
        return false;
    }
    vTaskDelay(pdMS_TO_TICKS(50));

    // ============================================
    // Power Management - Based on working library
    // ============================================

    // R1 (POWER1): 0x01B = Enable MICBIAS, BIASEN, BUFIOEN, VMID=5K
    // Bits: [8]=BUFDCOPEN, [7]=AUXEN, [6]=PLLEN, [5]=MICBIAS, [4]=BIAS,
    //       [3]=BUFIOEN, [2:0]=VMID (001=5K, 010=500K, 011=5K for rec)
    WriteReg(WM8978_POWER1, 0x01B);

    // R2 (POWER2): 0x1BF = Enable everything: ROUT1EN, LOUT1EN, BOOSTENL/R, INPPGAENL/R, ADCENL/R
    // Bits: [8]=BOOSTENL, [7]=BOOSTENR, [6]=INPPGAENL, [5]=INPPGAENR,
    //       [4]=ADCENL, [3]=ADCENR, [2]=unused, [1]=LOUT1EN, [0]=ROUT1EN
    WriteReg(WM8978_POWER2, 0x1BF);

    // R3 (POWER3): 0x06F = Enable LOUT2EN, ROUT2EN, RMIXEN, LMIXEN, DACENL, DACENR
    // Bits: [8]=OUT4EN, [7]=OUT3EN, [6]=LOUT2EN, [5]=ROUT2EN,
    //       [3]=RMIXEN, [2]=LMIXEN, [1]=DACENR, [0]=DACENL
    WriteReg(WM8978_POWER3, 0x06F);

    // ============================================
    // Clock Configuration - External MCLK from I2S
    // ============================================

    // R6 (CLK_GEN): 0x000 = External MCLK, MCLK not divided
    WriteReg(WM8978_CLK_GEN, 0x000);

    // ============================================
    // Audio Interface - I2S 16-bit
    // ============================================

    // R4 (AUDIO_IF): 0x010 = I2S format (FMT=10), 16-bit word length (WL=00)
    // Bits: [8]=BCP, [7]=LRP, [6:5]=WL, [4:3]=FMT, [2]=DLRSWAP, [1]=ALRSWAP, [0]=MONO
    WriteReg(WM8978_AUDIO_IF, 0x010);

    // ============================================
    // ADC/DAC Configuration
    // ============================================

    // R10 (DAC_CTRL): 0x008 = Disable soft mute, 128x oversampling
    WriteReg(WM8978_DAC_CTRL, 0x008);

    // R11 (LEFT_DAC_VOL): 0x1FF = Max volume (0dB) with update bit
    WriteReg(WM8978_LEFT_DAC_VOL, 0x1FF);

    // R12 (RIGHT_DAC_VOL): 0x1FF = Max volume (0dB) with update bit
    WriteReg(WM8978_RIGHT_DAC_VOL, 0x1FF);

    // R14 (ADC_CTRL): 0x108 = ADC 128x oversampling, high-pass filter at 3.7Hz
    WriteReg(WM8978_ADC_CTRL, 0x108);

    // R15 (LEFT_ADC_VOL): 0x1FF = Max volume (0dB) with update bit
    WriteReg(WM8978_LEFT_ADC_VOL, 0x1FF);

    // R16 (RIGHT_ADC_VOL): 0x1FF = Max volume (0dB) with update bit
    WriteReg(WM8978_RIGHT_ADC_VOL, 0x1FF);

    // ============================================
    // Beep Control - IMPORTANT for speaker output
    // ============================================

    // R43 (BEEP_CTRL): 0x010 = Invert ROUT2 for speaker drive
    // This is critical for proper speaker operation!
    WriteReg(WM8978_BEEP_CTRL, 0x010);

    // ============================================
    // Mic/Input Configuration
    // ============================================

    // R44 (INPUT_CTRL): Connect L2/R2 inputs to PGA for microphone
    // Bits [5:4] = L2_2INPPGA, R2_2INPPGA (connect L2/R2 to input PGA)
    WriteReg(WM8978_INPUT_CTRL, 0x033);  // Enable L2 and R2 inputs

    // R45 (LEFT_INP_PGA): Unmute, max volume
    // Bit [7] = INPPGAMUTEL (0=unmute), Bits [5:0] = volume (0x3F=max)
    WriteReg(WM8978_LEFT_INP_PGA, 0x13F);  // Unmute, max gain, update

    // R46 (RIGHT_INP_PGA): Unmute, max volume
    WriteReg(WM8978_RIGHT_INP_PGA, 0x13F);  // Unmute, max gain, update

    // R47 (LEFT_ADC_BOOST): 0x100 = +20dB boost for microphone
    WriteReg(WM8978_LEFT_ADC_BOOST, 0x100);

    // R48 (RIGHT_ADC_BOOST): 0x100 = +20dB boost for microphone
    WriteReg(WM8978_RIGHT_ADC_BOOST, 0x100);

    // ============================================
    // Output Control
    // ============================================

    // R49 (OUTPUT_CTRL): 0x002 = Thermal shutdown enable, VROI=1
    WriteReg(WM8978_OUTPUT_CTRL, 0x002);

    // R50 (LEFT_MIXER): 0x001 = DAC to left mixer (bypass disabled)
    WriteReg(WM8978_LEFT_MIXER, 0x001);

    // R51 (RIGHT_MIXER): 0x001 = DAC to right mixer (bypass disabled)
    WriteReg(WM8978_RIGHT_MIXER, 0x001);

    // ============================================
    // Volume Settings
    // ============================================

    // LOUT1/ROUT1 volume (headphone) - Max volume with zero-cross
    WriteReg(WM8978_LOUT1_VOL, 0x13F);  // Max volume, ZC, update
    WriteReg(WM8978_ROUT1_VOL, 0x13F);  // Max volume, ZC, update

    // LOUT2/ROUT2 volume (speaker) - Max volume with zero-cross
    WriteReg(WM8978_LOUT2_VOL, 0x13F);  // Max volume, ZC, update
    WriteReg(WM8978_ROUT2_VOL, 0x13F);  // Max volume, ZC, update

    ESP_LOGI(TAG, "WM8978 codec initialization complete");
    return true;
}

void Wm8978AudioCodec::CreateDuplexChannels(gpio_num_t mclk, gpio_num_t bclk, gpio_num_t ws,
                                            gpio_num_t dout, gpio_num_t din) {
    ESP_LOGI(TAG, "Creating I2S duplex channels");
    ESP_LOGI(TAG, "  MCLK=%d, BCK=%d, WS=%d, DOUT=%d, DIN=%d",
             mclk, bclk, ws, dout, din);

    // Use same sample rate for input and output (required for duplex)
    assert(input_sample_rate_ == output_sample_rate_);

    i2s_chan_config_t chan_cfg = {
        .id = I2S_NUM_0,
        .role = I2S_ROLE_MASTER,
        .dma_desc_num = 8,
        .dma_frame_num = 240,
        .auto_clear_after_cb = true,
        .auto_clear_before_cb = false,
        .intr_priority = 0,
    };
    ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, &tx_handle_, &rx_handle_));

    i2s_std_config_t std_cfg = {
        .clk_cfg = {
            .sample_rate_hz = (uint32_t)output_sample_rate_,
            .clk_src = I2S_CLK_SRC_DEFAULT,
            .mclk_multiple = I2S_MCLK_MULTIPLE_256,
#ifdef I2S_HW_VERSION_2
            .ext_clk_freq_hz = 0,
#endif
        },
        .slot_cfg = {
            .data_bit_width = I2S_DATA_BIT_WIDTH_16BIT,
            .slot_bit_width = I2S_SLOT_BIT_WIDTH_AUTO,
            .slot_mode = I2S_SLOT_MODE_STEREO,
            .slot_mask = I2S_STD_SLOT_BOTH,
            .ws_width = I2S_DATA_BIT_WIDTH_16BIT,
            .ws_pol = false,
            .bit_shift = true,
#ifdef I2S_HW_VERSION_2
            .left_align = true,
            .big_endian = false,
            .bit_order_lsb = false
#endif
        },
        .gpio_cfg = {
            .mclk = mclk,
            .bclk = bclk,
            .ws = ws,
            .dout = dout,
            .din = din,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv = false
            }
        }
    };

    ESP_ERROR_CHECK(i2s_channel_init_std_mode(tx_handle_, &std_cfg));
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(rx_handle_, &std_cfg));

    ESP_LOGI(TAG, "I2S duplex channels created at %d Hz", output_sample_rate_);
}

void Wm8978AudioCodec::SetOutputVolume(int volume) {
    std::lock_guard<std::mutex> lock(mutex_);

    // Map 0-100 to WM8978 volume range (0-63 with 0x100 for update bit)
    uint8_t vol = (volume * 63) / 100;
    uint16_t reg_val = 0x100 | vol;  // Set update bit

    WriteReg(WM8978_LOUT1_VOL, reg_val);
    WriteReg(WM8978_ROUT1_VOL, reg_val);
    WriteReg(WM8978_LOUT2_VOL, reg_val);
    WriteReg(WM8978_ROUT2_VOL, reg_val);

    AudioCodec::SetOutputVolume(volume);
    ESP_LOGI(TAG, "Output volume set to %d (%d/63)", volume, vol);
}

void Wm8978AudioCodec::SetMicGain(uint8_t gain) {
    if (gain > 63) gain = 63;

    // Set update bit (0x100) to apply changes immediately
    WriteReg(WM8978_LEFT_INP_PGA, 0x100 | gain);
    WriteReg(WM8978_RIGHT_INP_PGA, 0x100 | gain);
    ESP_LOGI(TAG, "Mic gain set to %d", gain);
}

void Wm8978AudioCodec::SetHeadphoneVolume(uint8_t volume) {
    if (volume > 63) volume = 63;
    uint16_t reg_val = 0x100 | volume;

    WriteReg(WM8978_LOUT1_VOL, reg_val);
    WriteReg(WM8978_ROUT1_VOL, reg_val);
    ESP_LOGI(TAG, "Headphone volume set to %d", volume);
}

void Wm8978AudioCodec::SetSpeakerVolume(uint8_t volume) {
    if (volume > 63) volume = 63;
    uint16_t reg_val = 0x100 | volume;

    WriteReg(WM8978_LOUT2_VOL, reg_val);
    WriteReg(WM8978_ROUT2_VOL, reg_val);
    ESP_LOGI(TAG, "Speaker volume set to %d", volume);
}

void Wm8978AudioCodec::EnableInput(bool enable) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (enable == input_enabled_) {
        return;
    }

    if (enable) {
        // Enable RX channel
        ESP_ERROR_CHECK(i2s_channel_enable(rx_handle_));
        ESP_LOGI(TAG, "Microphone enabled");
    } else {
        // Disable RX channel
        ESP_ERROR_CHECK(i2s_channel_disable(rx_handle_));
        ESP_LOGI(TAG, "Microphone disabled");
    }

    AudioCodec::EnableInput(enable);
}

void Wm8978AudioCodec::EnableOutput(bool enable) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (enable == output_enabled_) {
        return;
    }

    if (enable) {
        // Enable TX channel
        ESP_ERROR_CHECK(i2s_channel_enable(tx_handle_));

        // Enable PA if present
        if (pa_pin_ != GPIO_NUM_NC) {
            gpio_set_level(pa_pin_, 1);
        }
        ESP_LOGI(TAG, "Speaker enabled");
    } else {
        // Disable PA first
        if (pa_pin_ != GPIO_NUM_NC) {
            gpio_set_level(pa_pin_, 0);
        }

        // Disable TX channel
        ESP_ERROR_CHECK(i2s_channel_disable(tx_handle_));
        ESP_LOGI(TAG, "Speaker disabled");
    }

    AudioCodec::EnableOutput(enable);
}

int Wm8978AudioCodec::Read(int16_t* dest, int samples) {
    if (input_enabled_ && rx_handle_ && rx_stereo_buffer_) {
        // Check if samples exceeds pre-allocated buffer
        if (samples > (int)STEREO_BUFFER_SAMPLES) {
            ESP_LOGW(TAG, "Read samples %d exceeds buffer %d, truncating",
                     samples, STEREO_BUFFER_SAMPLES);
            samples = STEREO_BUFFER_SAMPLES;
        }

        size_t bytes_read = 0;
        // WM8978 outputs stereo, we read stereo but extract mono
        // Using pre-allocated buffer to avoid heap fragmentation
        esp_err_t ret = i2s_channel_read(rx_handle_, rx_stereo_buffer_,
                                         samples * 2 * sizeof(int16_t),
                                         &bytes_read, portMAX_DELAY);
        if (ret == ESP_OK && bytes_read > 0) {
            // Extract left channel only (mono)
            int samples_read = bytes_read / (2 * sizeof(int16_t));
            for (int i = 0; i < samples_read; i++) {
                dest[i] = rx_stereo_buffer_[i * 2];  // Left channel
            }
            // Zero remaining samples if any
            if (samples_read < samples) {
                memset(&dest[samples_read], 0, (samples - samples_read) * sizeof(int16_t));
            }
        } else {
            ESP_LOGE(TAG, "I2S read failed: %s (bytes_read=%d)", esp_err_to_name(ret), bytes_read);
            memset(dest, 0, samples * sizeof(int16_t));
        }
    } else {
        memset(dest, 0, samples * sizeof(int16_t));
    }
    return samples;
}

int Wm8978AudioCodec::Write(const int16_t* data, int samples) {
    if (output_enabled_ && tx_handle_ && data != nullptr && tx_stereo_buffer_) {
        // Check if samples exceeds pre-allocated buffer
        if (samples > (int)STEREO_BUFFER_SAMPLES) {
            ESP_LOGW(TAG, "Write samples %d exceeds buffer %d, truncating",
                     samples, STEREO_BUFFER_SAMPLES);
            samples = STEREO_BUFFER_SAMPLES;
        }

        // Convert mono to stereo for WM8978
        // Using pre-allocated buffer to avoid heap fragmentation
        for (int i = 0; i < samples; i++) {
            tx_stereo_buffer_[i * 2] = data[i];      // Left
            tx_stereo_buffer_[i * 2 + 1] = data[i];  // Right
        }

        size_t bytes_written = 0;
        esp_err_t ret = i2s_channel_write(tx_handle_, tx_stereo_buffer_,
                                          samples * 2 * sizeof(int16_t),
                                          &bytes_written, portMAX_DELAY);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "I2S write failed: %s", esp_err_to_name(ret));
        }
    }
    return samples;
}
