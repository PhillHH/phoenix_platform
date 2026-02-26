// ==========================================================================
// FILE: src/UI/Screens/SettingsScreen.cpp
// Phoenix v108.0 — Settings screen
// Language, WiFi, LIMS, A11y, Diagnostics, Factory Reset, About
// ==========================================================================
#include "phoenix/UI/PhoenixUI.h"
#include <esp_log.h>
#include <esp_system.h>
#include <esp_mac.h>
#include <lvgl.h>

namespace phoenix {

static const char* TAG = "SettingsScr";

// ─── Setting Row Helper ──────────────────────────────────────────────
static lv_obj_t* addSettingRow(lv_obj_t* parent, const char* icon,
                                 const char* label, const char* value) {
    lv_obj_t* row = lv_obj_create(parent);
    lv_obj_set_size(row, LV_PCT(100), 44);
    lv_obj_set_style_bg_color(row, lv_color_hex(0x111122), 0);
    lv_obj_set_style_radius(row, 8, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t* ic = lv_label_create(row);
    lv_label_set_text(ic, icon);
    lv_obj_align(ic, LV_ALIGN_LEFT_MID, 10, 0);
    lv_obj_set_style_text_color(ic, lv_color_hex(0x888888), 0);

    lv_obj_t* lbl = lv_label_create(row);
    lv_label_set_text(lbl, label);
    lv_obj_align(lbl, LV_ALIGN_LEFT_MID, 36, 0);
    lv_obj_set_style_text_color(lbl, lv_color_hex(0xDDDDDD), 0);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);

    if (value) {
        lv_obj_t* val = lv_label_create(row);
        lv_label_set_text(val, value);
        lv_obj_align(val, LV_ALIGN_RIGHT_MID, -10, 0);
        lv_obj_set_style_text_color(val, lv_color_hex(0x888888), 0);
        lv_obj_set_style_text_font(val, &lv_font_montserrat_14, 0);
    }

    // Chevron
    lv_obj_t* chev = lv_label_create(row);
    lv_label_set_text(chev, LV_SYMBOL_RIGHT);
    lv_obj_align(chev, LV_ALIGN_RIGHT_MID, -4, 0);
    lv_obj_set_style_text_color(chev, lv_color_hex(0x444444), 0);

    return row;
}

// ─── Section Header ──────────────────────────────────────────────────
static void addSectionHeader(lv_obj_t* parent, const char* text) {
    lv_obj_t* lbl = lv_label_create(parent);
    lv_label_set_text(lbl, text);
    lv_obj_set_style_text_color(lbl, lv_color_hex(0x555577), 0);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
    lv_obj_set_style_pad_left(lbl, 4, 0);
    lv_obj_set_style_pad_top(lbl, 8, 0);
}

void createSettingsScreen(lv_obj_t* content, PhoenixUI* ui) {
    // Title
    lv_obj_t* title = lv_label_create(content);
    lv_label_set_text(title, LV_SYMBOL_SETTINGS " Einstellungen");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 4);
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_18, 0);

    // Scrollable list
    lv_obj_t* list = lv_obj_create(content);
    lv_obj_set_size(list, LV_PCT(92), LV_PCT(80));
    lv_obj_align(list, LV_ALIGN_TOP_MID, 0, 32);
    lv_obj_set_style_bg_opa(list, LV_OPA_0, 0);
    lv_obj_set_style_border_width(list, 0, 0);
    lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_gap(list, 4, 0);
    lv_obj_set_style_pad_all(list, 0, 0);

    // ── Allgemein ─────────────────────────────────────────────────
    addSectionHeader(list, "ALLGEMEIN");
    addSettingRow(list, LV_SYMBOL_KEYBOARD, "Sprache", "Deutsch");
    addSettingRow(list, LV_SYMBOL_EYE_OPEN, "Barrierefreiheit", "");
    addSettingRow(list, LV_SYMBOL_BELL,     "Benachrichtigungen", "Ein");

    // ── Konnektivität ─────────────────────────────────────────────
    addSectionHeader(list, "KONNEKTIVITÄT");
    addSettingRow(list, LV_SYMBOL_WIFI,     "WLAN", "Nicht verbunden");
    addSettingRow(list, LV_SYMBOL_UPLOAD,   "LIMS Server", "Nicht konfiguriert");
    addSettingRow(list, LV_SYMBOL_LOOP,     "Auto-Sync", "Aus");

    // ── Gerät ─────────────────────────────────────────────────────
    addSectionHeader(list, "GERÄT");
    addSettingRow(list, LV_SYMBOL_WARNING,  "Diagnose", "");
    addSettingRow(list, LV_SYMBOL_DOWNLOAD, "Firmware Update", "v108.0");

    // Device ID from MAC
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    char mac_str[20];
    snprintf(mac_str, sizeof(mac_str), "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    addSettingRow(list, LV_SYMBOL_USB,      "Geräte-ID", mac_str);

    // ── Daten ─────────────────────────────────────────────────────
    addSectionHeader(list, "DATEN");
    addSettingRow(list, LV_SYMBOL_TRASH,    "Ergebnisse löschen", "");
    addSettingRow(list, LV_SYMBOL_TRASH,    "Patienten löschen", "");

    // Factory reset (danger zone)
    lv_obj_t* reset_row = addSettingRow(list, LV_SYMBOL_CLOSE, "Werksreset", "");
    lv_obj_set_style_bg_color(reset_row, lv_color_hex(0x2a1111), 0);
    lv_obj_add_event_cb(reset_row, [](lv_event_t*) {
        ESP_LOGW("Settings", "Factory reset requested");
        // In production: confirmation dialog + nvs_flash_erase + reboot
    }, LV_EVENT_CLICKED, nullptr);

    // ── Über ──────────────────────────────────────────────────────
    addSectionHeader(list, "ÜBER");
    addSettingRow(list, LV_SYMBOL_INFO, "Phoenix AI / Igloo Pro", "v108.0");
    addSettingRow(list, LV_SYMBOL_FILE, "Polaris Diagnostics Europe UG", "");
    addSettingRow(list, LV_SYMBOL_OK,   "IEC 62304 Class C · CE-IVD", "");

    // ── Back ──────────────────────────────────────────────────────
    lv_obj_t* back = lv_btn_create(content);
    lv_obj_set_size(back, 100, 36);
    lv_obj_align(back, LV_ALIGN_BOTTOM_LEFT, 12, -4);
    lv_obj_set_style_bg_color(back, lv_color_hex(0x1a1a1a), 0);
    lv_obj_t* bb = lv_label_create(back);
    lv_label_set_text(bb, LV_SYMBOL_LEFT " Zurück");
    lv_obj_center(bb);
    lv_obj_set_style_text_color(bb, lv_color_hex(0x888888), 0);
    lv_obj_add_event_cb(back, [](lv_event_t* e) {
        static_cast<PhoenixUI*>(lv_event_get_user_data(e))
            ->navigateTo(ScreenID::HOME);
    }, LV_EVENT_CLICKED, ui);

    ESP_LOGI(TAG, "Settings screen ready");
}

} // namespace phoenix
