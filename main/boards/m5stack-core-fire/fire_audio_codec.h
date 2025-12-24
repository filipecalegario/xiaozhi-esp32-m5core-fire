#ifndef _FIRE_AUDIO_CODEC_H_
#define _FIRE_AUDIO_CODEC_H_

#include "audio_codec.h"
#include <driver/gpio.h>

// M5Stack Core Fire Audio Codec - Text Input Only Version
// Audio I/O disabled due to ESP32 hardware limitations
// Use text input via button (double-click Button B) instead

class FireAudioCodec : public AudioCodec {
private:
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
