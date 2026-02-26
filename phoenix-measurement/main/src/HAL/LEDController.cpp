// ==========================================================================
// FILE: src/HAL/LEDController.cpp
// LED Controller — Igloo Pro RGB ring + white illumination LED
// Uses LEDC PWM for dimming control
// ==========================================================================

#include "phoenix/HAL/Interfaces.h"
#include <esp_log.h>
#include <driver/ledc.h>
#include <driver/gpio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace phoenix {

static const char* TAG = "LED";

// ─── Igloo Pro LED pin mapping ────────────────────────────────────────
// From Product Spec: RGB LED status indication ring
static constexpr gpio_num_t PIN_LED_WHITE = GPIO_NUM_4;    // White illumination
static constexpr gpio_num_t PIN_LED_R     = GPIO_NUM_2;    // RGB ring red
static constexpr gpio_num_t PIN_LED_G     = GPIO_NUM_14;   // RGB ring green (GPIO12 is strapping pin)
static constexpr gpio_num_t PIN_LED_B     = GPIO_NUM_13;   // RGB ring blue

// LEDC channels (separate from camera XCLK which uses CH0)
static constexpr ledc_channel_t CH_WHITE = LEDC_CHANNEL_1;
static constexpr ledc_channel_t CH_R     = LEDC_CHANNEL_2;
static constexpr ledc_channel_t CH_G     = LEDC_CHANNEL_3;
static constexpr ledc_channel_t CH_B     = LEDC_CHANNEL_4;

static constexpr ledc_timer_t   LED_TIMER = LEDC_TIMER_1;
static constexpr uint32_t       LED_FREQ  = 5000;  // 5 kHz PWM

class IglooLEDController : public ILEDController {
public:
    Result<void> initialize() override {
        if (initialized_) return Ok();

        // Timer shared by all LED channels
        ledc_timer_config_t timer_cfg = {};
        timer_cfg.speed_mode      = LEDC_LOW_SPEED_MODE;
        timer_cfg.duty_resolution = LEDC_TIMER_8_BIT;  // 0-255
        timer_cfg.timer_num       = LED_TIMER;
        timer_cfg.freq_hz         = LED_FREQ;
        timer_cfg.clk_cfg         = LEDC_AUTO_CLK;

        esp_err_t err = ledc_timer_config(&timer_cfg);
        if (err != ESP_OK) {
            return Err(ErrorCategory::HARDWARE_FAILURE,
                       "LED timer config failed", static_cast<uint32_t>(err));
        }

        // Configure each channel
        struct { gpio_num_t pin; ledc_channel_t ch; } channels[] = {
            {PIN_LED_WHITE, CH_WHITE},
            {PIN_LED_R,     CH_R},
            {PIN_LED_G,     CH_G},
            {PIN_LED_B,     CH_B},
        };

        for (auto& c : channels) {
            ledc_channel_config_t ch_cfg = {};
            ch_cfg.gpio_num   = c.pin;
            ch_cfg.speed_mode = LEDC_LOW_SPEED_MODE;
            ch_cfg.channel    = c.ch;
            ch_cfg.intr_type  = LEDC_INTR_DISABLE;
            ch_cfg.timer_sel  = LED_TIMER;
            ch_cfg.duty       = 0;
            ch_cfg.hpoint     = 0;

            err = ledc_channel_config(&ch_cfg);
            if (err != ESP_OK) {
                return Err(ErrorCategory::HARDWARE_FAILURE,
                           "LED channel config failed",
                           static_cast<uint32_t>(err));
            }
        }

        initialized_ = true;
        ESP_LOGI(TAG, "LED controller initialized (4 channels)");
        return Ok();
    }

    Result<void> setMode(const LEDConfig& cfg) override {
        if (!initialized_) {
            return Err(ErrorCategory::NOT_INITIALIZED, "LED not init");
        }

        // Turn everything off first
        setDuty(CH_WHITE, 0);
        setDuty(CH_R, 0);
        setDuty(CH_G, 0);
        setDuty(CH_B, 0);

        switch (cfg.mode) {
        case LEDMode::OFF:
            break;

        case LEDMode::WHITE:
            setDuty(CH_WHITE, cfg.intensity);
            ESP_LOGD(TAG, "White LED: %u/255", cfg.intensity);
            break;

        case LEDMode::UV_365NM:
            // UV LED shares white channel on Igloo Pro (hardware limitation)
            // In production: separate UV LED driver
            setDuty(CH_WHITE, cfg.intensity);
            ESP_LOGD(TAG, "UV LED: %u/255", cfg.intensity);
            break;

        case LEDMode::STATUS_RGB:
            setDuty(CH_R, cfg.r);
            setDuty(CH_G, cfg.g);
            setDuty(CH_B, cfg.b);
            ESP_LOGD(TAG, "RGB: (%u,%u,%u)", cfg.r, cfg.g, cfg.b);
            break;
        }

        current_mode_ = cfg;
        return Ok();
    }

    Result<void> selfTest() override {
        if (!initialized_) {
            return Err(ErrorCategory::NOT_INITIALIZED, "LED not init");
        }

        // Flash each color briefly
        LEDConfig cfg;

        // Red
        cfg.mode = LEDMode::STATUS_RGB;
        cfg.r = 50; cfg.g = 0; cfg.b = 0;
        setMode(cfg);
        vTaskDelay(pdMS_TO_TICKS(100));

        // Green
        cfg.r = 0; cfg.g = 50; cfg.b = 0;
        setMode(cfg);
        vTaskDelay(pdMS_TO_TICKS(100));

        // Blue
        cfg.r = 0; cfg.g = 0; cfg.b = 50;
        setMode(cfg);
        vTaskDelay(pdMS_TO_TICKS(100));

        // White
        cfg.mode = LEDMode::WHITE;
        cfg.intensity = 50;
        setMode(cfg);
        vTaskDelay(pdMS_TO_TICKS(100));

        off();

        ESP_LOGI(TAG, "Self-test OK");
        return Ok();
    }

    void off() override {
        setDuty(CH_WHITE, 0);
        setDuty(CH_R, 0);
        setDuty(CH_G, 0);
        setDuty(CH_B, 0);
        current_mode_.mode = LEDMode::OFF;
    }

private:
    bool      initialized_ = false;
    LEDConfig current_mode_ = {};

    void setDuty(ledc_channel_t ch, uint8_t duty) {
        ledc_set_duty(LEDC_LOW_SPEED_MODE, ch, duty);
        ledc_update_duty(LEDC_LOW_SPEED_MODE, ch);
    }
};

// Singleton instance
static IglooLEDController s_led_controller;

ILEDController* getIglooLEDController() {
    return &s_led_controller;
}

} // namespace phoenix
