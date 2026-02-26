// ==========================================================================
// FILE: src/UI/Screens/PatientScreen.cpp
// Phoenix v108.0 — Patient management screen
// List, search, add, select for measurement linkage
// ==========================================================================
#include "phoenix/UI/PhoenixUI.h"
#include <esp_log.h>
#include <lvgl.h>

namespace phoenix {

static const char* TAG = "PatScr";

void createPatientScreen(lv_obj_t* content, PhoenixUI* ui) {
    // Title
    lv_obj_t* title = lv_label_create(content);
    lv_label_set_text(title, LV_SYMBOL_USER " Patienten");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 4);
    lv_obj_set_style_text_color(title, lv_color_hex(0x1ABC9C), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_18, 0);

    // ── Search Bar ────────────────────────────────────────────────
    lv_obj_t* search_row = lv_obj_create(content);
    lv_obj_set_size(search_row, LV_PCT(90), 40);
    lv_obj_align(search_row, LV_ALIGN_TOP_MID, 0, 32);
    lv_obj_set_style_bg_color(search_row, lv_color_hex(0x111122), 0);
    lv_obj_set_style_radius(search_row, 8, 0);
    lv_obj_set_style_border_color(search_row, lv_color_hex(0x222244), 0);
    lv_obj_set_style_border_width(search_row, 1, 0);
    lv_obj_clear_flag(search_row, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* search_icon = lv_label_create(search_row);
    lv_label_set_text(search_icon, LV_SYMBOL_EYE_OPEN);
    lv_obj_align(search_icon, LV_ALIGN_LEFT_MID, 8, 0);
    lv_obj_set_style_text_color(search_icon, lv_color_hex(0x555555), 0);

    lv_obj_t* search_lbl = lv_label_create(search_row);
    lv_label_set_text(search_lbl, "Suche (ID / Name)…");
    lv_obj_align(search_lbl, LV_ALIGN_LEFT_MID, 30, 0);
    lv_obj_set_style_text_color(search_lbl, lv_color_hex(0x555555), 0);
    lv_obj_set_style_text_font(search_lbl, &lv_font_montserrat_14, 0);

    // ── Patient List ──────────────────────────────────────────────
    lv_obj_t* list = lv_list_create(content);
    lv_obj_set_size(list, LV_PCT(90), LV_PCT(55));
    lv_obj_align(list, LV_ALIGN_TOP_MID, 0, 78);
    lv_obj_set_style_bg_color(list, lv_color_hex(0x0a0a1a), 0);
    lv_obj_set_style_border_color(list, lv_color_hex(0x181828), 0);
    lv_obj_set_style_border_width(list, 1, 0);
    lv_obj_set_style_radius(list, 10, 0);

    // Demo patients (in production: loaded from PatientManager)
    struct DemoPatient { const char* id; const char* initials; uint16_t tests; };
    static const DemoPatient demos[] = {
        {"PAT-001", "A.M.", 12},
        {"PAT-002", "B.K.", 8},
        {"PAT-003", "C.S.", 3},
        {"PAT-004", "D.L.", 15},
        {"PAT-005", "E.R.", 1},
    };

    for (auto& p : demos) {
        char txt[64];
        snprintf(txt, sizeof(txt), "%s  (%s)  — %u Tests", p.id, p.initials, p.tests);
        lv_obj_t* item = lv_list_add_btn(list, LV_SYMBOL_USER, txt);
        lv_obj_set_style_bg_color(item, lv_color_hex(0x111122), 0);
        lv_obj_set_style_text_color(item, lv_color_hex(0xCCCCCC), 0);
    }

    // ── Empty State ───────────────────────────────────────────────
    // (shown when list is empty in production)

    // ── Add Patient Button ────────────────────────────────────────
    lv_obj_t* add_btn = lv_btn_create(content);
    lv_obj_set_size(add_btn, LV_PCT(90), 44);
    lv_obj_align(add_btn, LV_ALIGN_BOTTOM_MID, 0, -50);
    lv_obj_set_style_bg_color(add_btn, lv_color_hex(0x0a2a2a), 0);
    lv_obj_set_style_radius(add_btn, 10, 0);
    lv_obj_t* al = lv_label_create(add_btn);
    lv_label_set_text(al, LV_SYMBOL_PLUS " Neuer Patient");
    lv_obj_center(al);
    lv_obj_set_style_text_color(al, lv_color_hex(0x1ABC9C), 0);
    lv_obj_set_style_text_font(al, &lv_font_montserrat_18, 0);

    // ── Back ──────────────────────────────────────────────────────
    lv_obj_t* back = lv_btn_create(content);
    lv_obj_set_size(back, 100, 36);
    lv_obj_align(back, LV_ALIGN_BOTTOM_LEFT, 12, -8);
    lv_obj_set_style_bg_color(back, lv_color_hex(0x1a1a1a), 0);
    lv_obj_t* bb = lv_label_create(back);
    lv_label_set_text(bb, LV_SYMBOL_LEFT " Zurück");
    lv_obj_center(bb);
    lv_obj_set_style_text_color(bb, lv_color_hex(0x888888), 0);
    lv_obj_add_event_cb(back, [](lv_event_t* e) {
        static_cast<PhoenixUI*>(lv_event_get_user_data(e))
            ->navigateTo(ScreenID::HOME);
    }, LV_EVENT_CLICKED, ui);

    ESP_LOGI(TAG, "Patient screen ready");
}

} // namespace phoenix
