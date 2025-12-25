#ifndef _FIRE_AUDIO_CODEC_H_
#define _FIRE_AUDIO_CODEC_H_

#include "audio_codec.h"
#include <driver/gpio.h>
#include "esp_adc/adc_oneshot.h"
#include "driver/dac_continuous.h"

// M5Stack Core Fire Audio Codec
// Input: ADC microphone (MEMS BSE3729 on GPIO34/ADC1_CH6) using oneshot mode
// Output: DAC continuous mode on GPIO25 (internal DAC channel 1)
//
// Note: ADC continuous mode doesn't work properly on M5Stack Core Fire,
// so we use ADC oneshot mode for input. Since ADC oneshot doesn't use I2S0,
// we can use DAC continuous mode (which uses I2S0) for audio output.

class FireAudioCodec : public AudioCodec {
private:
    // ADC for microphone input
    adc_oneshot_unit_handle_t adc_handle_ = nullptr;
    adc_channel_t adc_channel_;
    int dc_offset_ = 2048;  // DC offset for centering audio (12-bit ADC midpoint)

    // DAC for speaker output
    dac_continuous_handle_t dac_handle_ = nullptr;
    gpio_num_t dac_gpio_;

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
