#include "fire_audio_codec.h"

#include <esp_log.h>
#include <esp_timer.h>
#include <string.h>
#include "settings.h"
#include "config.h"

static const char TAG[] = "FireAudioCodec";

// M5Stack Core Fire Audio Codec
// NOTE: The original ESP32 has hardware limitations that prevent simultaneous
// use of ADC continuous mode (for microphone) and DAC continuous mode (for speaker)
// because both require I2S0 for DMA.
//
// This implementation provides a dummy codec that allows the device to boot
// and function with display/buttons/LED strip. For actual audio functionality,
// an external I2S audio codec would be needed.

FireAudioCodec::FireAudioCodec(int input_sample_rate, int output_sample_rate,
    uint32_t adc_mic_channel, gpio_num_t dac_gpio, gpio_num_t pa_ctl) {

    input_reference_ = false;
    input_sample_rate_ = input_sample_rate;
    output_sample_rate_ = output_sample_rate;

    // Microseconds per sample (for timing reference)
    if (output_sample_rate > 0) {
        output_sample_rate_us_ = 1000000 / output_sample_rate;
    } else {
        output_sample_rate_us_ = 41; // Default to ~24kHz
    }

    // Initialize DAC oneshot for basic audio output (no DMA, direct register writes)
    dac_oneshot_config_t dac_cfg = {
        .chan_id = DAC_CHAN_0,  // GPIO25 = DAC Channel 0
    };

    esp_err_t dac_err = dac_oneshot_new_channel(&dac_cfg, &dac_handle_);
    if (dac_err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to create DAC channel: %s", esp_err_to_name(dac_err));
        dac_handle_ = nullptr;
    } else {
        ESP_LOGI(TAG, "DAC oneshot initialized on GPIO%d", dac_gpio);
    }

    // Create audio queue (for future use)
    audio_queue_ = xQueueCreate(4, sizeof(AudioChunk));

    output_volume_ = 100;

    // Configure PA control pin if provided
    if (pa_ctl != GPIO_NUM_NC) {
        pa_ctrl_pin_ = pa_ctl;
        gpio_config_t io_conf = {};
        io_conf.intr_type = GPIO_INTR_DISABLE;
        io_conf.mode = GPIO_MODE_OUTPUT;
        io_conf.pin_bit_mask = (1ULL << pa_ctrl_pin_);
        io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
        io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
        gpio_config(&io_conf);
    }

    // Create output timer for auto-disable
    esp_timer_create_args_t output_timer_args = {
        .callback = &FireAudioCodec::OutputTimerCallback,
        .arg = this,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "fire_output_timer"
    };
    ESP_ERROR_CHECK(esp_timer_create(&output_timer_args, &output_timer_));

    ESP_LOGW(TAG, "FireAudioCodec: Audio input disabled due to ESP32 I2S limitations");
    ESP_LOGI(TAG, "FireAudioCodec initialized (DAC GPIO%d, no mic)", dac_gpio);
}

FireAudioCodec::~FireAudioCodec() {
    dac_task_running_ = false;
    if (dac_task_handle_) {
        vTaskDelete(dac_task_handle_);
        dac_task_handle_ = nullptr;
    }

    if (audio_queue_) {
        vQueueDelete(audio_queue_);
        audio_queue_ = nullptr;
    }

    if (output_timer_) {
        esp_timer_stop(output_timer_);
        esp_timer_delete(output_timer_);
        output_timer_ = nullptr;
    }

    if (dac_handle_) {
        dac_oneshot_del_channel(dac_handle_);
    }
}

void FireAudioCodec::SetOutputVolume(int volume) {
    output_volume_ = volume;
    AudioCodec::SetOutputVolume(volume);
}

void FireAudioCodec::EnableInput(bool enable) {
    // Microphone input is not available due to hardware limitations
    // ADC continuous mode requires I2S0 which conflicts with DAC
    AudioCodec::EnableInput(enable);
}

void FireAudioCodec::EnableOutput(bool enable) {
    if (enable == output_enabled_) {
        return;
    }
    if (enable) {
        if (pa_ctrl_pin_ != GPIO_NUM_NC) {
            gpio_set_level(pa_ctrl_pin_, 1);
        }

        // Start auto-disable timer
        if (output_timer_) {
            esp_timer_start_once(output_timer_, TIMER_TIMEOUT_US);
        }
    } else {
        if (output_timer_) {
            esp_timer_stop(output_timer_);
        }
        if (pa_ctrl_pin_ != GPIO_NUM_NC) {
            gpio_set_level(pa_ctrl_pin_, 0);
        }
    }
    AudioCodec::EnableOutput(enable);
}

int FireAudioCodec::Read(int16_t* dest, int samples) {
    // Return silence - microphone not available
    memset(dest, 0, samples * sizeof(int16_t));
    return samples;
}

int FireAudioCodec::Write(const int16_t* data, int samples) {
    // Audio output disabled - DAC oneshot with blocking delays causes watchdog timeouts
    // The ESP32 original cannot do proper audio output without DMA, which conflicts with PSRAM
    // For audio functionality, an external I2S codec would be needed
    (void)data;
    return samples;
}

void FireAudioCodec::Start() {
    Settings settings("audio", false);
    output_volume_ = settings.GetInt("output_volume", output_volume_);
    if (output_volume_ <= 0) {
        ESP_LOGW(TAG, "Output volume value (%d) is too small, setting to default (10)", output_volume_);
        output_volume_ = 10;
    }

    EnableInput(true);
    EnableOutput(true);
    ESP_LOGI(TAG, "Audio codec started (output only, no mic)");
}

void FireAudioCodec::OutputTimerCallback(void* arg) {
    FireAudioCodec* codec = static_cast<FireAudioCodec*>(arg);
    if (codec && codec->output_enabled_) {
        codec->EnableOutput(false);
    }
}

void FireAudioCodec::DacPlaybackTask(void* arg) {
    // Not used in simple blocking mode
    vTaskDelete(NULL);
}
