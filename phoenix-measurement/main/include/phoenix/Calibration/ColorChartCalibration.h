// ==========================================================================
// FILE: include/phoenix/Calibration/ColorChartCalibration.h
// Phoenix v108.0 — Color Chart Reference Plate Calibration System
// Based on DXR.007.01-01.DR (Left) and DXR.007.01-02.DR (Right)
// 
// The Igloo Pro uses cardboard color chart plates with 11 defined
// color strips for camera white balance, linearity, and intensity
// calibration. This is a KEY differentiator of the Igloo platform.
//
// Plate dimensions: 8.75 × 45 mm, thickness 0.4–0.6 mm
// 11 strips: 1.8 mm wide × 7 mm long, 0.2 mm spacing
// ==========================================================================
#pragma once

#include "phoenix/Core/Result.h"
#include "phoenix/Core/FixedString.h"
#include <cstdint>
#include <cmath>

namespace phoenix {

// ── Color Chart Strip Definitions (from DXR.007.01 drawings) ──────────
// Strip ordering: 0 (top) through 10 (bottom) on the plate

struct ColorReference {
    uint8_t  strip_id;     // 0–10
    uint8_t  r, g, b;      // Expected sRGB values
    uint32_t hex;           // Hex code from drawing
    const char* name;       // Human-readable
    const char* purpose;    // Calibration purpose
};

// DXR.007.01 defines these exact color codes:
static constexpr ColorReference COLOR_CHART[11] = {
    // id   R     G     B     hex        name           purpose
    { 0,    0,    0,    0,  0x000000, "Black",     "Dark reference / baseline zero"},
    { 1,  255,  255,  255,  0xFFFFFF, "White",     "Full-scale reference / gain"},
    { 2,  117,   94,    5,  0x755E05, "Dark Gold", "Mid-tone / LFT gold nanoparticle reference"},
    { 3,  255,    0,  255,  0xFF00FF, "Magenta",   "Red+Blue channel cross-talk"},
    { 4,  255,  255,    0,  0xFFFF00, "Yellow",    "Red+Green linearity / LFT background"},
    { 5,    0,    0,    0,  0x000000, "Black 2",   "Repeated dark reference (drift check)"},
    { 6,  255,  255,  255,  0xFFFFFF, "White 2",   "Repeated white reference (drift check)"},
    { 7,    0,    0,  255,  0x0000FF, "Blue",      "Blue channel sensitivity"},
    { 8,    0,  255,    0,  0x00FF00, "Green",     "Green channel sensitivity"},
    { 9,  255,    0,    0,  0xFF0000, "Red",       "Red channel sensitivity / hemoglobin ref"},
    {10,  255,  255,  255,  0xFFFFFF, "White 3",   "Final white reference (stability check)"},
};

static constexpr size_t NUM_STRIPS = 11;

// ── Physical Plate Dimensions (mm) ───────────────────────────────────
struct PlateGeometry {
    static constexpr float PLATE_WIDTH_MM     = 8.75f;
    static constexpr float PLATE_HEIGHT_MM    = 45.0f;
    static constexpr float PLATE_THICKNESS_MM = 0.5f;  // 0.4–0.6
    static constexpr float STRIP_WIDTH_MM     = 1.8f;
    static constexpr float STRIP_LENGTH_MM    = 7.0f;
    static constexpr float STRIP_SPACING_MM   = 0.2f;
    static constexpr float STRIP_TOTAL_MM     = 
        NUM_STRIPS * STRIP_WIDTH_MM + (NUM_STRIPS - 1) * STRIP_SPACING_MM;
    // = 11 * 1.8 + 10 * 0.2 = 21.8 mm (fits within 45mm plate)
    
    static constexpr float TOP_MARGIN_MM      = 13.0f;  // from plate top to first strip
    static constexpr float HOLE_OFFSET_Y_MM   = 5.0f;   // registration hole
    static constexpr float HOLE_OFFSET_X_MM   = 5.5f;   // from drawing
};

// ── Measured Strip Values ─────────────────────────────────────────────
// Raw camera readings for each strip
struct StripReading {
    uint8_t  strip_id;
    float    raw_intensity;    // Grayscale (OV9281 is mono)
    float    r_channel;        // With color filter (if available)
    float    g_channel;
    float    b_channel;
    float    uniformity;       // Spatial uniformity across strip (0–1)
    bool     valid;
};

// ── Calibration Coefficients ──────────────────────────────────────────
// Computed from color chart measurement

struct IntensityCalibration {
    // Gain and offset (linear model: corrected = gain * raw + offset)
    float gain    = 1.0f;
    float offset  = 0.0f;
    
    // Gamma correction
    float gamma   = 1.0f;
    
    // Non-linearity correction (quadratic: y = a*x² + b*x + c)
    float nl_a    = 0.0f;
    float nl_b    = 1.0f;
    float nl_c    = 0.0f;
    
    // Apply correction
    float correct(float raw) const {
        float lin = nl_a * raw * raw + nl_b * raw + nl_c;
        return gain * lin + offset;
    }
};

struct WhiteBalanceCalibration {
    float r_gain = 1.0f;
    float g_gain = 1.0f;
    float b_gain = 1.0f;
    
    void apply(float& r, float& g, float& b) const {
        r *= r_gain;
        g *= g_gain;
        b *= b_gain;
    }
};

struct ColorChartCalibrationResult {
    static constexpr uint32_t MAGIC = 0x43434C31;  // "CCL1"
    static constexpr uint16_t VERSION = 1;
    
    uint32_t magic   = MAGIC;
    uint16_t version = VERSION;
    
    // Intensity calibration (from Black/White strips)
    IntensityCalibration intensity;
    
    // White balance (from RGB strips)
    WhiteBalanceCalibration white_balance;
    
    // Reference values for LFT gold nanoparticle detection
    float gold_np_reference     = 0.0f;   // Strip 2 (Dark Gold #755E05) intensity
    float gold_np_to_white_ratio = 0.0f;  // Gold / White ratio
    
    // Hemoglobin interference reference
    float hemoglobin_reference  = 0.0f;   // Strip 9 (Red) intensity
    
    // Drift indicators (repeated Black/White)
    float dark_drift            = 0.0f;   // |Black1 - Black2| / Black1
    float white_drift           = 0.0f;   // max(|W1-W2|, |W1-W3|, |W2-W3|) / W1
    
    // Overall quality
    float r_squared             = 0.0f;   // Linearity R²
    float uniformity_score      = 0.0f;   // Average strip uniformity
    uint32_t timestamp          = 0;
    uint32_t expires_at         = 0;      // 24h expiry for production use
    
    bool isValid() const {
        return magic == MAGIC && version == VERSION && r_squared > 0.95f;
    }
    
    bool isExpired(uint32_t now) const {
        return expires_at > 0 && now > expires_at;
    }
};

// ── Color Chart Calibration Pipeline ──────────────────────────────────

class ColorChartCalibrator {
public:
    // ── Step 1: Acquire strip readings from camera image ──────────
    // Called by MeasurementPipeline when a calibration plate is detected
    Result<void> setStripReading(uint8_t strip_id, const StripReading& reading);
    
    // ── Step 2: Validate all strips are present and readable ──────
    Result<void> validateReadings();
    
    // ── Step 3: Compute calibration ───────────────────────────────
    // This is the core algorithm:
    //   a) Dark reference from strips 0,5 (Black)
    //   b) White reference from strips 1,6,10 (White)
    //   c) Gain/offset from Black-White pair
    //   d) Gamma from mid-tone (Gold strip 2)
    //   e) RGB channel gains from strips 7,8,9 (Blue/Green/Red)
    //   f) Cross-talk correction from strips 3,4 (Magenta/Yellow)
    //   g) Drift check: compare repeated Black/White strips
    //   h) Linearity R² from all 11 strips
    Result<ColorChartCalibrationResult> computeCalibration();
    
    // ── Step 4: Save to NVS ───────────────────────────────────────
    Result<void> saveCalibration(const ColorChartCalibrationResult& cal);
    Result<ColorChartCalibrationResult> loadCalibration();
    
    // ── Apply calibration to a raw pixel value ────────────────────
    float applyIntensityCorrection(float raw) const;
    void  applyWhiteBalance(float& r, float& g, float& b) const;
    
    // ── Gold nanoparticle reference for LFT quantitation ──────────
    // Returns expected signal intensity for the gold NP reference color
    float getGoldNPReference() const { return active_.gold_np_reference; }
    float getGoldNPRatio() const { return active_.gold_np_to_white_ratio; }
    
    // ── Status ────────────────────────────────────────────────────
    bool hasActiveCalibration() const { return active_.isValid(); }
    uint8_t getStripCount() const { return readings_count_; }
    const ColorChartCalibrationResult& getActive() const { return active_; }
    
private:
    StripReading readings_[NUM_STRIPS] = {};
    uint8_t      readings_count_       = 0;
    ColorChartCalibrationResult active_ = {};
    
    // Internal computation helpers
    float computeDarkReference() const;
    float computeWhiteReference() const;
    float computeGamma(float dark, float white) const;
    float computeLinearity() const;
};

} // namespace phoenix
