// ==========================================================================
// FILE: src/UI/Screens/ResultScreen.cpp (v108 REWRITE)
// Technology-aware result display with reference ranges and QC flags
// ==========================================================================
#include "phoenix/UI/PhoenixUI.h"
#include "phoenix/Core/AssayTechnology.h"
#include <esp_log.h>
#include <lvgl.h>

namespace phoenix {

// Forward declaration — defined in DemoMeasurementEngine.cpp
bool isDemoMode();

static const char* TAG = "ResultScr";

static lv_color_t interpColor(const char* i) {
    if (!i) return lv_color_white();
    if (!strcmp(i,"NORMAL")||!strcmp(i,"NEGATIVE")) return lv_color_hex(0x22C55E);
    if (!strcmp(i,"HIGH")||!strcmp(i,"POSITIVE"))   return lv_color_hex(0xEF4444);
    if (!strcmp(i,"LOW"))                           return lv_color_hex(0xF59E0B);
    return lv_color_hex(0x888888);
}
static const char* interpDE(const char* i) {
    if (!i) return "---";
    if (!strcmp(i,"NORMAL"))   return "Normal";
    if (!strcmp(i,"HIGH"))     return "Erhöht";
    if (!strcmp(i,"LOW"))      return "Erniedrigt";
    if (!strcmp(i,"POSITIVE")) return "Positiv";
    if (!strcmp(i,"NEGATIVE")) return "Negativ";
    return i;
}

void createResultScreen(lv_obj_t* content, PhoenixUI* ui) {
    // Placeholder result (overwritten by DemoEngine or live data)
    MeasurementResult r = {};
    r.concentration_ng_ml = 3.2f;
    r.analyte_name = "CRP (hs)"; r.unit = "mg/L";
    r.interpretation = "NORMAL";
    r.reference_low = 0.0f; r.reference_high = 5.0f;
    r.confidence = 0.97f; r.qc_flags = QCFlag::NONE;

    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(content, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(content, 6, 0);
    lv_obj_set_style_pad_top(content, 12, 0);

    auto* nm = lv_label_create(content);
    lv_label_set_text(nm, r.analyte_name.c_str());
    lv_obj_set_style_text_color(nm, lv_color_white(), 0);
    lv_obj_set_style_text_font(nm, &lv_font_montserrat_20, 0);

    auto* val = lv_label_create(content);
    lv_label_set_text_fmt(val, "%.2f", (double)r.concentration_ng_ml);
    lv_obj_set_style_text_color(val, interpColor(r.interpretation.c_str()), 0);
    lv_obj_set_style_text_font(val, &lv_font_montserrat_36, 0);

    auto* u = lv_label_create(content);
    lv_label_set_text(u, r.unit.c_str());
    lv_obj_set_style_text_color(u, lv_color_hex(0x888888), 0);

    auto* ip = lv_label_create(content);
    lv_label_set_text(ip, interpDE(r.interpretation.c_str()));
    lv_obj_set_style_text_color(ip, interpColor(r.interpretation.c_str()), 0);
    lv_obj_set_style_text_font(ip, &lv_font_montserrat_20, 0);

    auto* ref = lv_label_create(content);
    lv_label_set_text_fmt(ref, "Ref: %.1f – %.1f %s", (double)r.reference_low, (double)r.reference_high, r.unit.c_str());
    lv_obj_set_style_text_color(ref, lv_color_hex(0x666666), 0);
    lv_obj_set_style_text_font(ref, &lv_font_montserrat_12, 0);

    if (isDemoMode()) {
        auto* d = lv_label_create(content);
        lv_label_set_text(d, "Demo-Ergebnis (simuliert)");
        lv_obj_set_style_text_color(d, lv_color_hex(0xF59E0B), 0);
        lv_obj_set_style_text_font(d, &lv_font_montserrat_10, 0);
    }
    ESP_LOGI(TAG, "Result: %s=%.2f %s", r.analyte_name.c_str(), (double)r.concentration_ng_ml, r.unit.c_str());
}
} // namespace phoenix
