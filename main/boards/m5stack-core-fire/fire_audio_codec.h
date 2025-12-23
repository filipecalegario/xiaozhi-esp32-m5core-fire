#ifndef _FIRE_AUDIO_CODEC_H_
#define _FIRE_AUDIO_CODEC_H_

#include "audio_codec.h"

#include <driver/gpio.h>
#include <driver/dac_oneshot.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>

// M5Stack Core Fire Audio Codec
// Input: ADC microphone (MEMS BSE3729 on GPIO34/ADC1_CH6)
// Output: DAC oneshot on GPIO25 (ESP32 built-in 8-bit DAC)
// Note: DAC uses oneshot mode (no DMA) to avoid I2S0 conflict with ADC mic

// Audio buffer structure for queue
struct AudioChunk {
    uint8_t* data;
    int samples;
};

class FireAudioCodec : public AudioCodec {
private:
    dac_oneshot_handle_t dac_handle_ = nullptr;
    gpio_num_t pa_ctrl_pin_ = GPIO_NUM_NC;

    // Timer for auto-disable output after silence
    esp_timer_handle_t output_timer_ = nullptr;
    static constexpr uint64_t TIMER_TIMEOUT_US = 120000; // 120ms

    // Audio output task for DAC oneshot playback
    TaskHandle_t dac_task_handle_ = nullptr;
    QueueHandle_t audio_queue_ = nullptr;
    volatile bool dac_task_running_ = false;
    int output_sample_rate_us_ = 0; // Microseconds per sample

    // Timer callback
    static void OutputTimerCallback(void* arg);

    // DAC playback task
    static void DacPlaybackTask(void* arg);

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
