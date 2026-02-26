// ==========================================================================
// FILE: src/Calibration/ColorChartCalibration.cpp
// Phoenix v108.0 — Color Chart Calibration Pipeline Implementation
// DXR.007.01 reference plate processing
//
// The calibration algorithm:
//   1. Read all 11 strips from the camera image
//   2. Extract dark reference (strips 0,5 — Black)
//   3. Extract white reference (strips 1,6,10 — White)
//   4. Compute gain/offset: corrected = gain * (raw - dark)
//   5. Compute gamma from mid-tone strip 2 (Gold #755E05)
//   6. Compute RGB channel gains from strips 7,8,9
//   7. Validate drift between repeated Black/White strips
//   8. Store gold nanoparticle reference for LFT quantitation
// ==========================================================================
#include "phoenix/Calibration/ColorChartCalibration.h"
#include <esp_log.h>
#include <nvs_flash.h>
#include <nvs.h>
#include <cstring>
#include <cmath>

namespace phoenix {

static const char* TAG = "ColorCal";

// NVS storage key
static constexpr const char* NVS_NAMESPACE = "color_cal";
static constexpr const char* NVS_KEY       = "cal_data";

// ── Strip Reading Storage ─────────────────────────────────────────────

Result<void> ColorChartCalibrator::setStripReading(uint8_t strip_id,
                                                     const StripReading& reading) {
    if (strip_id >= NUM_STRIPS) {
        ESP_LOGE(TAG, "Invalid strip ID: %d", strip_id);
        return Err(ErrorCategory::INVALID_PARAMETER, "Strip ID out of range");
    }
    
    readings_[strip_id] = reading;
    readings_[strip_id].strip_id = strip_id;
    readings_[strip_id].valid = true;
    
    // Count valid readings
    readings_count_ = 0;
    for (size_t i = 0; i < NUM_STRIPS; i++) {
        if (readings_[i].valid) readings_count_++;
    }
    
    ESP_LOGI(TAG, "Strip %d: intensity=%.3f uniform=%.2f (%d/%d complete)",
             strip_id,
             static_cast<double>(reading.raw_intensity),
             static_cast<double>(reading.uniformity),
             readings_count_, NUM_STRIPS);
    
    return Ok();
}

// ── Validate All Readings ─────────────────────────────────────────────

Result<void> ColorChartCalibrator::validateReadings() {
    // Check all strips are present
    for (size_t i = 0; i < NUM_STRIPS; i++) {
        if (!readings_[i].valid) {
            ESP_LOGE(TAG, "Missing strip %d (%s)", (int)i, COLOR_CHART[i].name);
            return Err(ErrorCategory::INVALID_PARAMETER, "Missing strip reading");
        }
    }
    
    // Check Black strips are actually dark
    float black_threshold = 0.15f;  // max 15% intensity for "black"
    if (readings_[0].raw_intensity > black_threshold ||
        readings_[5].raw_intensity > black_threshold) {
        ESP_LOGW(TAG, "Black strips too bright: S0=%.3f S5=%.3f",
                 static_cast<double>(readings_[0].raw_intensity),
                 static_cast<double>(readings_[5].raw_intensity));
        return Err(ErrorCategory::CALIBRATION_ERROR,
                       "Black reference too bright — check plate orientation");
    }
    
    // Check White strips are actually bright
    float white_threshold = 0.60f;  // min 60% intensity for "white"
    if (readings_[1].raw_intensity < white_threshold ||
        readings_[6].raw_intensity < white_threshold ||
        readings_[10].raw_intensity < white_threshold) {
        ESP_LOGW(TAG, "White strips too dim: S1=%.3f S6=%.3f S10=%.3f",
                 static_cast<double>(readings_[1].raw_intensity),
                 static_cast<double>(readings_[6].raw_intensity),
                 static_cast<double>(readings_[10].raw_intensity));
        return Err(ErrorCategory::CALIBRATION_ERROR,
                       "White reference too dim — check illumination");
    }
    
    // Check uniformity (each strip should be > 0.85 uniform)
    for (size_t i = 0; i < NUM_STRIPS; i++) {
        if (readings_[i].uniformity < 0.80f) {
            ESP_LOGW(TAG, "Strip %d poor uniformity: %.2f", (int)i,
                     static_cast<double>(readings_[i].uniformity));
            // Warning only, don't fail
        }
    }
    
    ESP_LOGI(TAG, "All %d strips validated", NUM_STRIPS);
    return Ok();
}

// ── Internal: Dark Reference ──────────────────────────────────────────

float ColorChartCalibrator::computeDarkReference() const {
    // Average of the two Black strips (0 and 5)
    return (readings_[0].raw_intensity + readings_[5].raw_intensity) / 2.0f;
}

// ── Internal: White Reference ─────────────────────────────────────────

float ColorChartCalibrator::computeWhiteReference() const {
    // Average of the three White strips (1, 6, 10)
    return (readings_[1].raw_intensity +
            readings_[6].raw_intensity +
            readings_[10].raw_intensity) / 3.0f;
}

// ── Internal: Gamma from Mid-tone ─────────────────────────────────────

float ColorChartCalibrator::computeGamma(float dark, float white) const {
    // Strip 2 (Dark Gold #755E05) has known expected luminance
    // sRGB luminance of #755E05: Y = 0.2126*0x75/255 + 0.7152*0x5E/255 + 0.0722*0x05/255
    //                            Y ≈ 0.2126*0.459 + 0.7152*0.369 + 0.0722*0.020
    //                            Y ≈ 0.0976 + 0.2639 + 0.0014 = 0.3629
    float expected_normalized = 0.363f;  // Expected luminance in 0–1 range
    
    // Measured normalized value
    float measured_normalized = 0.0f;
    if (fabsf(white - dark) > 0.001f) {
        measured_normalized = (readings_[2].raw_intensity - dark) / (white - dark);
    }
    
    if (measured_normalized <= 0.01f || measured_normalized >= 0.99f) {
        ESP_LOGW(TAG, "Gamma: gold strip out of range (%.3f), using gamma=1.0",
                 static_cast<double>(measured_normalized));
        return 1.0f;
    }
    
    // gamma = log(expected) / log(measured)
    float gamma = logf(expected_normalized) / logf(measured_normalized);
    
    // Clamp to reasonable range
    if (gamma < 0.5f) gamma = 0.5f;
    if (gamma > 3.0f) gamma = 3.0f;
    
    ESP_LOGI(TAG, "Gamma: expected=%.3f measured=%.3f → gamma=%.3f",
             static_cast<double>(expected_normalized),
             static_cast<double>(measured_normalized),
             static_cast<double>(gamma));
    
    return gamma;
}

// ── Internal: Linearity R² ────────────────────────────────────────────

float ColorChartCalibrator::computeLinearity() const {
    // Compute R² of measured vs expected luminance for all 11 strips
    // Expected luminance values from sRGB:
    static const float expected_lum[NUM_STRIPS] = {
        0.000f,  // Black
        1.000f,  // White
        0.363f,  // Dark Gold
        0.285f,  // Magenta (R+B)
        0.928f,  // Yellow (R+G)
        0.000f,  // Black 2
        1.000f,  // White 2
        0.072f,  // Blue
        0.715f,  // Green
        0.213f,  // Red
        1.000f,  // White 3
    };
    
    float dark = computeDarkReference();
    float white = computeWhiteReference();
    float range = white - dark;
    if (range < 0.01f) return 0.0f;
    
    // Measured normalized values
    float measured[NUM_STRIPS];
    for (size_t i = 0; i < NUM_STRIPS; i++) {
        measured[i] = (readings_[i].raw_intensity - dark) / range;
        if (measured[i] < 0) measured[i] = 0;
        if (measured[i] > 1) measured[i] = 1;
    }
    
    // Compute R²
    float sum_exp = 0, sum_meas = 0;
    for (size_t i = 0; i < NUM_STRIPS; i++) {
        sum_exp += expected_lum[i];
        sum_meas += measured[i];
    }
    float mean_exp = sum_exp / NUM_STRIPS;
    float mean_meas = sum_meas / NUM_STRIPS;
    
    float ss_tot = 0, ss_res = 0;
    for (size_t i = 0; i < NUM_STRIPS; i++) {
        float diff_tot = measured[i] - mean_meas;
        ss_tot += diff_tot * diff_tot;
        
        // Simple linear model: measured = a * expected + b
        // For R², we compare measured to mean(measured)
        float predicted = expected_lum[i];  // Ideal prediction
        float diff_res = measured[i] - predicted;
        ss_res += diff_res * diff_res;
    }
    
    float r2 = (ss_tot > 0) ? (1.0f - ss_res / ss_tot) : 0.0f;
    if (r2 < 0) r2 = 0;
    
    ESP_LOGI(TAG, "Linearity R² = %.4f", static_cast<double>(r2));
    return r2;
}

// ── Main Calibration Computation ──────────────────────────────────────

Result<ColorChartCalibrationResult> ColorChartCalibrator::computeCalibration() {
    ESP_LOGI(TAG, "═══ Computing Color Chart Calibration ═══");
    
    // Step 1: Validate
    auto val_result = validateReadings();
    if (val_result.is_err()) {
        return Err<ColorChartCalibrationResult>(
            val_result.error().category, val_result.error().message);
    }
    
    ColorChartCalibrationResult cal = {};
    
    // Step 2: Dark/White references
    float dark  = computeDarkReference();
    float white = computeWhiteReference();
    float range = white - dark;
    
    ESP_LOGI(TAG, "Dark ref: %.4f  White ref: %.4f  Range: %.4f",
             static_cast<double>(dark), static_cast<double>(white),
             static_cast<double>(range));
    
    if (range < 0.1f) {
        return Err<ColorChartCalibrationResult>(
            ErrorCategory::CALIBRATION_ERROR,
            "Insufficient dynamic range (White - Black < 0.1)");
    }
    
    // Step 3: Intensity calibration (gain/offset)
    cal.intensity.gain   = 1.0f / range;
    cal.intensity.offset = -dark / range;
    
    // Step 4: Gamma correction from Gold strip
    cal.intensity.gamma = computeGamma(dark, white);
    
    // Step 5: Non-linearity (quadratic fit through Black, Gold, White)
    // Using 3-point fit: (0, dark), (0.363, gold), (1.0, white)
    float x0 = 0.0f, y0 = dark;
    float x1 = 0.363f, y1 = readings_[2].raw_intensity;
    float x2 = 1.0f, y2 = white;
    
    // Solve: y = a*x² + b*x + c with 3 points
    // c = y0 = dark
    float c = y0;
    // a*x1² + b*x1 = y1 - c
    // a*x2² + b*x2 = y2 - c
    float det = x1*x1*x2 - x2*x2*x1;
    if (fabsf(det) > 0.0001f) {
        cal.intensity.nl_a = ((y1 - c)*x2 - (y2 - c)*x1) / det;
        cal.intensity.nl_b = ((y2 - c)*x1*x1 - (y1 - c)*x2*x2) / det;
        cal.intensity.nl_c = c;
    } else {
        // Fallback to linear
        cal.intensity.nl_a = 0;
        cal.intensity.nl_b = range;
        cal.intensity.nl_c = dark;
    }
    
    // Step 6: White balance from RGB strips (7=Blue, 8=Green, 9=Red)
    // For OV9281 mono sensor: these give relative spectral response
    // The mono sensor with different illumination wavelengths gives 
    // channel-like information when using UV/visible LED switching
    float blue_normalized  = (readings_[7].raw_intensity - dark) / range;
    float green_normalized = (readings_[8].raw_intensity - dark) / range;
    float red_normalized   = (readings_[9].raw_intensity - dark) / range;
    
    // Expected: pure blue=0.072, green=0.715, red=0.213 (luminance)
    // But with mono sensor under white light, all appear as gray
    // The actual WB matters for multi-LED illumination systems
    if (blue_normalized > 0.01f)
        cal.white_balance.b_gain = 0.072f / blue_normalized;
    if (green_normalized > 0.01f)
        cal.white_balance.g_gain = 0.715f / green_normalized;
    if (red_normalized > 0.01f)
        cal.white_balance.r_gain = 0.213f / red_normalized;
    
    ESP_LOGI(TAG, "WB gains: R=%.3f G=%.3f B=%.3f",
             static_cast<double>(cal.white_balance.r_gain),
             static_cast<double>(cal.white_balance.g_gain),
             static_cast<double>(cal.white_balance.b_gain));
    
    // Step 7: Gold nanoparticle reference (critical for LFT!)
    cal.gold_np_reference = readings_[2].raw_intensity;
    cal.gold_np_to_white_ratio = readings_[2].raw_intensity / white;
    
    ESP_LOGI(TAG, "Gold NP ref: %.4f  Ratio to white: %.4f",
             static_cast<double>(cal.gold_np_reference),
             static_cast<double>(cal.gold_np_to_white_ratio));
    
    // Step 8: Hemoglobin interference reference
    cal.hemoglobin_reference = readings_[9].raw_intensity;
    
    // Step 9: Drift check
    float black_drift = fabsf(readings_[0].raw_intensity - readings_[5].raw_intensity);
    if (readings_[0].raw_intensity > 0.001f)
        cal.dark_drift = black_drift / readings_[0].raw_intensity;
    
    float w1 = readings_[1].raw_intensity;
    float w2 = readings_[6].raw_intensity;
    float w3 = readings_[10].raw_intensity;
    float max_w_diff = fmaxf(fabsf(w1-w2), fmaxf(fabsf(w1-w3), fabsf(w2-w3)));
    if (w1 > 0.001f)
        cal.white_drift = max_w_diff / w1;
    
    ESP_LOGI(TAG, "Drift: dark=%.4f white=%.4f %s",
             static_cast<double>(cal.dark_drift),
             static_cast<double>(cal.white_drift),
             (cal.dark_drift < 0.05f && cal.white_drift < 0.03f) ? "✓ OK" : "⚠ WARNING");
    
    // Step 10: Linearity
    cal.r_squared = computeLinearity();
    
    // Step 11: Uniformity score (average)
    float uni_sum = 0;
    for (size_t i = 0; i < NUM_STRIPS; i++) {
        uni_sum += readings_[i].uniformity;
    }
    cal.uniformity_score = uni_sum / NUM_STRIPS;
    
    // Timestamp and expiry (24h)
    // cal.timestamp = getEpoch(); // TODO: real time
    cal.expires_at = cal.timestamp + 86400;
    
    ESP_LOGI(TAG, "═══ Calibration Complete ═══");
    ESP_LOGI(TAG, "  Gain: %.4f  Offset: %.4f  Gamma: %.3f",
             static_cast<double>(cal.intensity.gain),
             static_cast<double>(cal.intensity.offset),
             static_cast<double>(cal.intensity.gamma));
    ESP_LOGI(TAG, "  R²: %.4f  Uniformity: %.2f",
             static_cast<double>(cal.r_squared),
             static_cast<double>(cal.uniformity_score));
    ESP_LOGI(TAG, "  Gold NP ref: %.4f (ratio: %.4f)",
             static_cast<double>(cal.gold_np_reference),
             static_cast<double>(cal.gold_np_to_white_ratio));
    
    if (!cal.isValid()) {
        ESP_LOGW(TAG, "Calibration R² < 0.95 — consider recalibrating");
    }
    
    // Store as active
    active_ = cal;
    
    return Ok(cal);
}

// ── NVS Persistence ───────────────────────────────────────────────────

Result<void> ColorChartCalibrator::saveCalibration(
    const ColorChartCalibrationResult& cal) {
    
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        return Err(ErrorCategory::HARDWARE_FAILURE, "NVS open failed");
    }
    
    err = nvs_set_blob(handle, NVS_KEY, &cal, sizeof(cal));
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }
    nvs_close(handle);
    
    if (err != ESP_OK) {
        return Err(ErrorCategory::HARDWARE_FAILURE, "NVS write failed");
    }
    
    ESP_LOGI(TAG, "Calibration saved to NVS (%d bytes)", (int)sizeof(cal));
    return Ok();
}

Result<ColorChartCalibrationResult> ColorChartCalibrator::loadCalibration() {
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        return Err<ColorChartCalibrationResult>(
            ErrorCategory::NOT_FOUND, "No saved calibration");
    }
    
    ColorChartCalibrationResult cal = {};
    size_t required_size = sizeof(cal);
    err = nvs_get_blob(handle, NVS_KEY, &cal, &required_size);
    nvs_close(handle);
    
    if (err != ESP_OK || !cal.isValid()) {
        return Err<ColorChartCalibrationResult>(
            ErrorCategory::CALIBRATION_ERROR, "Stored calibration invalid");
    }
    
    active_ = cal;
    ESP_LOGI(TAG, "Calibration loaded from NVS (R²=%.4f)",
             static_cast<double>(cal.r_squared));
    
    return Ok(cal);
}

// ── Apply Corrections ─────────────────────────────────────────────────

float ColorChartCalibrator::applyIntensityCorrection(float raw) const {
    if (!active_.isValid()) return raw;
    return active_.intensity.correct(raw);
}

void ColorChartCalibrator::applyWhiteBalance(float& r, float& g, float& b) const {
    if (!active_.isValid()) return;
    active_.white_balance.apply(r, g, b);
}

} // namespace phoenix
