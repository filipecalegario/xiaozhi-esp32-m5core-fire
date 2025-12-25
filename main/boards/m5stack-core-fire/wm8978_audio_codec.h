/**
 * @file wm8978_audio_codec.h
 * @brief WM8978 Audio Codec Driver for M5Stack Node Base
 *
 * This driver implements full-duplex I2S audio for the WM8978 codec
 * used in the M5Stack Node Base. The WM8978 is a high-quality audio
 * codec featuring:
 * - 24-bit ADC/DAC
 * - Up to 48kHz sample rate
 * - Integrated microphone preamplifier with +20dB boost
 * - Stereo headphone and speaker outputs
 * - I2S audio interface
 * - I2C control interface (address 0x1A)
 *
 * Hardware Connection (M5Stack Node Base):
 * - MCLK: GPIO0 (Master clock, 256x sample rate)
 * - BCK:  GPIO5 (Bit clock)
 * - WS:   GPIO13 (Word select / LRCLK)
 * - DOUT: GPIO2 (Data out to codec - speaker)
 * - DIN:  GPIO34 (Data in from codec - microphone)
 * - I2C:  GPIO21 (SDA), GPIO22 (SCL)
 *
 * Usage Example:
 * @code
 * Wm8978AudioCodec codec(
 *     i2c_bus,
 *     16000,      // Input sample rate
 *     16000,      // Output sample rate
 *     GPIO_NUM_0, // MCLK
 *     GPIO_NUM_5, // BCK
 *     GPIO_NUM_13,// WS
 *     GPIO_NUM_2, // DOUT
 *     GPIO_NUM_34 // DIN
 * );
 *
 * codec.SetOutputVolume(80);  // 80%
 * codec.EnableOutput(true);
 * codec.EnableInput(true);
 * @endcode
 *
 * @note The WM8978 does not support I2C register reads. This driver
 *       maintains a shadow register cache for read operations.
 *
 * @note GPIO0 is a strapping pin on ESP32. The driver assumes GPIO0
 *       is configured as MCLK output after boot completes.
 *
 * @author XiaoZhi Project
 * @date 2025-12-25
 */

#ifndef _WM8978_AUDIO_CODEC_H_
#define _WM8978_AUDIO_CODEC_H_

#include "audio_codec.h"
#include <driver/gpio.h>
#include <driver/i2s_std.h>
#include <driver/i2c_master.h>
#include <mutex>

/** WM8978 I2C device address (7-bit) */
#define WM8978_ADDR 0x1A

/**
 * @class Wm8978AudioCodec
 * @brief Full-duplex I2S audio codec driver for WM8978
 *
 * This class implements the AudioCodec interface for the WM8978 codec.
 * It supports simultaneous audio input (microphone) and output (speaker).
 *
 * Key features:
 * - Full-duplex I2S operation
 * - Mono-to-stereo and stereo-to-mono conversion
 * - Thread-safe volume control
 * - Configurable microphone gain
 */
class Wm8978AudioCodec : public AudioCodec {
private:
    /** I2S TX channel handle (for audio output) */
    i2s_chan_handle_t tx_handle_ = nullptr;

    /** I2S RX channel handle (for audio input) */
    i2s_chan_handle_t rx_handle_ = nullptr;

    /** I2C device handle for codec control */
    i2c_master_dev_handle_t i2c_dev_ = nullptr;

    /** I2C bus handle (shared with other devices) */
    i2c_master_bus_handle_t i2c_bus_;

    /** Power amplifier control GPIO (GPIO_NUM_NC if not used) */
    gpio_num_t pa_pin_;

    /**
     * WM8978 register cache
     * The WM8978 has 58 registers (0x00-0x39) but doesn't support I2C reads.
     * This cache stores the last written value for each register.
     */
    uint16_t reg_cache_[58];

    /** Mutex for thread-safe operations */
    std::mutex mutex_;

    /**
     * @brief Create I2S full-duplex channels
     * @param mclk Master clock GPIO
     * @param bclk Bit clock GPIO
     * @param ws Word select GPIO
     * @param dout Data output GPIO (to codec)
     * @param din Data input GPIO (from codec)
     */
    void CreateDuplexChannels(gpio_num_t mclk, gpio_num_t bclk, gpio_num_t ws,
                              gpio_num_t dout, gpio_num_t din);

    /**
     * @brief Write a value to a WM8978 register
     * @param reg Register address (0-57)
     * @param value 9-bit value to write
     * @return true on success, false on I2C error
     */
    bool WriteReg(uint8_t reg, uint16_t value);

    /**
     * @brief Read a cached register value
     * @param reg Register address (0-57)
     * @return Cached register value
     * @note WM8978 doesn't support I2C reads; returns cached value
     */
    uint16_t ReadReg(uint8_t reg);

    /**
     * @brief Initialize the WM8978 codec
     * Performs software reset and configures all registers for:
     * - I2S 16-bit audio format
     * - Microphone input with +20dB boost
     * - Speaker and headphone outputs
     * - Maximum volume settings
     * @return true on success, false if codec not responding
     */
    bool InitCodec();

    /**
     * @brief Configure I2S interface format (not used, kept for future)
     */
    void ConfigureI2S(uint8_t format, uint8_t bits);

    /**
     * @brief Configure ADC/DAC enable (not used, kept for future)
     */
    void ConfigureADDA(bool adc_en, bool dac_en);

    /**
     * @brief Configure input sources (not used, kept for future)
     */
    void ConfigureInput(bool mic_en, bool line_en, bool aux_en);

    /**
     * @brief Configure output routing (not used, kept for future)
     */
    void ConfigureOutput(bool dac_en, bool bypass_en);

    /**
     * @brief Read audio samples from microphone
     * @param dest Destination buffer for mono samples
     * @param samples Number of samples to read
     * @return Number of samples read
     * @note Internally reads stereo and extracts left channel
     */
    virtual int Read(int16_t* dest, int samples) override;

    /**
     * @brief Write audio samples to speaker
     * @param data Source buffer with mono samples
     * @param samples Number of samples to write
     * @return Number of samples written
     * @note Internally converts mono to stereo for codec
     */
    virtual int Write(const int16_t* data, int samples) override;

public:
    /**
     * @brief Construct a new WM8978 Audio Codec object
     *
     * @param i2c_bus Initialized I2C bus handle
     * @param input_sample_rate Microphone sample rate (Hz)
     * @param output_sample_rate Speaker sample rate (Hz)
     * @param mclk Master clock GPIO (usually GPIO0)
     * @param bclk Bit clock GPIO
     * @param ws Word select GPIO
     * @param dout Data output GPIO (ESP32 → codec)
     * @param din Data input GPIO (codec → ESP32)
     * @param pa_pin Power amplifier control GPIO (optional)
     *
     * @note input_sample_rate must equal output_sample_rate for full-duplex
     */
    Wm8978AudioCodec(i2c_master_bus_handle_t i2c_bus,
                     int input_sample_rate, int output_sample_rate,
                     gpio_num_t mclk, gpio_num_t bclk, gpio_num_t ws,
                     gpio_num_t dout, gpio_num_t din,
                     gpio_num_t pa_pin = GPIO_NUM_NC);

    /**
     * @brief Destroy the WM8978 Audio Codec object
     * Releases I2S channels and I2C device
     */
    virtual ~Wm8978AudioCodec();

    /**
     * @brief Set the output volume
     * @param volume Volume level 0-100 (percentage)
     * @note Sets both headphone and speaker outputs
     */
    virtual void SetOutputVolume(int volume) override;

    /**
     * @brief Enable or disable audio input (microphone)
     * @param enable true to enable, false to disable
     */
    virtual void EnableInput(bool enable) override;

    /**
     * @brief Enable or disable audio output (speaker)
     * @param enable true to enable, false to disable
     */
    virtual void EnableOutput(bool enable) override;

    /**
     * @brief Set microphone input gain
     * @param gain Gain value 0-63 (0 = mute, 63 = max)
     * @note Additional +20dB boost is applied by default
     */
    void SetMicGain(uint8_t gain);

    /**
     * @brief Set headphone output volume
     * @param volume Volume 0-63 (0 = mute, 63 = max)
     */
    void SetHeadphoneVolume(uint8_t volume);

    /**
     * @brief Set speaker output volume
     * @param volume Volume 0-63 (0 = mute, 63 = max)
     */
    void SetSpeakerVolume(uint8_t volume);
};

#endif // _WM8978_AUDIO_CODEC_H_
