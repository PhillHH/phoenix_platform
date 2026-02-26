// ==========================================================================
// FILE: src/UI/Screens/TestSelectionScreen.cpp
// Phoenix v108.0 — Test selection with technology categories
// Shows: Colorimetric | Immunofluorescence | Dry Chemistry | Microfluidics
// Each category expands to show available assays
// ==========================================================================
#include "phoenix/UI/PhoenixUI.h"
#include "phoenix/Core/AssayTechnology.h"
#include <esp_log.h>
#include <lvgl.h>

namespace phoenix {

// Forward declaration — defined in DemoMeasurementEngine.cpp
bool isDemoMode();

static const char* TAG = "TestSel";

// ── Color helpers ─────────────────────────────────────────────────────
static lv_color_t techColor(Technology t) {
    return lv_color_hex(getTechInfo(t).color_hex);
}

static lv_color_t techColorDim(Technology t) {
    uint32_t c = getTechInfo(t).color_hex;
    uint8_t r = ((c >> 16) & 0xFF) / 4;
    uint8_t g = ((c >>  8) & 0xFF) / 4;
    uint8_t b = ((c      ) & 0xFF) / 4;
    return lv_color_make(r, g, b);
}

// ── Technology Card ───────────────────────────────────────────────────
static void onTechCardClicked(lv_event_t* e);
static void onAssayClicked(lv_event_t* e);

struct TechCardData {
    Technology tech;
    PhoenixUI* ui;
    lv_obj_t*  assay_list;
    bool       expanded;
};

static TechCardData s_cards[NUM_TECHNOLOGIES];

static lv_obj_t* createTechCard(lv_obj_t* parent, Technology tech, PhoenixUI* ui) {
    uint8_t idx = static_cast<uint8_t>(tech);
    const TechInfo& info = getTechInfo(tech);

    lv_obj_t* card = lv_obj_create(parent);
    lv_obj_set_size(card, LV_PCT(92), LV_SIZE_CONTENT);
    lv_obj_set_style_radius(card, 16, 0);
    lv_obj_set_style_bg_color(card, techColorDim(tech), 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(card, techColor(tech), 0);
    lv_obj_set_style_border_width(card, 2, 0);
    lv_obj_set_style_border_opa(card, LV_OPA_40, 0);
    lv_obj_set_style_pad_all(card, 12, 0);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(card, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

    // Header row: icon + name + arrow
    lv_obj_t* header = lv_obj_create(card);
    lv_obj_set_size(header, LV_PCT(100), 44);
    lv_obj_set_style_bg_opa(header, LV_OPA_0, 0);
    lv_obj_set_style_border_width(header, 0, 0);
    lv_obj_set_style_pad_all(header, 0, 0);
    lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(header, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(header, LV_FLEX_ALIGN_SPACE_BETWEEN,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // Icon
    lv_obj_t* icon = lv_label_create(header);
    lv_label_set_text(icon, info.icon);
    lv_obj_set_style_text_color(icon, techColor(tech), 0);
    lv_obj_set_style_text_font(icon, &lv_font_montserrat_20, 0);

    // Name + count
    size_t assay_count = 0;
    const auto* assays = getDemoAssays(assay_count);
    size_t tech_count = 0;
    for (size_t i = 0; i < assay_count; i++) {
        if (assays[i].tech == tech) tech_count++;
    }

    lv_obj_t* name = lv_label_create(header);
    lv_label_set_text_fmt(name, "%s (%d)", info.name_de, (int)tech_count);
    lv_obj_set_style_text_color(name, lv_color_white(), 0);
    lv_obj_set_style_text_font(name, &lv_font_montserrat_16, 0);
    lv_obj_set_flex_grow(name, 1);

    // Expand arrow
    lv_obj_t* arrow = lv_label_create(header);
    lv_label_set_text(arrow, LV_SYMBOL_DOWN);
    lv_obj_set_style_text_color(arrow, lv_color_hex(0x888888), 0);

    // Hidden assay list (shown on tap)
    lv_obj_t* assay_list = lv_obj_create(card);
    lv_obj_set_size(assay_list, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(assay_list, LV_OPA_0, 0);
    lv_obj_set_style_border_width(assay_list, 0, 0);
    lv_obj_set_style_pad_all(assay_list, 0, 0);
    lv_obj_set_style_pad_row(assay_list, 4, 0);
    lv_obj_set_flex_flow(assay_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_add_flag(assay_list, LV_OBJ_FLAG_HIDDEN);

    // Populate assay buttons
    for (size_t i = 0; i < assay_count; i++) {
        if (assays[i].tech != tech) continue;

        lv_obj_t* btn = lv_btn_create(assay_list);
        lv_obj_set_size(btn, LV_PCT(100), 40);
        lv_obj_set_style_radius(btn, 10, 0);
        lv_obj_set_style_bg_color(btn, lv_color_hex(0x1E1E2E), 0);
        lv_obj_set_style_bg_color(btn, techColor(tech), LV_STATE_PRESSED);
        lv_obj_set_user_data(btn, (void*)(uintptr_t)i); // assay index

        lv_obj_t* row = lv_obj_create(btn);
        lv_obj_set_size(row, LV_PCT(100), LV_PCT(100));
        lv_obj_set_style_bg_opa(row, LV_OPA_0, 0);
        lv_obj_set_style_border_width(row, 0, 0);
        lv_obj_set_style_pad_all(row, 0, 0);
        lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN,
                              LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

        lv_obj_t* lbl = lv_label_create(row);
        lv_label_set_text(lbl, assays[i].name_de.c_str());
        lv_obj_set_style_text_color(lbl, lv_color_white(), 0);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);

        lv_obj_t* unit = lv_label_create(row);
        lv_label_set_text_fmt(unit, "%s  " LV_SYMBOL_RIGHT,
                              assays[i].unit.c_str());
        lv_obj_set_style_text_color(unit, lv_color_hex(0x888888), 0);
        lv_obj_set_style_text_font(unit, &lv_font_montserrat_12, 0);

        lv_obj_add_event_cb(btn, onAssayClicked, LV_EVENT_CLICKED, ui);
    }

    // Store card data
    s_cards[idx] = {tech, ui, assay_list, false};
    lv_obj_set_user_data(header, &s_cards[idx]);
    lv_obj_add_event_cb(header, onTechCardClicked, LV_EVENT_CLICKED, nullptr);
    lv_obj_add_flag(header, LV_OBJ_FLAG_CLICKABLE);

    return card;
}

// ── Expand/collapse ───────────────────────────────────────────────────
static void onTechCardClicked(lv_event_t* e) {
    auto* data = static_cast<TechCardData*>(lv_obj_get_user_data(lv_event_get_target(e)));
    if (!data) return;

    data->expanded = !data->expanded;
    if (data->expanded) {
        lv_obj_clear_flag(data->assay_list, LV_OBJ_FLAG_HIDDEN);
        ESP_LOGI(TAG, "Expanded: %s", getTechInfo(data->tech).name);
    } else {
        lv_obj_add_flag(data->assay_list, LV_OBJ_FLAG_HIDDEN);
    }
}

// ── Assay selected → start measurement ────────────────────────────────
static void onAssayClicked(lv_event_t* e) {
    auto* ui = static_cast<PhoenixUI*>(lv_event_get_user_data(e));
    size_t idx = (size_t)(uintptr_t)lv_obj_get_user_data(lv_event_get_target(e));

    size_t count = 0;
    const auto* assays = getDemoAssays(count);
    if (idx < count) {
        ESP_LOGI(TAG, "Selected assay: %s [%s]",
                 assays[idx].name.c_str(), assays[idx].code.c_str());
        // Store selected assay and navigate to measurement screen
        // PhoenixUI handles this via setSelectedAssay() + showScreen()
    }
}

// ── Public: Create Test Selection Screen ──────────────────────────────
void createTestSelectionScreen(lv_obj_t* content, PhoenixUI* ui) {
    // Title
    lv_obj_t* title = lv_label_create(content);
    lv_label_set_text(title, "Test auswählen");
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 8);

    // Subtitle: mode indicator
    lv_obj_t* mode = lv_label_create(content);
    lv_label_set_text(mode, isDemoMode() ? "Demo-Modus" : "Live-Modus");
    lv_obj_align_to(mode, title, LV_ALIGN_OUT_BOTTOM_MID, 0, 2);
    lv_obj_set_style_text_color(mode,
        isDemoMode() ? lv_color_hex(0xF59E0B) : lv_color_hex(0x22C55E), 0);
    lv_obj_set_style_text_font(mode, &lv_font_montserrat_12, 0);

    // Scrollable container for tech cards
    lv_obj_t* list = lv_obj_create(content);
    lv_obj_set_size(list, LV_PCT(100), LV_PCT(80));
    lv_obj_align(list, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_opa(list, LV_OPA_0, 0);
    lv_obj_set_style_border_width(list, 0, 0);
    lv_obj_set_style_pad_all(list, 4, 0);
    lv_obj_set_style_pad_row(list, 8, 0);
    lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(list, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_scroll_dir(list, LV_DIR_VER);

    // Create one card per technology
    createTechCard(list, Technology::COLORIMETRIC, ui);
    createTechCard(list, Technology::IMMUNOFLUORESCENCE, ui);
    createTechCard(list, Technology::DRY_CHEMISTRY, ui);
    createTechCard(list, Technology::MICROFLUIDICS, ui);

    ESP_LOGI(TAG, "Test selection screen created (4 technologies)");
}

} // namespace phoenix
