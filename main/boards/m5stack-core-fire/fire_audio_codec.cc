#include "fire_audio_codec.h"

#include <esp_log.h>
#include <string.h>
#include "settings.h"
#include "config.h"

static const char TAG[] = "FireAudioCodec";

// M5Stack Core Fire Audio Codec - Text Input Only Version
// Audio input and output are disabled due to ESP32 I2S/ADC limitations
// Use text input via button (double-click Button B) instead

FireAudioCodec::FireAudioCodec(int input_sample_rate, int output_sample_rate,
    uint32_t adc_mic_channel, gpio_num_t dac_gpio, gpio_num_t pa_ctl) {

    input_reference_ = false;
    input_sample_rate_ = input_sample_rate;
    output_sample_rate_ = output_sample_rate;
    output_volume_ = 0;

    ESP_LOGW(TAG, "FireAudioCodec: Audio I/O disabled - use text input via Button B double-click");
    ESP_LOGI(TAG, "FireAudioCodec initialized (no audio, text-only mode)");
}

FireAudioCodec::~FireAudioCodec() {
}

void FireAudioCodec::SetOutputVolume(int volume) {
    output_volume_ = 0;
    AudioCodec::SetOutputVolume(0);
}

void FireAudioCodec::EnableInput(bool enable) {
    // Microphone input disabled
    AudioCodec::EnableInput(false);
}

void FireAudioCodec::EnableOutput(bool enable) {
    // Audio output disabled
    AudioCodec::EnableOutput(false);
}

int FireAudioCodec::Read(int16_t* dest, int samples) {
    // Return silence - microphone not available
    memset(dest, 0, samples * sizeof(int16_t));
    return samples;
}

int FireAudioCodec::Write(const int16_t* data, int samples) {
    // Discard audio - speaker not available
    (void)data;
    return samples;
}

void FireAudioCodec::Start() {
    EnableInput(false);
    EnableOutput(false);
    ESP_LOGI(TAG, "Audio codec started (text-only mode - use Button B double-click)");
}
