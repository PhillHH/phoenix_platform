// ==========================================================================
// FILE: src/HAL/DisplayDriver_ST7701S.cpp
// Phoenix v108.0 — ST7701S driver for Igloo Pro (Dx365 Reader)
// Hardware: T-RGB 2.1" Round IPS 480x480, ST7701S + XL9535 I/O Expander
// CONFIRMED from firmware binary reverse-engineering + LilyGo T-RGB SDK
// ==========================================================================
#include "phoenix/Core/Result.h"
#include <esp_log.h>
#include <esp_lcd_panel_rgb.h>
#include <esp_lcd_panel_ops.h>
#include <driver/gpio.h>
#include <driver/i2c.h>
#include <driver/ledc.h>
#include <esp_heap_caps.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <lvgl.h>
#include <string.h>

namespace phoenix {

static const char* TAG = "ST7701S";

// ══════════════════════════════════════════════════════════════════════
// XL9535 I2C I/O Expander — controls SPI init, LCD reset, LCD power
// The ESP32-S3 doesn't have enough GPIOs for RGB parallel + SPI + touch
// ══════════════════════════════════════════════════════════════════════
static constexpr uint8_t    XL9535_ADDR   = 0x20;
static constexpr i2c_port_t I2C_PORT      = I2C_NUM_0;
static constexpr gpio_num_t PIN_I2C_SDA   = GPIO_NUM_8;
static constexpr gpio_num_t PIN_I2C_SCL   = GPIO_NUM_48;

// XL9535 pin assignments (active low where noted)
static constexpr uint8_t XL_TP_RST       = 1;  // Touch reset
static constexpr uint8_t XL_LCD_PWR_EN   = 2;  // LCD power enable (active low)
static constexpr uint8_t XL_LCD_SPI_SDA  = 4;  // ST7701S SPI data
static constexpr uint8_t XL_LCD_SPI_SCK  = 5;  // ST7701S SPI clock
static constexpr uint8_t XL_LCD_RST      = 6;  // ST7701S reset (active low)
static constexpr uint8_t XL_SD_CS        = 7;  // SD card CS

// XL9535 registers
static constexpr uint8_t XL9535_OUTPUT_PORT0 = 0x02;
static constexpr uint8_t XL9535_CONFIG_PORT0 = 0x06;
static constexpr uint8_t XL9535_CONFIG_PORT1 = 0x07;

// ── ESP32-S3 Direct GPIO — RGB Parallel Interface ────────────────────
static constexpr gpio_num_t PIN_DE    = GPIO_NUM_2;
static constexpr gpio_num_t PIN_VSYNC = GPIO_NUM_17;
static constexpr gpio_num_t PIN_HSYNC = GPIO_NUM_16;
static constexpr gpio_num_t PIN_PCLK  = GPIO_NUM_21;

// RGB565 data bus: B[4:0], G[5:0], R[4:0]
static constexpr int DATA_PINS[16] = {
    19, 20, 40, 9, 10,         // B4..B0
    46, 11, 12, 13, 14, 15,    // G5..G0
    7, 6, 5, 4, 3              // R4..R0
};

// ── Backlight (LEDC PWM) ──────────────────────────────────────────────
static constexpr gpio_num_t PIN_BL = GPIO_NUM_46;

// ── State ─────────────────────────────────────────────────────────────
static esp_lcd_panel_handle_t s_panel = nullptr;
static lv_disp_drv_t          s_disp_drv;
static lv_disp_draw_buf_t     s_draw_buf;
static uint8_t                s_xl_port0 = 0xFF;

// ── XL9535 helpers ────────────────────────────────────────────────────
static esp_err_t xl_write(uint8_t reg, uint8_t val) {
    uint8_t buf[2] = {reg, val};
    return i2c_master_write_to_device(I2C_PORT, XL9535_ADDR, buf, 2,
                                      pdMS_TO_TICKS(100));
}

static void xl_pin(uint8_t pin, bool high) {
    if (high) s_xl_port0 |=  (1 << pin);
    else      s_xl_port0 &= ~(1 << pin);
    xl_write(XL9535_OUTPUT_PORT0, s_xl_port0);
}

static void xl_init() {
    uint8_t out_mask = (1<<XL_SD_CS)|(1<<XL_LCD_RST)|(1<<XL_LCD_SPI_SCK)|
                       (1<<XL_LCD_SPI_SDA)|(1<<XL_LCD_PWR_EN)|(1<<XL_TP_RST);
    xl_write(XL9535_CONFIG_PORT0, ~out_mask & 0xFF);
    s_xl_port0 = 0xFF;
    xl_write(XL9535_OUTPUT_PORT0, s_xl_port0);
    xl_write(XL9535_CONFIG_PORT1, 0xFF);
    ESP_LOGI(TAG, "XL9535 init OK (addr 0x%02X)", XL9535_ADDR);
}

// ── 9-bit SPI via XL9535 ──────────────────────────────────────────────
static void spi9(uint16_t d9) {
    for (int i = 8; i >= 0; i--) {
        xl_pin(XL_LCD_SPI_SCK, false);
        xl_pin(XL_LCD_SPI_SDA, (d9 >> i) & 1);
        xl_pin(XL_LCD_SPI_SCK, true);
    }
}
static void cmd(uint8_t c) { spi9(0x000 | c); }
static void dat(uint8_t d) { spi9(0x100 | d); }
static void cmd_data(uint8_t c, const uint8_t* d, size_t n) {
    cmd(c); for (size_t i = 0; i < n; i++) dat(d[i]);
}

// ── ST7701S Init ──────────────────────────────────────────────────────
static void st7701s_init() {
    cmd(0x01); vTaskDelay(pdMS_TO_TICKS(150));
    cmd(0x11); vTaskDelay(pdMS_TO_TICKS(120));
    // Command2 BK0
    { uint8_t d[]={0x77,0x01,0x00,0x00,0x10}; cmd_data(0xFF,d,5); }
    { uint8_t d[]={0xE9,0x03}; cmd_data(0xC0,d,2); }
    { uint8_t d[]={0x11,0x02}; cmd_data(0xC1,d,2); }
    { uint8_t d[]={0x31,0x08}; cmd_data(0xC2,d,2); }
    { uint8_t d[]={0x10};      cmd_data(0xCC,d,1); }
    { uint8_t d[]={0x40,0x02,0x87,0x09,0x12,0x07,0x02,0x09,
                   0x09,0x1F,0x07,0x15,0x12,0x4C,0x10,0xC8};
      cmd_data(0xB0,d,16); }
    { uint8_t d[]={0x40,0x02,0x87,0x09,0x12,0x07,0x02,0x09,
                   0x09,0x1F,0x07,0x15,0x12,0x4C,0x10,0xC8};
      cmd_data(0xB1,d,16); }
    // Command2 BK1 — Power
    { uint8_t d[]={0x77,0x01,0x00,0x00,0x11}; cmd_data(0xFF,d,5); }
    { uint8_t d[]={0x6D}; cmd_data(0xB0,d,1); }
    { uint8_t d[]={0x37}; cmd_data(0xB1,d,1); }
    { uint8_t d[]={0x81}; cmd_data(0xB2,d,1); }
    { uint8_t d[]={0x80}; cmd_data(0xB3,d,1); }
    { uint8_t d[]={0x43}; cmd_data(0xB5,d,1); }
    { uint8_t d[]={0x85}; cmd_data(0xB7,d,1); }
    { uint8_t d[]={0x20}; cmd_data(0xB8,d,1); }
    { uint8_t d[]={0x78}; cmd_data(0xC1,d,1); }
    { uint8_t d[]={0x78}; cmd_data(0xC2,d,1); }
    { uint8_t d[]={0x88}; cmd_data(0xD0,d,1); }
    // GIP timing
    { uint8_t d[]={0x00,0x00,0x02}; cmd_data(0xE0,d,3); }
    { uint8_t d[]={0x06,0x30,0x08,0x30,0x05,0x30,0x07,0x30,0x00,0x33,0x33};
      cmd_data(0xE1,d,11); }
    { uint8_t d[]={0x11,0x11,0x33,0x33,0xF4,0x00,0x00,0x00,0xF4,0x00,0x00,0x00};
      cmd_data(0xE2,d,12); }
    { uint8_t d[]={0x00,0x00,0x11,0x11}; cmd_data(0xE3,d,4); }
    { uint8_t d[]={0x44,0x44};           cmd_data(0xE4,d,2); }
    { uint8_t d[]={0x0D,0xF5,0x30,0xF0,0x0F,0xF7,0x30,0xF0,
                   0x09,0xF1,0x30,0xF0,0x0B,0xF3,0x30,0xF0};
      cmd_data(0xE5,d,16); }
    { uint8_t d[]={0x00,0x00,0x11,0x11}; cmd_data(0xE6,d,4); }
    { uint8_t d[]={0x44,0x44};           cmd_data(0xE7,d,2); }
    { uint8_t d[]={0x0C,0xF4,0x30,0xF0,0x0E,0xF6,0x30,0xF0,
                   0x08,0xF0,0x30,0xF0,0x0A,0xF2,0x30,0xF0};
      cmd_data(0xE8,d,16); }
    { uint8_t d[]={0x02,0x01}; cmd_data(0xEB,d,2); }
    { uint8_t d[]={0x02,0x01}; cmd_data(0xEC,d,2); }
    { uint8_t d[]={0xAB,0x89,0x76,0x54,0x01,0xFF,0xFF,0xFF,
                   0xFF,0x10,0x45,0x67,0x98,0xBA};
      cmd_data(0xED,d,14); }
    // Exit Command2
    { uint8_t d[]={0x77,0x01,0x00,0x00,0x00}; cmd_data(0xFF,d,5); }
    { uint8_t d[]={0x55}; cmd_data(0x3A,d,1); }  // RGB565
    cmd(0x29); vTaskDelay(pdMS_TO_TICKS(20));
    ESP_LOGI(TAG, "ST7701S init complete (via XL9535 SPI)");
}

// ── LVGL flush ────────────────────────────────────────────────────────
static void lvgl_flush_cb(lv_disp_drv_t* drv, const lv_area_t* a, lv_color_t* px) {
    esp_lcd_panel_draw_bitmap(s_panel, a->x1, a->y1, a->x2+1, a->y2+1, px);
    lv_disp_flush_ready(drv);
}

// ── I2C bus init ──────────────────────────────────────────────────────
static void i2c_init() {
    i2c_config_t c = {};
    c.mode = I2C_MODE_MASTER;
    c.sda_io_num = PIN_I2C_SDA;
    c.scl_io_num = PIN_I2C_SCL;
    c.sda_pullup_en = GPIO_PULLUP_ENABLE;
    c.scl_pullup_en = GPIO_PULLUP_ENABLE;
    c.master.clk_speed = 400000;
    ESP_ERROR_CHECK(i2c_param_config(I2C_PORT, &c));
    ESP_ERROR_CHECK(i2c_driver_install(I2C_PORT, I2C_MODE_MASTER, 0, 0, 0));
    ESP_LOGI(TAG, "I2C: SDA=%d SCL=%d 400kHz", PIN_I2C_SDA, PIN_I2C_SCL);
}

// ── Backlight ─────────────────────────────────────────────────────────
static void bl_init() {
    ledc_timer_config_t t = {};
    t.speed_mode = LEDC_LOW_SPEED_MODE;
    t.timer_num = LEDC_TIMER_0;
    t.duty_resolution = LEDC_TIMER_8_BIT;
    t.freq_hz = 1000;
    t.clk_cfg = LEDC_AUTO_CLK;
    ESP_ERROR_CHECK(ledc_timer_config(&t));
    ledc_channel_config_t ch = {};
    ch.speed_mode = LEDC_LOW_SPEED_MODE;
    ch.channel = LEDC_CHANNEL_0;
    ch.timer_sel = LEDC_TIMER_0;
    ch.gpio_num = PIN_BL;
    ch.duty = 200;
    ESP_ERROR_CHECK(ledc_channel_config(&ch));
    ESP_LOGI(TAG, "Backlight: GPIO %d, duty=%d", PIN_BL, 200);
}

// ══════════════════════════════════════════════════════════════════════
// Public API
// ══════════════════════════════════════════════════════════════════════
void displayInit() {
    ESP_LOGI(TAG, "=== Igloo Pro Display Init (T-RGB 2.1\" 480x480) ===");
    i2c_init();
    xl_init();
    // Power + reset
    xl_pin(XL_LCD_PWR_EN, false); vTaskDelay(pdMS_TO_TICKS(20));
    xl_pin(XL_LCD_RST, false);    vTaskDelay(pdMS_TO_TICKS(20));
    xl_pin(XL_LCD_RST, true);     vTaskDelay(pdMS_TO_TICKS(50));
    xl_pin(XL_TP_RST, false);     vTaskDelay(pdMS_TO_TICKS(20));
    xl_pin(XL_TP_RST, true);      vTaskDelay(pdMS_TO_TICKS(50));
    // ST7701S init via XL9535 SPI
    st7701s_init();
    // RGB panel
    esp_lcd_rgb_panel_config_t pc = {};
    pc.clk_src = LCD_CLK_SRC_DEFAULT;
    pc.timings.pclk_hz = 16000000;
    pc.timings.h_res = 480; pc.timings.v_res = 480;
    pc.timings.hsync_pulse_width=10; pc.timings.hsync_back_porch=50;
    pc.timings.hsync_front_porch=10; pc.timings.vsync_pulse_width=10;
    pc.timings.vsync_back_porch=20;  pc.timings.vsync_front_porch=10;
    pc.timings.flags.pclk_active_neg = 1;
    pc.data_width = 16;
    pc.hsync_gpio_num = PIN_HSYNC; pc.vsync_gpio_num = PIN_VSYNC;
    pc.de_gpio_num = PIN_DE; pc.pclk_gpio_num = PIN_PCLK;
    pc.disp_gpio_num = -1;
    for (int i = 0; i < 16; i++) pc.data_gpio_nums[i] = DATA_PINS[i];
    pc.flags.fb_in_psram = 1;
    pc.bounce_buffer_size_px = 480 * 10;
    pc.psram_trans_align = 64; pc.sram_trans_align = 4;
    ESP_ERROR_CHECK(esp_lcd_new_rgb_panel(&pc, &s_panel));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(s_panel));
    ESP_ERROR_CHECK(esp_lcd_panel_init(s_panel));
    ESP_LOGI(TAG, "RGB panel OK (480x480 @16MHz)");
    bl_init();
    // LVGL
    auto* b1 = (lv_color_t*)heap_caps_malloc(480*40*sizeof(lv_color_t), MALLOC_CAP_SPIRAM);
    auto* b2 = (lv_color_t*)heap_caps_malloc(480*40*sizeof(lv_color_t), MALLOC_CAP_SPIRAM);
    if (!b1||!b2) { ESP_LOGE(TAG, "LVGL buf alloc fail!"); return; }
    lv_disp_draw_buf_init(&s_draw_buf, b1, b2, 480*40);
    lv_disp_drv_init(&s_disp_drv);
    s_disp_drv.hor_res=480; s_disp_drv.ver_res=480;
    s_disp_drv.flush_cb=lvgl_flush_cb; s_disp_drv.draw_buf=&s_draw_buf;
    lv_disp_drv_register(&s_disp_drv);
    ESP_LOGI(TAG, "=== Display init complete ===");
}

} // namespace phoenix
