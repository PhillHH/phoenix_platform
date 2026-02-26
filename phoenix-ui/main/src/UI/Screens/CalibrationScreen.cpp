// ==========================================================================
// FILE: src/UI/Screens/CalibrationScreen.cpp (REWRITE)
// Phoenix v108.0 — Color Chart Calibration UI
// Shows the 11-strip calibration process with visual feedback
// References: DXR.007.01-01.DR (Plate Left), DXR.007.01-02.DR (Plate Right)
// ==========================================================================
#include "phoenix/UI/PhoenixUI.h"
#include "phoenix/Core/AssayTechnology.h"
#include <esp_log.h>
#include <lvgl.h>

namespace phoenix {

// Forward declaration — defined in DemoMeasurementEngine.cpp
bool isDemoMode();

static const char* TAG = "CalScr";

// ── Strip color definitions matching DXR.007.01 ──────────────────────
struct StripVisual {
    uint32_t color;
    const char* label;
    const char* label_de;
};

static constexpr StripVisual STRIP_VIS[11] = {
    {0x000000, "Black",      "Schwarz"},
    {0xFFFFFF, "White",      "Weiß"},
    {0x755E05, "Gold Ref",   "Gold Ref"},
    {0xFF00FF, "Magenta",    "Magenta"},
    {0xFFFF00, "Yellow",     "Gelb"},
    {0x000000, "Black 2",    "Schwarz 2"},
    {0xFFFFFF, "White 2",    "Weiß 2"},
    {0x0000FF, "Blue",       "Blau"},
    {0x00FF00, "Green",      "Grün"},
    {0xFF0000, "Red",        "Rot"},
    {0xFFFFFF, "White 3",    "Weiß 3"},
};

// ── Calibration State ─────────────────────────────────────────────────
enum class CalState {
    IDLE,         // Waiting to start
    INSERT_PLATE, // Prompt to insert plate
    SCANNING,     // Reading strips
    COMPUTING,    // Processing
    RESULT_OK,    // Calibration passed
    RESULT_FAIL,  // Calibration failed
};

struct CalScreenWidgets {
    lv_obj_t* strips[11];       // Strip visual indicators
    lv_obj_t* status_label;
    lv_obj_t* progress_bar;
    lv_obj_t* r2_label;
    lv_obj_t* drift_label;
    lv_obj_t* gold_ref_label;
    lv_obj_t* action_btn;
    CalState  state;
    uint8_t   current_strip;
};

static CalScreenWidgets s_cal = {};

// ── Create the strip visualization ────────────────────────────────────
static lv_obj_t* createStripRow(lv_obj_t* parent) {
    // Visual representation of the color chart plate
    lv_obj_t* plate = lv_obj_create(parent);
    lv_obj_set_size(plate, 44, 280);
    lv_obj_set_style_bg_color(plate, lv_color_hex(0x2A2A2A), 0);
    lv_obj_set_style_radius(plate, 6, 0);
    lv_obj_set_style_border_color(plate, lv_color_hex(0x555555), 0);
    lv_obj_set_style_border_width(plate, 1, 0);
    lv_obj_set_style_pad_all(plate, 4, 0);
    lv_obj_set_style_pad_row(plate, 2, 0);
    lv_obj_set_flex_flow(plate, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(plate, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(plate, LV_OBJ_FLAG_SCROLLABLE);

    for (int i = 0; i < 11; i++) {
        lv_obj_t* strip = lv_obj_create(plate);
        lv_obj_set_size(strip, 36, 20);
        
        // Set the actual color from the reference chart
        uint32_t c = STRIP_VIS[i].color;
        lv_obj_set_style_bg_color(strip, lv_color_hex(c), 0);
        lv_obj_set_style_bg_opa(strip, LV_OPA_COVER, 0);
        lv_obj_set_style_radius(strip, 2, 0);
        lv_obj_set_style_border_width(strip, 1, 0);
        lv_obj_set_style_border_color(strip, lv_color_hex(0x444444), 0);
        lv_obj_clear_flag(strip, LV_OBJ_FLAG_SCROLLABLE);

        // "Pending" overlay (gray border, turns green when scanned)
        lv_obj_set_style_border_color(strip, lv_color_hex(0x666666), 0);
        lv_obj_set_style_border_width(strip, 2, 0);

        s_cal.strips[i] = strip;
    }

    return plate;
}

// ── Update strip state ────────────────────────────────────────────────
static void markStripScanned(uint8_t idx, bool ok) {
    if (idx >= 11) return;
    lv_obj_t* strip = s_cal.strips[idx];
    if (ok) {
        lv_obj_set_style_border_color(strip, lv_color_hex(0x22C55E), 0); // green
        lv_obj_set_style_border_width(strip, 3, 0);
    } else {
        lv_obj_set_style_border_color(strip, lv_color_hex(0xEF4444), 0); // red
        lv_obj_set_style_border_width(strip, 3, 0);
    }
}

static void markStripActive(uint8_t idx) {
    if (idx >= 11) return;
    lv_obj_set_style_border_color(s_cal.strips[idx], lv_color_hex(0xF59E0B), 0);
    lv_obj_set_style_border_width(s_cal.strips[idx], 3, 0);
}

// ── Button callbacks ──────────────────────────────────────────────────
static void onCalActionClicked(lv_event_t* e) {
    auto* ui = static_cast<PhoenixUI*>(lv_event_get_user_data(e));
    (void)ui;

    switch (s_cal.state) {
    case CalState::IDLE:
        // Start calibration → prompt to insert plate
        s_cal.state = CalState::INSERT_PLATE;
        lv_label_set_text(s_cal.status_label,
                          "Kalibrier-Platte einlegen\n(DXR.007.01)");
        lv_label_set_text(lv_obj_get_child(s_cal.action_btn, 0),
                          "Scan starten");
        break;

    case CalState::INSERT_PLATE:
        // Start scanning
        s_cal.state = CalState::SCANNING;
        s_cal.current_strip = 0;
        lv_label_set_text(s_cal.status_label, "Scanne Strip 0: Schwarz...");
        lv_label_set_text(lv_obj_get_child(s_cal.action_btn, 0),
                          "Scanning...");
        lv_obj_add_state(s_cal.action_btn, LV_STATE_DISABLED);
        markStripActive(0);
        
        // In demo mode: simulate scanning with timer
        // In live mode: trigger camera capture
        if (isDemoMode()) {
            // Timer will advance strips (handled by demo engine)
            ESP_LOGI(TAG, "Demo: simulating color chart scan");
        }
        break;

    case CalState::RESULT_OK:
    case CalState::RESULT_FAIL:
        // Reset
        s_cal.state = CalState::IDLE;
        lv_label_set_text(s_cal.status_label, "Bereit zur Kalibrierung");
        lv_label_set_text(lv_obj_get_child(s_cal.action_btn, 0),
                          LV_SYMBOL_REFRESH " Kalibrieren");
        lv_obj_clear_state(s_cal.action_btn, LV_STATE_DISABLED);
        // Reset strip indicators
        for (int i = 0; i < 11; i++) {
            lv_obj_set_style_border_color(s_cal.strips[i],
                                           lv_color_hex(0x666666), 0);
            lv_obj_set_style_border_width(s_cal.strips[i], 2, 0);
        }
        break;

    default:
        break;
    }
}

// ── Public: Create Calibration Screen ─────────────────────────────────
void createCalibrationScreen(lv_obj_t* content, PhoenixUI* ui) {
    s_cal.state = CalState::IDLE;

    // Title
    lv_obj_t* title = lv_label_create(content);
    lv_label_set_text(title, "Farbkarten-Kalibrierung");
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_18, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 8);

    // Main layout: plate visual (left) + info (right)
    lv_obj_t* main_row = lv_obj_create(content);
    lv_obj_set_size(main_row, LV_PCT(95), LV_PCT(65));
    lv_obj_align_to(main_row, title, LV_ALIGN_OUT_BOTTOM_MID, 0, 8);
    lv_obj_set_style_bg_opa(main_row, LV_OPA_0, 0);
    lv_obj_set_style_border_width(main_row, 0, 0);
    lv_obj_set_style_pad_all(main_row, 4, 0);
    lv_obj_clear_flag(main_row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(main_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(main_row, LV_FLEX_ALIGN_SPACE_EVENLY,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // Left: Color chart plate visualization
    createStripRow(main_row);

    // Right: Info panel
    lv_obj_t* info = lv_obj_create(main_row);
    lv_obj_set_size(info, LV_PCT(60), LV_PCT(100));
    lv_obj_set_style_bg_opa(info, LV_OPA_0, 0);
    lv_obj_set_style_border_width(info, 0, 0);
    lv_obj_set_style_pad_all(info, 4, 0);
    lv_obj_clear_flag(info, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(info, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(info, 6, 0);

    // Status text
    s_cal.status_label = lv_label_create(info);
    lv_label_set_text(s_cal.status_label, "Bereit zur Kalibrierung");
    lv_obj_set_style_text_color(s_cal.status_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(s_cal.status_label, &lv_font_montserrat_14, 0);
    lv_label_set_long_mode(s_cal.status_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(s_cal.status_label, LV_PCT(100));

    // Progress bar
    s_cal.progress_bar = lv_bar_create(info);
    lv_obj_set_size(s_cal.progress_bar, LV_PCT(100), 12);
    lv_bar_set_range(s_cal.progress_bar, 0, 11);
    lv_bar_set_value(s_cal.progress_bar, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(s_cal.progress_bar, lv_color_hex(0x1E1E2E), 0);
    lv_obj_set_style_bg_color(s_cal.progress_bar, lv_color_hex(0x22C55E),
                              LV_PART_INDICATOR);
    lv_obj_set_style_radius(s_cal.progress_bar, 6, 0);
    lv_obj_set_style_radius(s_cal.progress_bar, 6, LV_PART_INDICATOR);

    // R² display
    s_cal.r2_label = lv_label_create(info);
    lv_label_set_text(s_cal.r2_label, "Linearität R²: ---");
    lv_obj_set_style_text_color(s_cal.r2_label, lv_color_hex(0x888888), 0);
    lv_obj_set_style_text_font(s_cal.r2_label, &lv_font_montserrat_12, 0);

    // Drift display
    s_cal.drift_label = lv_label_create(info);
    lv_label_set_text(s_cal.drift_label, "Drift: ---");
    lv_obj_set_style_text_color(s_cal.drift_label, lv_color_hex(0x888888), 0);
    lv_obj_set_style_text_font(s_cal.drift_label, &lv_font_montserrat_12, 0);

    // Gold NP reference
    s_cal.gold_ref_label = lv_label_create(info);
    lv_label_set_text(s_cal.gold_ref_label, "Gold-NP Ref: ---");
    lv_obj_set_style_text_color(s_cal.gold_ref_label, lv_color_hex(0x755E05), 0);
    lv_obj_set_style_text_font(s_cal.gold_ref_label, &lv_font_montserrat_12, 0);

    // Demo badge
    if (isDemoMode()) {
        lv_obj_t* demo = lv_label_create(info);
        lv_label_set_text(demo, "Demo-Modus");
        lv_obj_set_style_text_color(demo, lv_color_hex(0xF59E0B), 0);
        lv_obj_set_style_text_font(demo, &lv_font_montserrat_10, 0);
    }

    // Action button
    s_cal.action_btn = lv_btn_create(content);
    lv_obj_set_size(s_cal.action_btn, 200, 44);
    lv_obj_align(s_cal.action_btn, LV_ALIGN_BOTTOM_MID, 0, -8);
    lv_obj_set_style_bg_color(s_cal.action_btn, lv_color_hex(0x22C55E), 0);
    lv_obj_set_style_bg_color(s_cal.action_btn, lv_color_hex(0x333333),
                              LV_STATE_DISABLED);
    lv_obj_set_style_radius(s_cal.action_btn, 14, 0);

    lv_obj_t* btn_label = lv_label_create(s_cal.action_btn);
    lv_label_set_text(btn_label, LV_SYMBOL_REFRESH " Kalibrieren");
    lv_obj_set_style_text_font(btn_label, &lv_font_montserrat_16, 0);
    lv_obj_center(btn_label);

    lv_obj_add_event_cb(s_cal.action_btn, onCalActionClicked,
                         LV_EVENT_CLICKED, ui);

    ESP_LOGI(TAG, "Color Chart Calibration screen created (11 strips, DXR.007.01)");
}

// ── Demo: Simulate scan progress (called from DemoEngine timer) ───────
void updateCalibrationProgress(uint8_t strip, bool ok, float r2,
                                 float drift, float gold_ref) {
    if (strip < 11) {
        markStripScanned(strip, ok);
        lv_bar_set_value(s_cal.progress_bar, strip + 1, LV_ANIM_ON);

        if (strip < 10) {
            markStripActive(strip + 1);
            lv_label_set_text_fmt(s_cal.status_label,
                                  "Scanne Strip %d: %s...",
                                  strip + 1, STRIP_VIS[strip + 1].label_de);
        }
    }

    if (strip == 10) {
        // Final results
        lv_label_set_text_fmt(s_cal.r2_label, "Linearität R²: %.4f %s",
                              static_cast<double>(r2),
                              r2 > 0.95f ? "✓" : "✗");
        lv_obj_set_style_text_color(s_cal.r2_label,
            r2 > 0.95f ? lv_color_hex(0x22C55E) : lv_color_hex(0xEF4444), 0);

        lv_label_set_text_fmt(s_cal.drift_label, "Drift: %.2f%% %s",
                              static_cast<double>(drift * 100),
                              drift < 0.05f ? "✓" : "⚠");

        lv_label_set_text_fmt(s_cal.gold_ref_label,
                              "Gold-NP Ref: %.4f",
                              static_cast<double>(gold_ref));

        if (r2 > 0.95f) {
            s_cal.state = CalState::RESULT_OK;
            lv_label_set_text(s_cal.status_label,
                              "✓ Kalibrierung erfolgreich!");
            lv_obj_set_style_text_color(s_cal.status_label,
                                         lv_color_hex(0x22C55E), 0);
        } else {
            s_cal.state = CalState::RESULT_FAIL;
            lv_label_set_text(s_cal.status_label,
                              "✗ Kalibrierung fehlgeschlagen\nBitte Platte reinigen und wiederholen");
            lv_obj_set_style_text_color(s_cal.status_label,
                                         lv_color_hex(0xEF4444), 0);
        }

        lv_label_set_text(lv_obj_get_child(s_cal.action_btn, 0),
                          LV_SYMBOL_REFRESH " Nochmal");
        lv_obj_clear_state(s_cal.action_btn, LV_STATE_DISABLED);
    }
}

} // namespace phoenix
