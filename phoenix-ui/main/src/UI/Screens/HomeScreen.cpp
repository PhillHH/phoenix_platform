// ==========================================================================
// FILE: src/UI/Screens/HomeScreen.cpp
// Phoenix v108.0 — Home screen (circular menu entry point)
// Displays: quick-start test, last result, device status, battery
// ==========================================================================
#include "phoenix/UI/PhoenixUI.h"
#include "phoenix/UI/CircularMenu.h"
#include <esp_log.h>
#include <lvgl.h>

// The HomeScreen logic is implemented in PhoenixUI::showHome().
// This file provides supplementary widgets used by the home screen.

namespace phoenix {

static const char* TAG = "HomeScr";

// ─── Status Indicator Widget ──────────────────────────────────────────
// Shows WiFi, battery, MCU connection as colored dots

static lv_obj_t* createStatusDot(lv_obj_t* parent, lv_color_t color,
                                   const char* tooltip) {
    lv_obj_t* dot = lv_obj_create(parent);
    lv_obj_set_size(dot, 10, 10);
    lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(dot, color, 0);
    lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(dot, 0, 0);
    lv_obj_clear_flag(dot, LV_OBJ_FLAG_SCROLLABLE);
    (void)tooltip; // A11y: label for screen readers (future)
    return dot;
}

// ─── Quick-Result Banner ──────────────────────────────────────────────
// Shown at bottom of home when there's a recent result

static void createLastResultBanner(lv_obj_t* parent,
                                     float concentration,
                                     const char* analyte,
                                     const char* unit,
                                     const char* interpretation)
{
    lv_obj_t* banner = lv_obj_create(parent);
    lv_obj_set_size(banner, LV_PCT(90), 50);
    lv_obj_align(banner, LV_ALIGN_BOTTOM_MID, 0, -8);
    lv_obj_set_style_radius(banner, 12, 0);
    lv_obj_set_style_bg_color(banner, lv_color_hex(0x1a2a1a), 0);
    lv_obj_set_style_border_color(banner, lv_color_hex(0x22C55E), 0);
    lv_obj_set_style_border_width(banner, 1, 0);
    lv_obj_set_style_border_opa(banner, LV_OPA_30, 0);
    lv_obj_clear_flag(banner, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(banner, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(banner, LV_FLEX_ALIGN_SPACE_BETWEEN,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // Left: analyte + interpretation
    lv_obj_t* left = lv_obj_create(banner);
    lv_obj_set_size(left, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(left, LV_OPA_0, 0);
    lv_obj_set_style_border_width(left, 0, 0);
    lv_obj_set_style_pad_all(left, 0, 0);

    lv_obj_t* lbl1 = lv_label_create(left);
    lv_label_set_text_fmt(lbl1, "%s: %s", analyte, interpretation);
    lv_obj_set_style_text_color(lbl1, lv_color_hex(0x22C55E), 0);
    lv_obj_set_style_text_font(lbl1, &lv_font_montserrat_14, 0);

    // Right: value + unit
    lv_obj_t* val = lv_label_create(banner);
    lv_label_set_text_fmt(val, "%.1f %s",
                          static_cast<double>(concentration), unit);
    lv_obj_set_style_text_color(val, lv_color_white(), 0);
    lv_obj_set_style_text_font(val, &lv_font_montserrat_20, 0);
}

// ─── Device Status Row ────────────────────────────────────────────────

static void createStatusRow(lv_obj_t* parent, bool wifi, bool mcu,
                              float battery_pct)
{
    lv_obj_t* row = lv_obj_create(parent);
    lv_obj_set_size(row, LV_PCT(60), 20);
    lv_obj_align(row, LV_ALIGN_TOP_MID, 0, 4);
    lv_obj_set_style_bg_opa(row, LV_OPA_0, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(row, 6, 0);

    createStatusDot(row, wifi ? lv_color_hex(0x22C55E) : lv_color_hex(0x555555), "WiFi");
    lv_obj_t* wl = lv_label_create(row);
    lv_label_set_text(wl, wifi ? "WiFi" : "Offline");
    lv_obj_set_style_text_font(wl, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(wl, lv_color_hex(0x888888), 0);

    createStatusDot(row, mcu ? lv_color_hex(0x22C55E) : lv_color_hex(0xEF4444), "MCU");
    lv_obj_t* ml = lv_label_create(row);
    lv_label_set_text(ml, mcu ? "MCU" : "MCU!");
    lv_obj_set_style_text_font(ml, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(ml, lv_color_hex(0x888888), 0);

    lv_obj_t* bat = lv_label_create(row);
    lv_label_set_text_fmt(bat, LV_SYMBOL_BATTERY_3 " %.0f%%",
                          static_cast<double>(battery_pct));
    lv_obj_set_style_text_font(bat, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(bat,
        battery_pct > 20 ? lv_color_hex(0x888888) : lv_color_hex(0xEF4444), 0);
}

} // namespace phoenix
