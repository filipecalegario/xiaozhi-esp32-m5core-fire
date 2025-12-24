#ifndef _FIRE_AUDIO_CODEC_H_
#define _FIRE_AUDIO_CODEC_H_

#include "audio_codec.h"
#include <driver/gpio.h>
#include "esp_adc/adc_oneshot.h"

// M5Stack Core Fire Audio Codec - Microphone Only Version
// Input: ADC microphone (MEMS BSE3729 on GPIO34/ADC1_CH6) using oneshot mode
// Output: Disabled (text response on display only)
//
// Note: ADC continuous mode doesn't work properly on M5Stack Core Fire,
// so we use oneshot mode which is slower but reliable.

class FireAudioCodec : public AudioCodec {
private:
    adc_oneshot_unit_handle_t adc_handle_ = nullptr;
    adc_channel_t adc_channel_;
    int dc_offset_ = 2048;  // DC offset for centering audio (12-bit ADC midpoint)

    virtual int Read(int16_t* dest, int samples) override;
    virtual int Write(const int16_t* data, int samples) override;

public:
    FireAudioCodec(int input_sample_rate, int output_sample_rate,
        uint32_t adc_mic_channel, gpio_num_t dac_gpio, gpio_num_t pa_ctl = GPIO_NUM_NC);
    virtual ~FireAudioCodec();

    virtual void SetOutputVolume(int volume) override;
    virtual void EnableInput(bool enable) override;
    virtual void EnableOutput(bool enable) override;
    void Start();
};

#endif // _FIRE_AUDIO_CODEC_H_
