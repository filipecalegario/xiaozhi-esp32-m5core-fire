#include "wifi_board.h"
#include "wm8978_audio_codec.h"
#include "display/lcd_display.h"
#include "application.h"
#include "button.h"
#include "config.h"
#include "led/circular_strip.h"
#include "led/led.h"
#include "i2c_device.h"
#include "assets/lang_config.h"

#include <esp_log.h>
#include <driver/i2c_master.h>
#include <esp_lcd_panel_vendor.h>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include <driver/spi_common.h>
#include "esp_lcd_ili9341.h"

#define TAG "M5StackCoreFire"

// IP5306 Power Management IC
#define IP5306_ADDR              0x75
#define IP5306_REG_SYS_CTL0      0x00
#define IP5306_REG_SYS_CTL1      0x01
#define IP5306_REG_SYS_CTL2      0x02
#define IP5306_REG_READ0         0x70
#define IP5306_REG_READ1         0x71
#define IP5306_REG_READ2         0x72
#define IP5306_REG_READ3         0x78

class Ip5306 : public I2cDevice {
public:
    Ip5306(i2c_master_bus_handle_t i2c_bus, uint8_t addr) : I2cDevice(i2c_bus, addr) {
        // Configure IP5306 for optimal operation
        // Boost output normally open (bit 1 = 0), Key off enable (bit 0 = 1)
        // Note: WriteReg is called only after device presence is verified
    }

    bool Initialize() {
        // Try to configure IP5306 - returns false if device not present
        esp_err_t err = WriteRegNoCheck(IP5306_REG_SYS_CTL0, 0x35);
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "IP5306 initialized");
            return true;
        }
        ESP_LOGW(TAG, "IP5306 not found (M5GO Base not connected?)");
        return false;
    }

    esp_err_t WriteRegNoCheck(uint8_t reg, uint8_t value) {
        uint8_t data[2] = {reg, value};
        return i2c_master_transmit(i2c_device_, data, sizeof(data), 100);
    }

    int GetBatteryLevel() {
        uint8_t data = ReadReg(IP5306_REG_READ3);
        // Bits 7:4 indicate charge level (0x0 = 0%, 0xF = 100%)
        int level = ((data >> 4) & 0x0F) * 100 / 15;
        return level;
    }

    bool IsCharging() {
        uint8_t data = ReadReg(IP5306_REG_READ0);
        // Bit 3 indicates charging status
        return (data & 0x08) != 0;
    }
};

class M5StackCoreFireBoard : public WifiBoard {
private:
    i2c_master_bus_handle_t i2c_bus_;
    Ip5306* ip5306_ = nullptr;
    LcdDisplay* display_ = nullptr;
    CircularStrip* led_strip_ = nullptr;

    Button button_a_;
    Button button_b_;
    Button button_c_;

    void InitializeI2c() {
        ESP_LOGI(TAG, "Initializing I2C bus");
        i2c_master_bus_config_t i2c_bus_cfg = {
            .i2c_port = I2C_NUM_0,
            .sda_io_num = I2C_SDA_PIN,
            .scl_io_num = I2C_SCL_PIN,
            .clk_source = I2C_CLK_SRC_DEFAULT,
            .glitch_ignore_cnt = 7,
            .intr_priority = 0,
            .trans_queue_depth = 0,
            .flags = {
                .enable_internal_pullup = 1,
            },
        };
        ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_cfg, &i2c_bus_));
    }

    void InitializePowerManagement() {
        ESP_LOGI(TAG, "Checking for IP5306 power management");
        Ip5306* ip5306 = new Ip5306(i2c_bus_, IP5306_ADDR);
        if (ip5306->Initialize()) {
            ip5306_ = ip5306;  // Keep reference only if device found
        } else {
            delete ip5306;     // Clean up if device not present
            ip5306_ = nullptr;
            ESP_LOGI(TAG, "IP5306 not found (no battery monitoring)");
        }
    }

    void InitializeSpi() {
        ESP_LOGI(TAG, "Initializing SPI bus for display");
        spi_bus_config_t buscfg = {};
        buscfg.mosi_io_num = DISPLAY_SPI_MOSI_PIN;
        buscfg.miso_io_num = GPIO_NUM_NC;
        buscfg.sclk_io_num = DISPLAY_SPI_CLK_PIN;
        buscfg.quadwp_io_num = GPIO_NUM_NC;
        buscfg.quadhd_io_num = GPIO_NUM_NC;
        buscfg.max_transfer_sz = DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(uint16_t);
        ESP_ERROR_CHECK(spi_bus_initialize(SPI3_HOST, &buscfg, SPI_DMA_CH_AUTO));
    }

    void InitializeLcdDisplay() {
        ESP_LOGI(TAG, "Initializing ILI9342C LCD display");
        esp_lcd_panel_io_handle_t panel_io = nullptr;
        esp_lcd_panel_handle_t panel = nullptr;

        // Configure LCD panel IO
        esp_lcd_panel_io_spi_config_t io_config = {};
        io_config.cs_gpio_num = DISPLAY_SPI_CS_PIN;
        io_config.dc_gpio_num = DISPLAY_DC_PIN;
        io_config.spi_mode = DISPLAY_SPI_MODE;
        io_config.pclk_hz = DISPLAY_SPI_SCLK_HZ;
        io_config.trans_queue_depth = 10;
        io_config.lcd_cmd_bits = 8;
        io_config.lcd_param_bits = 8;
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(SPI3_HOST, &io_config, &panel_io));

        // Initialize LCD driver (ILI9341 is compatible with ILI9342C)
        esp_lcd_panel_dev_config_t panel_config = {};
        panel_config.reset_gpio_num = DISPLAY_RST_PIN;
        panel_config.rgb_ele_order = DISPLAY_RGB_ORDER;
        panel_config.bits_per_pixel = 16;
        ESP_ERROR_CHECK(esp_lcd_new_panel_ili9341(panel_io, &panel_config, &panel));

        esp_lcd_panel_reset(panel);
        esp_lcd_panel_init(panel);
        esp_lcd_panel_invert_color(panel, DISPLAY_INVERT_COLOR);
        esp_lcd_panel_swap_xy(panel, DISPLAY_SWAP_XY);
        esp_lcd_panel_mirror(panel, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y);

        display_ = new SpiLcdDisplay(panel_io, panel,
                                     DISPLAY_WIDTH, DISPLAY_HEIGHT,
                                     DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y,
                                     DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y,
                                     DISPLAY_SWAP_XY);
    }

    void InitializeLedStrip() {
        ESP_LOGI(TAG, "Initializing LED strip (10x SK6812)");
        led_strip_ = new CircularStrip(LED_STRIP_GPIO, LED_STRIP_NUM_LEDS);
    }

    void InitializeButtons() {
        ESP_LOGI(TAG, "Initializing buttons");

        // Button A - Main button for chat toggle
        button_a_.OnClick([this]() {
            ESP_LOGI(TAG, "Button A clicked");
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateStarting) {
                EnterWifiConfigMode();
                return;
            }
            app.ToggleChatState();
        });

        button_a_.OnLongPress([this]() {
            ESP_LOGI(TAG, "Button A long pressed - entering WiFi config mode");
            EnterWifiConfigMode();
        });

        // Button B - Volume down (single click) / Send text message (double click)
        button_b_.OnClick([this]() {
            ESP_LOGI(TAG, "Button B clicked - volume down");
            auto codec = GetAudioCodec();
            int current_volume = codec->output_volume();
            int new_volume = std::max(0, current_volume - 10);
            codec->SetOutputVolume(new_volume);
            ESP_LOGI(TAG, "Volume: %d -> %d", current_volume, new_volume);
        });

        button_b_.OnDoubleClick([this]() {
            ESP_LOGI(TAG, "Button B double clicked - sending text message");
            auto& app = Application::GetInstance();
            app.SendTextMessage("Olá");
        });

        // Button C - Volume up
        button_c_.OnClick([this]() {
            ESP_LOGI(TAG, "Button C clicked - volume up");
            auto codec = GetAudioCodec();
            int current_volume = codec->output_volume();
            int new_volume = std::min(100, current_volume + 10);
            codec->SetOutputVolume(new_volume);
            ESP_LOGI(TAG, "Volume: %d -> %d", current_volume, new_volume);
        });
    }

public:
    M5StackCoreFireBoard() :
        button_a_(BUTTON_A_GPIO),
        button_b_(BUTTON_B_GPIO),
        button_c_(BUTTON_C_GPIO) {

        ESP_LOGI(TAG, "Initializing M5Stack Core Fire board");

        InitializeI2c();
        InitializePowerManagement();
        InitializeSpi();
        InitializeLcdDisplay();
        // InitializeLedStrip();  // Disabled - Node Base has different LED configuration
        InitializeButtons();

        // Restore backlight brightness
        if (DISPLAY_BACKLIGHT_PIN != GPIO_NUM_NC) {
            GetBacklight()->RestoreBrightness();
        }

        // Log battery status
        if (ip5306_) {
            int battery = ip5306_->GetBatteryLevel();
            bool charging = ip5306_->IsCharging();
            ESP_LOGI(TAG, "Battery: %d%% %s", battery, charging ? "(charging)" : "");
        }

        ESP_LOGI(TAG, "M5Stack Core Fire board initialized");
    }

    virtual Led* GetLed() override {
        // LED strip disabled due to hardware conflicts
        static NoLed no_led;
        return &no_led;
    }

    virtual AudioCodec* GetAudioCodec() override {
        static Wm8978AudioCodec audio_codec(
            i2c_bus_,
            AUDIO_INPUT_SAMPLE_RATE,
            AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_MCLK_GPIO,
            AUDIO_I2S_BCK_GPIO,
            AUDIO_I2S_WS_GPIO,
            AUDIO_I2S_DOUT_GPIO,
            AUDIO_I2S_DIN_GPIO,
            AUDIO_PA_CTL_GPIO);
        return &audio_codec;
    }

    virtual Display* GetDisplay() override {
        return display_;
    }

    virtual Backlight* GetBacklight() override {
        if (DISPLAY_BACKLIGHT_PIN != GPIO_NUM_NC) {
            static PwmBacklight backlight(DISPLAY_BACKLIGHT_PIN, DISPLAY_BACKLIGHT_OUTPUT_INVERT);
            return &backlight;
        }
        return nullptr;
    }
};

DECLARE_BOARD(M5StackCoreFireBoard);
