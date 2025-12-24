#include "fire_audio_codec.h"

#include <esp_log.h>
#include <string.h>
#include "settings.h"
#include "config.h"

static const char TAG[] = "FireAudioCodec";

// M5Stack Core Fire Audio Codec - Microphone Only Version
// Uses ADC oneshot mode for microphone input (continuous mode doesn't work)
// Audio output is disabled

FireAudioCodec::FireAudioCodec(int input_sample_rate, int output_sample_rate,
    uint32_t adc_mic_channel, gpio_num_t dac_gpio, gpio_num_t pa_ctl) {

    input_reference_ = false;
    input_sample_rate_ = input_sample_rate;  // Use requested rate (16kHz)
    output_sample_rate_ = output_sample_rate;
    output_volume_ = 0;
    adc_channel_ = (adc_channel_t)adc_mic_channel;

    // Initialize ADC oneshot unit
    adc_oneshot_unit_init_cfg_t init_config = {
        .unit_id = ADC_UNIT_1,
        .clk_src = ADC_RTC_CLK_SRC_DEFAULT,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };

    esp_err_t ret = adc_oneshot_new_unit(&init_config, &adc_handle_);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init ADC unit: %s", esp_err_to_name(ret));
        adc_handle_ = nullptr;
        return;
    }

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
        return;
    }

    // Calibrate DC offset by reading some samples
    int32_t offset_sum = 0;
    for (int i = 0; i < 100; i++) {
        int raw = 0;
        adc_oneshot_read(adc_handle_, adc_channel_, &raw);
        offset_sum += raw;
    }
    dc_offset_ = offset_sum / 100;

    ESP_LOGI(TAG, "FireAudioCodec initialized (ADC oneshot on channel %d, DC offset=%d)",
             (int)adc_channel_, dc_offset_);
}

FireAudioCodec::~FireAudioCodec() {
    if (adc_handle_) {
        adc_oneshot_del_unit(adc_handle_);
        adc_handle_ = nullptr;
    }
}

void FireAudioCodec::SetOutputVolume(int volume) {
    output_volume_ = 0;
    AudioCodec::SetOutputVolume(0);
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
    // Audio output is disabled
    AudioCodec::EnableOutput(false);
}

int FireAudioCodec::Read(int16_t* dest, int samples) {
    static int debug_counter = 0;

    if (input_enabled_ && adc_handle_) {
        for (int i = 0; i < samples; i++) {
            int raw = 0;
            esp_err_t ret = adc_oneshot_read(adc_handle_, adc_channel_, &raw);
            if (ret == ESP_OK) {
                // Convert 12-bit unsigned (0-4095) to 16-bit signed (-32768 to 32767)
                // Subtract DC offset and scale up
                int32_t centered = raw - dc_offset_;
                // Scale from 12-bit range to 16-bit range (multiply by 16)
                dest[i] = (int16_t)(centered * 16);
            } else {
                dest[i] = 0;
            }
        }

        // Debug: print audio stats periodically
        debug_counter++;
        if (debug_counter >= 50) {
            debug_counter = 0;
            int16_t min_val = dest[0], max_val = dest[0];
            int32_t sum = 0;
            for (int i = 0; i < samples; i++) {
                if (dest[i] < min_val) min_val = dest[i];
                if (dest[i] > max_val) max_val = dest[i];
                sum += dest[i];
            }
            int16_t avg = (int16_t)(sum / samples);
            ESP_LOGI(TAG, "Audio: min=%d max=%d avg=%d range=%d", min_val, max_val, avg, max_val - min_val);
        }
    } else {
        memset(dest, 0, samples * sizeof(int16_t));
    }
    return samples;
}

int FireAudioCodec::Write(const int16_t* data, int samples) {
    // Audio output is disabled - discard data
    (void)data;
    return samples;
}

void FireAudioCodec::Start() {
    Settings settings("audio", false);

    EnableInput(true);
    EnableOutput(false);
    ESP_LOGI(TAG, "Audio codec started (microphone only, oneshot mode)");
}
