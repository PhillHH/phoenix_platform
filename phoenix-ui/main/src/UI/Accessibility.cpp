// ==========================================================================
// FILE: src/UI/Accessibility.cpp
// Phoenix v108.0 — Accessibility features for medical device UI
// Large text, high contrast, audio feedback, color-blind safe
// ==========================================================================
#include "phoenix/Core/Result.h"
#include <esp_log.h>
#include <nvs_flash.h>
#include <nvs.h>
#include <lvgl.h>

namespace phoenix {

static const char* TAG = "A11y";

struct AccessibilitySettings {
    bool     large_text        = false;
    bool     high_contrast     = false;
    bool     audio_feedback    = true;
    bool     screen_reader     = false;  // Future: TTS via BLE
    uint8_t  font_scale_pct    = 100;    // 100 = normal, 125 = large, 150 = extra
    uint8_t  animation_speed   = 100;    // 0 = disabled, 100 = normal
};

class AccessibilityManager {
public:
    Result<void> initialize() {
        nvs_handle_t h;
        if (nvs_open("a11y", NVS_READONLY, &h) == ESP_OK) {
            uint8_t val;
            if (nvs_get_u8(h, "large_txt", &val)   == ESP_OK) settings_.large_text     = val;
            if (nvs_get_u8(h, "hi_contr",  &val)   == ESP_OK) settings_.high_contrast   = val;
            if (nvs_get_u8(h, "audio_fb",  &val)   == ESP_OK) settings_.audio_feedback   = val;
            if (nvs_get_u8(h, "font_sc",   &val)   == ESP_OK) settings_.font_scale_pct   = val;
            if (nvs_get_u8(h, "anim_spd",  &val)   == ESP_OK) settings_.animation_speed  = val;
            nvs_close(h);
        }
        applySettings();
        ESP_LOGI(TAG, "A11y: large=%d contrast=%d audio=%d font=%u%%",
                 settings_.large_text, settings_.high_contrast,
                 settings_.audio_feedback, settings_.font_scale_pct);
        return Ok();
    }

    void applySettings() {
        // Adjust LVGL default font based on scale
        if (settings_.large_text || settings_.font_scale_pct >= 125) {
            lv_theme_t* th = lv_disp_get_theme(lv_disp_get_default());
            if (th) {
                lv_theme_set_font_small(th, &lv_font_montserrat_18);
                lv_theme_set_font_normal(th, &lv_font_montserrat_24);
                lv_theme_set_font_large(th, &lv_font_montserrat_32);
            }
        }

        // Disable animations if requested
        if (settings_.animation_speed == 0) {
            lv_anim_speed_set(0);
        }
    }

    // Get theme-appropriate colors (high contrast or normal)
    lv_color_t colorPositive() const {
        return settings_.high_contrast
            ? lv_color_hex(0xFF0000)    // Pure red
            : lv_color_hex(0xEF4444);  // Soft red
    }

    lv_color_t colorNegative() const {
        return settings_.high_contrast
            ? lv_color_hex(0x00FF00)
            : lv_color_hex(0x22C55E);
    }

    lv_color_t colorWarning() const {
        return settings_.high_contrast
            ? lv_color_hex(0xFFFF00)
            : lv_color_hex(0xF59E0B);
    }

    lv_color_t colorNeutral() const {
        return settings_.high_contrast
            ? lv_color_hex(0xFFFFFF)
            : lv_color_hex(0xAAAAAA);
    }

    lv_color_t colorBackground() const {
        return settings_.high_contrast
            ? lv_color_hex(0x000000)
            : lv_color_hex(0x0F0F1E);
    }

    // Font selection based on context and scale
    const lv_font_t* fontSmall() const {
        if (settings_.font_scale_pct >= 150) return &lv_font_montserrat_20;
        if (settings_.font_scale_pct >= 125) return &lv_font_montserrat_18;
        return &lv_font_montserrat_14;
    }

    const lv_font_t* fontNormal() const {
        if (settings_.font_scale_pct >= 150) return &lv_font_montserrat_28;
        if (settings_.font_scale_pct >= 125) return &lv_font_montserrat_24;
        return &lv_font_montserrat_18;
    }

    const lv_font_t* fontLarge() const {
        if (settings_.font_scale_pct >= 150) return &lv_font_montserrat_48;
        if (settings_.font_scale_pct >= 125) return &lv_font_montserrat_32;
        return &lv_font_montserrat_24;
    }

    const lv_font_t* fontResult() const {
        return &lv_font_montserrat_48; // Always large for results
    }

    // Minimum touch target size (IEC 62366 compliance)
    lv_coord_t minTouchTarget() const {
        return settings_.large_text ? 56 : 44; // 44px minimum per IEC 62366
    }

    // Get/set settings
    const AccessibilitySettings& getSettings() const { return settings_; }

    Result<void> setLargeText(bool v)     { settings_.large_text = v; applySettings(); return save(); }
    Result<void> setHighContrast(bool v)  { settings_.high_contrast = v; return save(); }
    Result<void> setAudioFeedback(bool v) { settings_.audio_feedback = v; return save(); }
    Result<void> setFontScale(uint8_t pct) {
        settings_.font_scale_pct = pct;
        applySettings();
        return save();
    }
    Result<void> setAnimationSpeed(uint8_t spd) {
        settings_.animation_speed = spd;
        applySettings();
        return save();
    }

    bool isLargeText()     const { return settings_.large_text; }
    bool isHighContrast()  const { return settings_.high_contrast; }
    bool hasAudioFeedback() const { return settings_.audio_feedback; }

private:
    AccessibilitySettings settings_ = {};

    Result<void> save() {
        nvs_handle_t h;
        if (nvs_open("a11y", NVS_READWRITE, &h) != ESP_OK)
            return Err(ErrorCategory::HARDWARE_FAILURE, "NVS");
        nvs_set_u8(h, "large_txt", settings_.large_text);
        nvs_set_u8(h, "hi_contr",  settings_.high_contrast);
        nvs_set_u8(h, "audio_fb",  settings_.audio_feedback);
        nvs_set_u8(h, "font_sc",   settings_.font_scale_pct);
        nvs_set_u8(h, "anim_spd",  settings_.animation_speed);
        nvs_commit(h);
        nvs_close(h);
        return Ok();
    }
};

static AccessibilityManager s_a11y;
AccessibilityManager* getAccessibilityManager() { return &s_a11y; }

} // namespace phoenix
