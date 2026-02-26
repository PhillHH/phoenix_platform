// ==========================================================================
// FILE: src/UI/Screens/MeasurementScreen.cpp
// Phoenix v108.0 — Measurement in-progress screen
// Animated pipeline stages, progress bar, cancel, auto-result transition
// ==========================================================================
#include "phoenix/UI/PhoenixUI.h"
#include <esp_log.h>
#include <lvgl.h>

namespace phoenix {

static const char* TAG = "MeasScr";

// Pipeline stage descriptors
struct StageInfo {
    const char* icon;
    const char* label;
    uint8_t     pct_start;
    uint8_t     pct_end;
};

static constexpr StageInfo STAGES[] = {
    {LV_SYMBOL_OK,       "Validierung",   0,  10},
    {LV_SYMBOL_CHARGE,   "LED Warmup",   10,  20},
    {LV_SYMBOL_IMAGE,    "Aufnahme",     20,  35},
    {LV_SYMBOL_EYE_OPEN, "Profil",       35,  55},
    {LV_SYMBOL_MINUS,    "Baseline",     55,  65},
    {LV_SYMBOL_GPS,      "Peaks",        65,  75},
    {LV_SYMBOL_SHUFFLE,  "Kalibrierung", 75,  90},
    {LV_SYMBOL_OK,       "QC Check",     90, 100},
};
static constexpr size_t NUM_STAGES = sizeof(STAGES) / sizeof(STAGES[0]);

// Widget references stored on content user_data
struct MeasScreenWidgets {
    lv_obj_t* bar;
    lv_obj_t* pct_label;
    lv_obj_t* msg_label;
    lv_obj_t* stage_dots[NUM_STAGES];
    lv_obj_t* stage_labels[NUM_STAGES];
};

static MeasScreenWidgets* getWidgets(lv_obj_t* content) {
    return static_cast<MeasScreenWidgets*>(lv_obj_get_user_data(content));
}

// ─── Create Measurement Screen ───────────────────────────────────────
void createMeasurementScreen(lv_obj_t* content, PhoenixUI* ui) {
    // Allocate widget tracker (lives until screen is cleaned)
    static MeasScreenWidgets widgets = {};
    lv_obj_set_user_data(content, &widgets);

    // Center container
    lv_obj_t* cont = lv_obj_create(content);
    lv_obj_set_size(cont, LV_PCT(90), LV_PCT(90));
    lv_obj_center(cont);
    lv_obj_set_style_bg_opa(cont, LV_OPA_0, 0);
    lv_obj_set_style_border_width(cont, 0, 0);
    lv_obj_clear_flag(cont, LV_OBJ_FLAG_SCROLLABLE);

    // ── Title ─────────────────────────────────────────────────────
    lv_obj_t* title = lv_label_create(cont);
    lv_label_set_text(title, LV_SYMBOL_REFRESH " Messung läuft");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0x2196F3), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);

    // ── Percentage ────────────────────────────────────────────────
    widgets.pct_label = lv_label_create(cont);
    lv_label_set_text(widgets.pct_label, "0%");
    lv_obj_align(widgets.pct_label, LV_ALIGN_TOP_MID, 0, 30);
    lv_obj_set_style_text_font(widgets.pct_label, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(widgets.pct_label, lv_color_white(), 0);

    // ── Message ───────────────────────────────────────────────────
    widgets.msg_label = lv_label_create(cont);
    lv_label_set_text(widgets.msg_label, "Validiere System…");
    lv_obj_align(widgets.msg_label, LV_ALIGN_TOP_MID, 0, 82);
    lv_obj_set_style_text_color(widgets.msg_label, lv_color_hex(0x888888), 0);
    lv_obj_set_style_text_font(widgets.msg_label, &lv_font_montserrat_14, 0);

    // ── Progress Bar ──────────────────────────────────────────────
    widgets.bar = lv_bar_create(cont);
    lv_obj_set_size(widgets.bar, LV_PCT(80), 12);
    lv_obj_align(widgets.bar, LV_ALIGN_CENTER, 0, -10);
    lv_obj_set_style_bg_color(widgets.bar, lv_color_hex(0x1a1a2e), LV_PART_MAIN);
    lv_obj_set_style_bg_color(widgets.bar, lv_color_hex(0x2196F3), LV_PART_INDICATOR);
    lv_obj_set_style_radius(widgets.bar, 6, LV_PART_MAIN);
    lv_obj_set_style_radius(widgets.bar, 6, LV_PART_INDICATOR);
    lv_bar_set_value(widgets.bar, 0, LV_ANIM_ON);

    // ── Stage Indicator Row ───────────────────────────────────────
    lv_obj_t* stages_row = lv_obj_create(cont);
    lv_obj_set_size(stages_row, LV_PCT(95), 50);
    lv_obj_align(stages_row, LV_ALIGN_CENTER, 0, 30);
    lv_obj_set_style_bg_opa(stages_row, LV_OPA_0, 0);
    lv_obj_set_style_border_width(stages_row, 0, 0);
    lv_obj_clear_flag(stages_row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(stages_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(stages_row, LV_FLEX_ALIGN_SPACE_EVENLY,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    for (size_t i = 0; i < NUM_STAGES; ++i) {
        lv_obj_t* stage = lv_obj_create(stages_row);
        lv_obj_set_size(stage, 30, 40);
        lv_obj_set_style_bg_opa(stage, LV_OPA_0, 0);
        lv_obj_set_style_border_width(stage, 0, 0);
        lv_obj_clear_flag(stage, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t* dot = lv_obj_create(stage);
        lv_obj_set_size(dot, 14, 14);
        lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(dot, lv_color_hex(0x333333), 0);
        lv_obj_set_style_border_width(dot, 0, 0);
        lv_obj_align(dot, LV_ALIGN_TOP_MID, 0, 0);
        widgets.stage_dots[i] = dot;

        lv_obj_t* lbl = lv_label_create(stage);
        lv_label_set_text(lbl, STAGES[i].icon);
        lv_obj_align(lbl, LV_ALIGN_BOTTOM_MID, 0, 0);
        lv_obj_set_style_text_color(lbl, lv_color_hex(0x555555), 0);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
        widgets.stage_labels[i] = lbl;
    }

    // ── Cancel Button ─────────────────────────────────────────────
    lv_obj_t* btn = lv_btn_create(cont);
    lv_obj_set_size(btn, 130, 44);
    lv_obj_align(btn, LV_ALIGN_BOTTOM_MID, 0, -5);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x2a1a1a), 0);
    lv_obj_set_style_radius(btn, 10, 0);

    lv_obj_t* bl = lv_label_create(btn);
    lv_label_set_text(bl, LV_SYMBOL_CLOSE " Abbrechen");
    lv_obj_center(bl);
    lv_obj_set_style_text_color(bl, lv_color_hex(0xEF4444), 0);

    lv_obj_add_event_cb(btn, [](lv_event_t* e) {
        auto* u = static_cast<PhoenixUI*>(lv_event_get_user_data(e));
        // Cancel via proxy and navigate home
        u->navigateTo(ScreenID::HOME);
    }, LV_EVENT_CLICKED, ui);
}

// ─── Update Progress (called from PhoenixUI::onMeasurementProgress) ──
void updateMeasurementProgress(lv_obj_t* content, uint8_t pct, const char* msg) {
    auto* w = getWidgets(content);
    if (!w || !w->bar) return;

    lv_bar_set_value(w->bar, pct, LV_ANIM_ON);
    lv_label_set_text_fmt(w->pct_label, "%u%%", pct);

    if (msg && w->msg_label) {
        lv_label_set_text(w->msg_label, msg);
    }

    // Update stage dots
    for (size_t i = 0; i < NUM_STAGES; ++i) {
        lv_color_t color;
        if (pct >= STAGES[i].pct_end) {
            color = lv_color_hex(0x22C55E); // Complete
        } else if (pct >= STAGES[i].pct_start) {
            color = lv_color_hex(0x2196F3); // Active
        } else {
            color = lv_color_hex(0x333333); // Pending
        }
        lv_obj_set_style_bg_color(w->stage_dots[i], color, 0);
        lv_obj_set_style_text_color(w->stage_labels[i],
            pct >= STAGES[i].pct_start ? lv_color_white() : lv_color_hex(0x555555), 0);
    }
}

} // namespace phoenix
