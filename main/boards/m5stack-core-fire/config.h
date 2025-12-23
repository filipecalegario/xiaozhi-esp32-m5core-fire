#ifndef _BOARD_CONFIG_H_
#define _BOARD_CONFIG_H_

// M5Stack Core Fire 2018.2A Board Configuration
// Hardware: ESP32-D0WDQ6, 16MB Flash, 8MB PSRAM
// Display: ILI9342C 320x240 LCD
// Audio: MEMS Mic (ADC) + Speaker (PDM)

#include <driver/gpio.h>

// ============================================
// Audio Configuration
// ============================================
#define AUDIO_INPUT_SAMPLE_RATE  16000
#define AUDIO_OUTPUT_SAMPLE_RATE 24000

// PDM upsampling factor (range: <=480, 441 is more stable on some devices)
#define AUDIO_PDM_UPSAMPLE_FS    441

// Microphone: MEMS BSE3729 analog on GPIO34 (ADC1_CH6)
#define AUDIO_ADC_MIC_CHANNEL    6

// Speaker: PDM output on GPIO25 (internal DAC pin)
#define AUDIO_PDM_SPEAK_P_GPIO   GPIO_NUM_25
#define AUDIO_PDM_SPEAK_N_GPIO   GPIO_NUM_NC  // Single-ended output
#define AUDIO_PA_CTL_GPIO        GPIO_NUM_NC  // No PA control needed

// ============================================
// Display Configuration (ILI9342C via SPI)
// ============================================
#define LCD_TYPE_ILI9341_SERIAL

#define DISPLAY_SPI_MOSI_PIN     GPIO_NUM_23
#define DISPLAY_SPI_CLK_PIN      GPIO_NUM_18
#define DISPLAY_SPI_CS_PIN       GPIO_NUM_14
#define DISPLAY_DC_PIN           GPIO_NUM_27
#define DISPLAY_RST_PIN          GPIO_NUM_33
#define DISPLAY_BACKLIGHT_PIN    GPIO_NUM_32

#define DISPLAY_WIDTH            320
#define DISPLAY_HEIGHT           240
#define DISPLAY_MIRROR_X         false
#define DISPLAY_MIRROR_Y         false
#define DISPLAY_SWAP_XY          false
#define DISPLAY_INVERT_COLOR     true
#define DISPLAY_RGB_ORDER        LCD_RGB_ELEMENT_ORDER_BGR
#define DISPLAY_OFFSET_X         0
#define DISPLAY_OFFSET_Y         0
#define DISPLAY_SPI_MODE         0
#define DISPLAY_SPI_SCLK_HZ      (40 * 1000 * 1000)  // 40MHz
#define DISPLAY_BACKLIGHT_OUTPUT_INVERT false

// ============================================
// Button Configuration
// ============================================
#define BUTTON_A_GPIO            GPIO_NUM_39  // Main button - toggle chat
#define BUTTON_B_GPIO            GPIO_NUM_38  // Volume down
#define BUTTON_C_GPIO            GPIO_NUM_37  // Volume up
#define BOOT_BUTTON_GPIO         GPIO_NUM_39  // Alias for button A

// ============================================
// LED Strip Configuration (M5GO Base)
// ============================================
#define LED_STRIP_GPIO           GPIO_NUM_15
#define LED_STRIP_NUM_LEDS       10

// ============================================
// I2C Configuration (Shared bus)
// ============================================
#define I2C_SDA_PIN              GPIO_NUM_21
#define I2C_SCL_PIN              GPIO_NUM_22

// I2C Device Addresses
#define IP5306_I2C_ADDR          0x75  // Power management
#define MPU6886_I2C_ADDR         0x68  // IMU (optional)
#define BMM150_I2C_ADDR          0x10  // Magnetometer (optional)

// ============================================
// TF Card (SD Card) - Optional
// ============================================
#define SD_SPI_MOSI_PIN          GPIO_NUM_23  // Shared with LCD
#define SD_SPI_MISO_PIN          GPIO_NUM_19
#define SD_SPI_CLK_PIN           GPIO_NUM_18  // Shared with LCD
#define SD_SPI_CS_PIN            GPIO_NUM_4

// ============================================
// Important Notes
// ============================================
// GPIO16 and GPIO17 are connected to PSRAM - DO NOT USE!
// Version 2018.2A does not support USB-C to USB-C or PD power supply

#endif // _BOARD_CONFIG_H_
