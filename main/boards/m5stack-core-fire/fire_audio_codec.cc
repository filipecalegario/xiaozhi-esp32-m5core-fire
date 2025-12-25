#include "fire_audio_codec.h"

#include <esp_log.h>
#include <string.h>
#include "settings.h"
#include "config.h"

static const char TAG[] = "FireAudioCodec";

// M5Stack Core Fire Audio Codec
// Input: ADC oneshot mode for microphone (continuous mode doesn't work)
// Output: DAC continuous mode for speaker (uses I2S0 internally)

FireAudioCodec::FireAudioCodec(int input_sample_rate, int output_sample_rate,
    uint32_t adc_mic_channel, gpio_num_t dac_gpio, gpio_num_t pa_ctl) {

    input_reference_ = false;
    input_sample_rate_ = input_sample_rate;
    output_sample_rate_ = output_sample_rate;
    output_volume_ = 70;  // Default volume
    adc_channel_ = (adc_channel_t)adc_mic_channel;
    dac_gpio_ = dac_gpio;

    // ========================================
    // Initialize ADC oneshot for microphone
    // ========================================
    adc_oneshot_unit_init_cfg_t adc_init_config = {
        .unit_id = ADC_UNIT_1,
        .clk_src = ADC_RTC_CLK_SRC_DEFAULT,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };

    esp_err_t ret = adc_oneshot_new_unit(&adc_init_config, &adc_handle_);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init ADC unit: %s", esp_err_to_name(ret));
        adc_handle_ = nullptr;
    } else {
        // Configure ADC channel
        adc_oneshot_chan_cfg_t chan_config = {
            .atten = ADC_ATTEN_DB_12,  // Full scale 0-3.3V
            .bitwidth = ADC_BITWIDTH_12,
        };

        ret = adc_oneshot_config_channel(adc_handle_, adc_channel_, &chan_config);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to config ADC channel: %s", esp_err_to_name(ret));
            adc_oneshot_del_unit(adc_handle_);
            adc_handle_ = nullptr;
        } else {
            // Calibrate DC offset by reading some samples
            int32_t offset_sum = 0;
            for (int i = 0; i < 100; i++) {
                int raw = 0;
                adc_oneshot_read(adc_handle_, adc_channel_, &raw);
                offset_sum += raw;
            }
            dc_offset_ = offset_sum / 100;
            ESP_LOGI(TAG, "ADC initialized (channel %d, DC offset=%d)", (int)adc_channel_, dc_offset_);
        }
    }

    // ========================================
    // Initialize DAC continuous for speaker
    // ========================================
    // Determine DAC channel from GPIO
    dac_channel_t dac_channel;
    if (dac_gpio == GPIO_NUM_25) {
        dac_channel = DAC_CHAN_0;  // GPIO25 = DAC channel 0
    } else if (dac_gpio == GPIO_NUM_26) {
        dac_channel = DAC_CHAN_1;  // GPIO26 = DAC channel 1
    } else {
        ESP_LOGE(TAG, "Invalid DAC GPIO: %d (must be 25 or 26)", dac_gpio);
        dac_handle_ = nullptr;
        return;
    }

    dac_continuous_config_t dac_cfg = {
        .chan_mask = (dac_channel_mask_t)(1 << dac_channel),
        .desc_num = 8,
        .buf_size = 2048,
        .freq_hz = (uint32_t)output_sample_rate,
        .offset = 0,
        .clk_src = DAC_DIGI_CLK_SRC_DEFAULT,
        .chan_mode = DAC_CHANNEL_MODE_SIMUL,
    };

    ret = dac_continuous_new_channels(&dac_cfg, &dac_handle_);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create DAC channels: %s", esp_err_to_name(ret));
        dac_handle_ = nullptr;
    } else {
        ESP_LOGI(TAG, "DAC initialized (GPIO%d, %d Hz)", dac_gpio, output_sample_rate);
    }

    ESP_LOGI(TAG, "FireAudioCodec initialized (ADC oneshot + DAC continuous)");
}

FireAudioCodec::~FireAudioCodec() {
    if (dac_handle_) {
        dac_continuous_disable(dac_handle_);
        dac_continuous_del_channels(dac_handle_);
        dac_handle_ = nullptr;
    }
    if (adc_handle_) {
        adc_oneshot_del_unit(adc_handle_);
        adc_handle_ = nullptr;
    }
}

void FireAudioCodec::SetOutputVolume(int volume) {
    output_volume_ = volume;
    AudioCodec::SetOutputVolume(volume);
    ESP_LOGI(TAG, "Output volume set to %d", volume);
}

void FireAudioCodec::EnableInput(bool enable) {
    if (enable == input_enabled_) {
        return;
    }

    if (!adc_handle_) {
        ESP_LOGE(TAG, "ADC not initialized");
        return;
    }

    if (enable) {
        ESP_LOGI(TAG, "Microphone enabled at %d Hz (oneshot mode)", input_sample_rate_);
    } else {
        ESP_LOGI(TAG, "Microphone disabled");
    }

    AudioCodec::EnableInput(enable);
}

void FireAudioCodec::EnableOutput(bool enable) {
    // DAC continuous mode doesn't like being enabled/disabled repeatedly
    // So we keep the DAC always enabled and just control the output_enabled_ flag
    // The Write() function will check output_enabled_ before writing data

    if (enable == output_enabled_) {
        return;
    }

    if (!dac_handle_) {
        ESP_LOGE(TAG, "DAC not initialized");
        AudioCodec::EnableOutput(false);
        return;
    }

    // Enable DAC on first call, never disable it
    static bool dac_started = false;
    if (enable && !dac_started) {
        esp_err_t ret = dac_continuous_enable(dac_handle_);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to enable DAC: %s", esp_err_to_name(ret));
            AudioCodec::EnableOutput(false);
            return;
        }
        dac_started = true;
        ESP_LOGI(TAG, "DAC started at %d Hz (will stay enabled)", output_sample_rate_);
    }

    ESP_LOGI(TAG, "Output %s", enable ? "enabled" : "disabled");
    AudioCodec::EnableOutput(enable);
}

int FireAudioCodec::Read(int16_t* dest, int samples) {
    if (input_enabled_ && adc_handle_) {
        for (int i = 0; i < samples; i++) {
            int raw = 0;
            esp_err_t ret = adc_oneshot_read(adc_handle_, adc_channel_, &raw);
            if (ret == ESP_OK) {
                // Convert 12-bit unsigned (0-4095) to 16-bit signed (-32768 to 32767)
                int32_t centered = raw - dc_offset_;
                // Scale from 12-bit range to 16-bit range (multiply by 16)
                dest[i] = (int16_t)(centered * 16);
            } else {
                dest[i] = 0;
            }
        }
    } else {
        memset(dest, 0, samples * sizeof(int16_t));
    }
    return samples;
}

int FireAudioCodec::Write(const int16_t* data, int samples) {
    static int write_count = 0;
    static int total_samples = 0;

    if (output_enabled_ && dac_handle_) {
        write_count++;
        total_samples += samples;

        // Use static buffer to avoid stack overflow
        const int CHUNK_SIZE = 512;
        static uint8_t dac_buffer[CHUNK_SIZE];

        int remaining = samples;
        int offset = 0;

        while (remaining > 0) {
            int chunk = (remaining > CHUNK_SIZE) ? CHUNK_SIZE : remaining;

            // Convert 16-bit signed to 8-bit unsigned for DAC
            for (int i = 0; i < chunk; i++) {
                // Apply volume (0-100)
                int32_t sample = (int32_t)data[offset + i] * output_volume_ / 100;

                // Convert: signed 16-bit (-32768..32767) to unsigned 8-bit (0..255)
                int32_t unsigned_sample = (sample + 32768) >> 8;

                // Clamp
                if (unsigned_sample < 0) unsigned_sample = 0;
                if (unsigned_sample > 255) unsigned_sample = 255;

                dac_buffer[i] = (uint8_t)unsigned_sample;
            }

            // Write chunk to DAC
            size_t bytes_written = 0;
            esp_err_t ret = dac_continuous_write(dac_handle_, dac_buffer, chunk, &bytes_written, 1000);
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "DAC write failed: %s (chunk=%d)", esp_err_to_name(ret), chunk);
                break;
            }

            remaining -= chunk;
            offset += chunk;
        }

        // Log every 50 calls
        if (write_count % 50 == 1) {
            ESP_LOGI(TAG, "Write: calls=%d, total_samples=%d, vol=%d",
                     write_count, total_samples, output_volume_);
        }
    }
    return samples;
}

void FireAudioCodec::Start() {
    Settings settings("audio", false);

    EnableInput(true);
    EnableOutput(true);

    ESP_LOGI(TAG, "Audio codec started (mic: ADC oneshot, speaker: DAC continuous)");
}
