// ==========================================================================
// Phoenix v108.0 "Chimera" — ESP32 #1 UI/Connectivity MCU
// LVGL display, WiFi, LIMS, UART IPC to Measurement MCU
// ==========================================================================
#include "phoenix/Core/Result.h"
#include "phoenix/Core/ServiceLocator.h"
#include "phoenix/UI/PhoenixUI.h"
#include "phoenix/IPC/UartBridge.h"
#include "phoenix/IPC/MeasurementProxy.h"
#include <esp_log.h>
#include <esp_timer.h>
#include <nvs_flash.h>
#include <lvgl.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

using namespace phoenix;
static const char* TAG = "UI-MAIN";

// Globals
static UartBridge         g_uart;
static MeasurementProxy*  g_proxy   = nullptr;
static PhoenixUI          g_ui;

// ─── LVGL Display Driver (ST7701S via SPI — Igloo Pro) ────────────────
// In production: implement with esp_lcd_panel_io + ST7701S driver
// For now: stub that LVGL needs

static void lvgl_flush_cb(lv_disp_drv_t* drv, const lv_area_t* area,
                           lv_color_t* color_p) {
    // TODO: Write to ST7701S via SPI/RGB interface
    // For Igloo Pro: 480x480 round display or 320x240 TFT
    lv_disp_flush_ready(drv);
}

static void lvgl_touch_cb(lv_indev_drv_t* drv, lv_indev_data_t* data) {
    // TODO: Read from FT5x06 touch controller via I2C
    data->state = LV_INDEV_STATE_REL;
}

static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf1[320 * 20];  // 20 line buffer
static lv_color_t buf2[320 * 20];
static lv_disp_drv_t disp_drv;
static lv_indev_drv_t touch_drv;

static lv_disp_t* init_lvgl() {
    lv_init();

    lv_disp_draw_buf_init(&draw_buf, buf1, buf2, 320 * 20);

    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res  = 320;
    disp_drv.ver_res  = 240;
    disp_drv.flush_cb = lvgl_flush_cb;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_t* disp = lv_disp_drv_register(&disp_drv);

    lv_indev_drv_init(&touch_drv);
    touch_drv.type    = LV_INDEV_TYPE_POINTER;
    touch_drv.read_cb = lvgl_touch_cb;
    lv_indev_drv_register(&touch_drv);

    return disp;
}

// ─── LVGL Tick (1ms timer) ────────────────────────────────────────────
static void lvgl_tick_cb(void* /*arg*/) {
    lv_tick_inc(1);
}

// ─── UI Task ──────────────────────────────────────────────────────────
static void ui_task(void* /*arg*/) {
    // LVGL tick timer
    const esp_timer_create_args_t tick_args = {
        .callback = lvgl_tick_cb,
        .arg = nullptr,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "lvgl_tick"
    };
    esp_timer_handle_t tick_timer;
    esp_timer_create(&tick_args, &tick_timer);
    esp_timer_start_periodic(tick_timer, 1000);  // 1ms

    while (true) {
        // LVGL rendering
        lv_timer_handler();

        // Poll for UART responses from Measurement MCU
        if (g_proxy) g_proxy->poll(5);

        vTaskDelay(pdMS_TO_TICKS(5));  // ~200 Hz UI refresh
    }
}

// ─── Main Task ────────────────────────────────────────────────────────
static void main_task(void* /*arg*/) {
    // ── NVS ───────────────────────────────────────────────────────
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES ||
        err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }

    // ── UART IPC to Measurement MCU ───────────────────────────────
    UartConfig uart_cfg;
    uart_cfg.uart_num  = 1;
    uart_cfg.tx_pin    = 43;
    uart_cfg.rx_pin    = 44;
    uart_cfg.baud_rate = 921600;
    auto uart_res = g_uart.initialize(uart_cfg);
    if (uart_res.is_err()) {
        ESP_LOGE(TAG, "UART init failed: %s", uart_res.error().message);
    }

    // ── Measurement Proxy ─────────────────────────────────────────
    static MeasurementProxy proxy(&g_uart);
    g_proxy = &proxy;

    // Wire callbacks to UI
    ProxyCallbacks cb;
    cb.ctx = &g_ui;
    cb.on_result = [](const MeasurementResult& r, void* ctx) {
        static_cast<PhoenixUI*>(ctx)->onMeasurementResult(r);
    };
    cb.on_progress = [](uint8_t pct, const char* msg, void* ctx) {
        static_cast<PhoenixUI*>(ctx)->onMeasurementProgress(pct, msg);
    };
    cb.on_error = [](ErrorCategory cat, const char* msg, void* ctx) {
        static_cast<PhoenixUI*>(ctx)->onMeasurementError(cat, msg);
    };
    proxy.setCallbacks(cb);

    // ── Ping Measurement MCU ──────────────────────────────────────
    auto ping = proxy.ping();
    ESP_LOGI(TAG, "Measurement MCU: %s",
             ping.is_ok() ? "CONNECTED" : "NOT RESPONDING");

    // ── Initialize LVGL + Display ─────────────────────────────────
    lv_disp_t* disp = init_lvgl();

    // ── Initialize Phoenix UI ─────────────────────────────────────
    g_ui.setProxy(g_proxy);
    auto ui_res = g_ui.initialize(disp);
    if (ui_res.is_err()) {
        ESP_LOGE(TAG, "UI init failed: %s", ui_res.error().message);
    }

    // ── Services ──────────────────────────────────────────────────
    auto& svc = ServiceLocator::getInstance();
    svc.registerService<UartBridge>("UART", &g_uart);
    svc.registerService<MeasurementProxy>("MeasProxy", g_proxy);
    svc.registerService<PhoenixUI>("UI", &g_ui);

    ESP_LOGI(TAG, "═══════════════════════════════════════════");
    ESP_LOGI(TAG, " Phoenix v108.0 UI MCU READY");
    ESP_LOGI(TAG, " Heap: %.1f kB | Services: %u",
             static_cast<double>(esp_get_free_heap_size()) / 1024.0,
             svc.count());
    ESP_LOGI(TAG, "═══════════════════════════════════════════");

    // ── Start UI task on Core 0 ───────────────────────────────────
    xTaskCreatePinnedToCore(ui_task, "ui_task", 8192, nullptr, 5, nullptr, 0);

    // ── WiFi (optional, background) ───────────────────────────────
    // WiFiManager init would go here
    // For now: WiFi is configured via Settings screen in Phase 4

    // This task can terminate — UI task runs forever
    vTaskDelete(nullptr);
}

// ─── ESP-IDF Entry ────────────────────────────────────────────────────
extern "C" void app_main() {
    ESP_LOGI(TAG, "Phoenix v108.0 Chimera — UI MCU booting...");
    xTaskCreatePinnedToCore(main_task, "main", 12288, nullptr, 5, nullptr, 0);
}
