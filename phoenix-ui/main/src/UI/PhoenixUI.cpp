// ==========================================================================
// FILE: src/UI/PhoenixUI.cpp
// LVGL UI Manager — dark theme, status bar, screen routing
// ==========================================================================
#include "phoenix/UI/PhoenixUI.h"
#include "phoenix/UI/CircularMenu.h"
#include "phoenix/IPC/MeasurementProxy.h"
#include <esp_log.h>

namespace phoenix {
static const char* TAG = "UI";

Result<void> PhoenixUI::initialize(lv_disp_t* disp) {
    (void)disp;
    screen_ = lv_obj_create(nullptr);
    lv_scr_load(screen_);
    lv_obj_set_style_bg_color(screen_, lv_color_hex(0x0f0f1e), 0);
    createStatusBar();
    content_ = lv_obj_create(screen_);
    lv_obj_set_size(content_, LV_PCT(100), LV_PCT(88));
    lv_obj_align(content_, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(content_, lv_color_hex(0x0f0f1e), 0);
    lv_obj_set_style_border_width(content_, 0, 0);
    lv_obj_clear_flag(content_, LV_OBJ_FLAG_SCROLLABLE);
    showHome();
    ESP_LOGI(TAG, "UI initialized");
    return Ok();
}

void PhoenixUI::createStatusBar() {
    status_bar_ = lv_obj_create(screen_);
    lv_obj_set_size(status_bar_, LV_PCT(100), 48);
    lv_obj_align(status_bar_, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(status_bar_, lv_color_hex(0x2196F3), 0);
    lv_obj_set_style_border_width(status_bar_, 0, 0);
    lv_obj_clear_flag(status_bar_, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* logo = lv_label_create(status_bar_);
    lv_label_set_text(logo, LV_SYMBOL_FLASH " Phoenix v107");
    lv_obj_align(logo, LV_ALIGN_LEFT_MID, 16, 0);
    lv_obj_set_style_text_color(logo, lv_color_white(), 0);

    lv_obj_t* icons = lv_label_create(status_bar_);
    lv_label_set_text(icons,
        LV_SYMBOL_WIFI " " LV_SYMBOL_BATTERY_3 " " LV_SYMBOL_SETTINGS);
    lv_obj_align(icons, LV_ALIGN_RIGHT_MID, -16, 0);
    lv_obj_set_style_text_color(icons, lv_color_white(), 0);

    // Tap status bar → system menu
    lv_obj_add_flag(status_bar_, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(status_bar_, [](lv_event_t* e) {
        auto* ui = static_cast<PhoenixUI*>(lv_event_get_user_data(e));
        ui->navigateTo(ScreenID::SYSTEM_MENU);
    }, LV_EVENT_CLICKED, this);
}

void PhoenixUI::navigateTo(ScreenID screen) {
    current_ = screen;
    lv_obj_clean(content_);
    switch (screen) {
    case ScreenID::HOME:         showHome(); break;
    case ScreenID::MEASUREMENT:  showMeasurement(); break;
    case ScreenID::CALIBRATION:  showCalibration(); break;
    case ScreenID::PATIENT:      showPatient(); break;
    case ScreenID::SETTINGS:     showSettings(); break;
    case ScreenID::SYSTEM_MENU:  showSystemMenu(); break;
    default: showHome();
    }
}

void PhoenixUI::showHome() {
    using namespace ui;
    static MenuItem items[] = {
        {"Test",      LV_SYMBOL_EDIT,     [](void* c){ static_cast<PhoenixUI*>(c)->navigateTo(ScreenID::MEASUREMENT); }, this, lv_color_hex(0x3ECF5E), "New test"},
        {"Results",   LV_SYMBOL_LIST,     [](void* c){ static_cast<PhoenixUI*>(c)->navigateTo(ScreenID::RESULT); }, this, lv_color_hex(0x2ECC71), "Results"},
        {"Calibrate", LV_SYMBOL_CHART,    [](void* c){ static_cast<PhoenixUI*>(c)->navigateTo(ScreenID::CALIBRATION); }, this, lv_color_hex(0xF1C40F), "Calibrate"},
        {"Patients",  LV_SYMBOL_USER,     [](void* c){ static_cast<PhoenixUI*>(c)->navigateTo(ScreenID::PATIENT); }, this, lv_color_hex(0x1ABC9C), "Patients"},
        {"AI Mode",   LV_SYMBOL_SETTINGS, [](void*){}, nullptr, lv_color_hex(0x42A5F5), "AI analysis"},
        {"Reports",   LV_SYMBOL_FILE,     [](void*){}, nullptr, lv_color_hex(0x9B59B6), "Reports"},
        {"QC",        LV_SYMBOL_OK,       [](void*){}, nullptr, lv_color_hex(0xBB86FC), "Quality control"},
        {"Export",    LV_SYMBOL_USB,      [](void*){}, nullptr, lv_color_hex(0x95A5A6), "Export"},
    };

    auto* menu = new CircularMenu(content_, 110, 40);
    menu->setTitle("Main Menu");
    menu->setCenterIcon(LV_SYMBOL_HOME, lv_color_hex(0x6A5CFF));
    menu->setItems(items, 8);
}

void PhoenixUI::showSystemMenu() {
    using namespace ui;
    static MenuItem items[] = {
        {"Diag",     LV_SYMBOL_WARNING,  [](void*){}, nullptr, lv_color_hex(0xFF9800), "Diagnostics"},
        {"Network",  LV_SYMBOL_WIFI,     [](void* c){ static_cast<PhoenixUI*>(c)->navigateTo(ScreenID::SETTINGS); }, this, lv_color_hex(0x2196F3), "Network"},
        {"Security", LV_SYMBOL_LOCK,     [](void*){}, nullptr, lv_color_hex(0x4CAF50), "Security"},
        {"Updates",  LV_SYMBOL_DOWNLOAD, [](void*){}, nullptr, lv_color_hex(0x9C27B0), "Updates"},
        {"Language", LV_SYMBOL_KEYBOARD, [](void*){}, nullptr, lv_color_hex(0x00BCD4), "Language"},
        {"About",    LV_SYMBOL_INFO,     [](void*){}, nullptr, lv_color_hex(0x607D8B), "About"},
    };
    auto* menu = new CircularMenu(content_, 90, 36);
    menu->setTitle("System");
    menu->setCenterIcon(LV_SYMBOL_SETTINGS, lv_color_hex(0xFF5722));
    menu->setItems(items, 6);
}

// ─── Measurement progress/result integration ──────────────────────────

void PhoenixUI::showMeasurement() {
    // Start measurement on remote MCU
    if (proxy_) proxy_->startMeasurement();

    lv_obj_t* cont = lv_obj_create(content_);
    lv_obj_set_size(cont, LV_PCT(80), LV_SIZE_CONTENT);
    lv_obj_center(cont);
    lv_obj_set_style_bg_opa(cont, LV_OPA_0, 0);
    lv_obj_set_layout(cont, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(cont, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t* lbl = lv_label_create(cont);
    lv_label_set_text(lbl, "Measuring...");
    lv_obj_set_style_text_color(lbl, lv_color_white(), 0);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_18, 0);

    lv_obj_t* bar = lv_bar_create(cont);
    lv_obj_set_size(bar, LV_PCT(100), 30);
    lv_obj_set_style_bg_color(bar, lv_color_hex(0x333333), LV_PART_MAIN);
    lv_obj_set_style_bg_color(bar, lv_color_hex(0x2196F3), LV_PART_INDICATOR);
    lv_bar_set_value(bar, 0, LV_ANIM_ON);

    // Store bar reference for progress updates (via user_data on content)
    lv_obj_set_user_data(content_, bar);

    // Cancel button
    lv_obj_t* btn = lv_btn_create(cont);
    lv_obj_set_size(btn, 120, 40);
    lv_obj_t* btn_lbl = lv_label_create(btn);
    lv_label_set_text(btn_lbl, LV_SYMBOL_CLOSE " Cancel");
    lv_obj_center(btn_lbl);
    lv_obj_add_event_cb(btn, [](lv_event_t* e) {
        auto* ui = static_cast<PhoenixUI*>(lv_event_get_user_data(e));
        if (ui->proxy_) ui->proxy_->cancelMeasurement();
        ui->navigateTo(ScreenID::HOME);
    }, LV_EVENT_CLICKED, this);
}

void PhoenixUI::onMeasurementProgress(uint8_t pct, const char* msg) {
    if (current_ != ScreenID::MEASUREMENT || !content_) return;

    lv_obj_t* bar = static_cast<lv_obj_t*>(lv_obj_get_user_data(content_));
    if (bar) lv_bar_set_value(bar, pct, LV_ANIM_ON);

    // Update label (first child of first child)
    // In production: use proper widget references
    ESP_LOGD(TAG, "Progress: %u%% %s", pct, msg);
}

void PhoenixUI::onMeasurementResult(const MeasurementResult& result) {
    showResult(result);
}

void PhoenixUI::onMeasurementError(ErrorCategory cat, const char* msg) {
    ESP_LOGE(TAG, "Measurement error [%u]: %s", static_cast<unsigned>(cat), msg);
    // Show error dialog
    lv_obj_clean(content_);
    lv_obj_t* lbl = lv_label_create(content_);
    lv_label_set_text_fmt(lbl, LV_SYMBOL_WARNING " Error: %s", msg);
    lv_obj_center(lbl);
    lv_obj_set_style_text_color(lbl, lv_color_hex(0xF44336), 0);
}

void PhoenixUI::showResult(const MeasurementResult& result) {
    current_ = ScreenID::RESULT;
    lv_obj_clean(content_);

    lv_obj_t* card = lv_obj_create(content_);
    lv_obj_set_size(card, LV_PCT(90), LV_PCT(80));
    lv_obj_center(card);
    lv_obj_set_style_radius(card, 20, 0);
    lv_obj_set_style_shadow_width(card, 20, 0);

    // Color by interpretation
    lv_color_t bg = lv_color_hex(0x1a2c1a);
    const char* icon = LV_SYMBOL_OK;
    if (result.interpretation == "POSITIVE" || result.interpretation == "HIGH") {
        bg = lv_color_hex(0x2c1810); icon = LV_SYMBOL_WARNING;
    } else if (result.interpretation == "BORDERLINE" || result.interpretation == "ELEVATED") {
        bg = lv_color_hex(0x2c2310); icon = LV_SYMBOL_WARNING;
    }
    lv_obj_set_style_bg_color(card, bg, 0);

    // Icon + interpretation
    lv_obj_t* h = lv_label_create(card);
    lv_label_set_text_fmt(h, "%s  %s", icon, result.interpretation.c_str());
    lv_obj_align(h, LV_ALIGN_TOP_MID, 0, 10);
    lv_obj_set_style_text_font(h, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(h, lv_color_white(), 0);

    // Concentration
    lv_obj_t* val = lv_label_create(card);
    lv_label_set_text_fmt(val, "%.2f",
                          static_cast<double>(result.concentration_ng_ml));
    lv_obj_align(val, LV_ALIGN_CENTER, -20, -10);
    lv_obj_set_style_text_font(val, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(val, lv_color_white(), 0);

    lv_obj_t* unit = lv_label_create(card);
    lv_label_set_text(unit, result.unit.c_str());
    lv_obj_align_to(unit, val, LV_ALIGN_OUT_RIGHT_MID, 8, 0);
    lv_obj_set_style_text_color(unit, lv_color_hex(0xaaaaaa), 0);

    // Reference range
    lv_obj_t* ref = lv_label_create(card);
    lv_label_set_text_fmt(ref, "Ref: %.1f – %.1f %s",
        static_cast<double>(result.reference_low),
        static_cast<double>(result.reference_high),
        result.unit.c_str());
    lv_obj_align(ref, LV_ALIGN_CENTER, 0, 40);
    lv_obj_set_style_text_color(ref, lv_color_hex(0x888888), 0);

    // Home button
    lv_obj_t* btn = lv_btn_create(card);
    lv_obj_set_size(btn, 120, 40);
    lv_obj_align(btn, LV_ALIGN_BOTTOM_MID, 0, -10);
    lv_obj_t* bl = lv_label_create(btn);
    lv_label_set_text(bl, LV_SYMBOL_HOME " Home");
    lv_obj_center(bl);
    lv_obj_add_event_cb(btn, [](lv_event_t* e) {
        static_cast<PhoenixUI*>(lv_event_get_user_data(e))
            ->navigateTo(ScreenID::HOME);
    }, LV_EVENT_CLICKED, this);
}

// ─── Placeholder screens ──────────────────────────────────────────────
void PhoenixUI::showCalibration() {
    lv_obj_t* lbl = lv_label_create(content_);
    lv_label_set_text(lbl, LV_SYMBOL_CHART " Calibration\n(Coming in Phase 4)");
    lv_obj_center(lbl);
    lv_obj_set_style_text_color(lbl, lv_color_white(), 0);
    lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER, 0);
}

void PhoenixUI::showPatient() {
    lv_obj_t* lbl = lv_label_create(content_);
    lv_label_set_text(lbl, LV_SYMBOL_USER " Patient Management\n(Coming in Phase 4)");
    lv_obj_center(lbl);
    lv_obj_set_style_text_color(lbl, lv_color_white(), 0);
    lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER, 0);
}

void PhoenixUI::showSettings() {
    lv_obj_t* lbl = lv_label_create(content_);
    lv_label_set_text(lbl, LV_SYMBOL_SETTINGS " Settings\n(Coming in Phase 4)");
    lv_obj_center(lbl);
    lv_obj_set_style_text_color(lbl, lv_color_white(), 0);
    lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER, 0);
}

void PhoenixUI::runTick() {
    lv_timer_handler();
}

void PhoenixUI::createTheme() {}

} // namespace phoenix
