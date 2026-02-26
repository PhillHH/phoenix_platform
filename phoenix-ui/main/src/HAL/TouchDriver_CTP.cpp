// ==========================================================================
// FILE: src/HAL/TouchDriver_CTP.cpp
// Phoenix v108.0 — Capacitive touch for Igloo Pro (T-RGB 2.1")
// Touch chips: CST820 (Full Circle) or FT3267 (Half Circle)
// I2C shared with XL9535: SDA=GPIO8, SCL=GPIO48
// TP_INT=GPIO1, TP_RST via XL9535 P1
// ==========================================================================
#include <esp_log.h>
#include <driver/gpio.h>
#include <driver/i2c.h>
#include <lvgl.h>

namespace phoenix {

static const char* TAG = "TouchCTP";

// I2C bus already initialized by DisplayDriver_ST7701S
static constexpr i2c_port_t I2C_PORT = I2C_NUM_0;
static constexpr gpio_num_t PIN_TP_INT = GPIO_NUM_1;

// Known touch controller I2C addresses
static constexpr uint8_t CST816_ADDR = 0x15;
static constexpr uint8_t FT5x06_ADDR = 0x38;

enum class TouchChip { NONE, CST816, FT5x06 };
static TouchChip s_chip = TouchChip::NONE;

// ── I2C read helper ───────────────────────────────────────────────────
static esp_err_t i2c_read(uint8_t addr, uint8_t reg, uint8_t* buf, size_t len) {
    return i2c_master_write_read_device(I2C_PORT, addr, &reg, 1, buf, len,
                                        pdMS_TO_TICKS(50));
}

// ── Detect touch controller ───────────────────────────────────────────
static TouchChip detect() {
    uint8_t id = 0;
    // Try CST816/CST820 (addr 0x15, chip ID at reg 0xA7)
    if (i2c_read(CST816_ADDR, 0xA7, &id, 1) == ESP_OK) {
        ESP_LOGI(TAG, "CST8xx detected (ID=0x%02X) at 0x%02X", id, CST816_ADDR);
        return TouchChip::CST816;
    }
    // Try FT3267/FT5x06 (addr 0x38, vendor ID at reg 0xA8)
    if (i2c_read(FT5x06_ADDR, 0xA8, &id, 1) == ESP_OK) {
        ESP_LOGI(TAG, "FT5x06 detected (ID=0x%02X) at 0x%02X", id, FT5x06_ADDR);
        return TouchChip::FT5x06;
    }
    ESP_LOGW(TAG, "No touch controller detected!");
    return TouchChip::NONE;
}

// ── Read CST816/CST820 ───────────────────────────────────────────────
static bool readCST816(uint16_t &x, uint16_t &y) {
    uint8_t buf[6];
    if (i2c_read(CST816_ADDR, 0x01, buf, 6) != ESP_OK) return false;
    uint8_t points = buf[1];
    if (points == 0) return false;
    x = ((buf[2] & 0x0F) << 8) | buf[3];
    y = ((buf[4] & 0x0F) << 8) | buf[5];
    return (x < 480 && y < 480);
}

// ── Read FT3267/FT5x06 ───────────────────────────────────────────────
static bool readFT5x06(uint16_t &x, uint16_t &y) {
    uint8_t buf[7];
    if (i2c_read(FT5x06_ADDR, 0x00, buf, 7) != ESP_OK) return false;
    uint8_t points = buf[2] & 0x0F;
    if (points == 0) return false;
    x = ((buf[3] & 0x0F) << 8) | buf[4];
    y = ((buf[5] & 0x0F) << 8) | buf[6];
    return (x < 480 && y < 480);
}

// ── LVGL touch callback ──────────────────────────────────────────────
static void lvgl_touch_cb(lv_indev_drv_t* drv, lv_indev_data_t* data) {
    uint16_t x = 0, y = 0;
    bool pressed = false;

    switch (s_chip) {
        case TouchChip::CST816: pressed = readCST816(x, y); break;
        case TouchChip::FT5x06: pressed = readFT5x06(x, y); break;
        default: break;
    }

    data->state = pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
    data->point.x = x;
    data->point.y = y;
}

// ══════════════════════════════════════════════════════════════════════
// Public: Initialize touch subsystem
// NOTE: I2C bus and XL9535 touch reset already done by displayInit()
// ══════════════════════════════════════════════════════════════════════
void touchInit() {
    // Configure interrupt pin
    gpio_config_t io = {};
    io.pin_bit_mask = 1ULL << PIN_TP_INT;
    io.mode = GPIO_MODE_INPUT;
    io.pull_up_en = GPIO_PULLUP_ENABLE;
    gpio_config(&io);

    // Auto-detect touch controller
    s_chip = detect();
    if (s_chip == TouchChip::NONE) return;

    // Register LVGL input device
    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = lvgl_touch_cb;
    lv_indev_drv_register(&indev_drv);

    ESP_LOGI(TAG, "Touch init complete (INT=GPIO%d)", PIN_TP_INT);
}

} // namespace phoenix
