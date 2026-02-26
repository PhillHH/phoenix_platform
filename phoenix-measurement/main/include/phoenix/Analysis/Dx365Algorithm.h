// ==========================================================================
// FILE: include/phoenix/Analysis/Dx365Algorithm.h
// Phoenix v108.0 — Verified Dx365 Algorithm Implementation
//
// Source: MCP project data (44 measurement sessions, IgE assay)
// Verified against real Dx365 reader measurements from 2025-10-31/11-05
// Device: DX365 S/N 010426247918003421T0DA2510000001, FW 1.8.49.0, HW DA
//
// KEY FINDINGS FROM REAL DATA:
//   - Camera: 1296×812 RGB, ROI: 76×346 px (3.9×13.2 mm)
//   - Scale: 0.036 mm/px (27.8 px/mm)
//   - Profile: 346-350 float values (1D intensity along cassette)
//   - Peak detection: 7-point symmetric fit per peak
//   - Signal: T/C ratio (Test Line ÷ Control Line intensity)
//   - Calibration: 5PL with G parameter (not E!)
//   - Control Line: center ≈ idx 47, width ≈ 44px, value ≈ 20-22
//   - Test Lines: configurable positions per cassette type
// ==========================================================================
#pragma once

#include "phoenix/Core/Result.h"
#include "phoenix/Core/FixedString.h"
#include <cstdint>
#include <cmath>
#include <cstring>

namespace phoenix {

// ══════════════════════════════════════════════════════════════════════
// 5PL MODEL WITH G PARAMETER (Dx365 convention)
// y = D + (A - D) / (1 + (x/C)^B)^G
//
// A = minimum asymptote (blank signal)
// B = Hill slope
// C = inflection point (EC50)
// D = maximum asymptote (saturated signal)
// G = asymmetry factor (Dx365 uses G, not E)
// ══════════════════════════════════════════════════════════════════════

struct FivePL_G {
    float A = 0.0f;
    float B = 1.0f;
    float C = 100.0f;
    float D = 1.0f;
    float G = 1.0f;

    // Forward: concentration → signal
    float evaluate(float x) const {
        if (x <= 0.0f) return A;
        float ratio = powf(x / C, B);
        return D + (A - D) / powf(1.0f + ratio, G);
    }

    // Inverse: signal → concentration
    float inverse(float y) const {
        if (fabsf(y - D) < 1e-8f) return 1e6f;  // at max asymptote
        if (fabsf(A - D) < 1e-8f) return C;       // degenerate
        float inner = (A - D) / (y - D);
        if (inner <= 0.0f) return 0.0f;
        float powered = powf(inner, 1.0f / G) - 1.0f;
        if (powered <= 0.0f) return 0.0f;
        return C * powf(powered, 1.0f / B);
    }
};

// ══════════════════════════════════════════════════════════════════════
// 7-POINT PEAK DESCRIPTOR (from MCP measurement sessions)
//
// Each peak is described by 7 index positions along the profile:
//   leftExpect → leftLimit → leftPeak → center → rightPeak → rightLimit → rightExpect
//
// Plus metrics: width, value (area), height, integral, valid
// ══════════════════════════════════════════════════════════════════════

struct PeakDescriptor {
    // 7-point indices in the 1D profile
    uint16_t left_expect_idx;   // Expected start of peak region
    uint16_t left_limit_idx;    // Start of actual peak base
    uint16_t left_peak_idx;     // Start of peak shoulders
    uint16_t center_idx;        // Peak center (maximum deviation)
    uint16_t right_peak_idx;    // End of peak shoulders
    uint16_t right_limit_idx;   // End of actual peak base
    uint16_t right_expect_idx;  // Expected end of peak region

    // Metrics
    uint16_t width;             // Peak width in pixels
    float    value;             // Peak area (integrated intensity)
    float    height;            // Peak height from baseline
    float    integral;          // Full integral including negative lobes
    bool     valid;             // Peak detection succeeded
};

// ══════════════════════════════════════════════════════════════════════
// LINE PROFILE (1D intensity along the cassette)
//
// From real data: 346-350 float values, range ~35 (peak) to ~170 (baseline)
// Note: INVERTED — lower value = more gold nanoparticle = stronger signal
// ══════════════════════════════════════════════════════════════════════

static constexpr size_t MAX_PROFILE_LEN = 400;

struct LineProfile {
    float    data[MAX_PROFILE_LEN] = {};
    uint16_t length = 0;

    // Real data characteristics (from MCP):
    // - Typical baseline: ~165-170
    // - Strong peak (CL): drops to ~35-40
    // - Weak peak (TL at low conc): barely visible dip
    // - Profile extracted from 76-pixel wide ROI, averaged across width
};

// ══════════════════════════════════════════════════════════════════════
// ASSAY LINE DEFINITION (from MCP project.json)
// ══════════════════════════════════════════════════════════════════════

struct AssayLine {
    String32 id;
    bool     is_control;        // true = Control Line, false = Test Line
    float    x_position;        // Expected position in ROI (px from left)
    float    x_width;           // Expected width (px)
    uint8_t  color_id;          // Decorative color for UI
    String64 assay_id;          // Links to assay definition
};

static constexpr size_t MAX_LINES = 8;

// ══════════════════════════════════════════════════════════════════════
// ASSAY CONFIGURATION (from MCP project.json)
// ══════════════════════════════════════════════════════════════════════

enum class SignalType : uint8_t {
    TL_DIV_CL = 0,    // Test Line / Control Line (most common)
    TL_ONLY   = 1,    // Test Line intensity only
    CL_ONLY   = 2,    // Control Line only
    TL_MINUS_CL = 3,  // Test Line - Control Line
};

enum class ScaleType : uint8_t {
    LINEAR = 0,
    LOG    = 1,
};

struct AssayConfig {
    String32    id;
    String64    name;
    String16    loinc_id;         // LOINC code (e.g., "51651-8")
    uint8_t     measure_unit_id;  // Unit code
    SignalType  signal_type;
    ScaleType   scale_type;
    uint16_t    incubation_sec;

    // 5PL coefficients (3 curves per assay)
    FivePL_G    test_5pl;         // Maps concentration → test line peak value
    FivePL_G    control_5pl;      // Maps concentration → control line peak value
    FivePL_G    div_5pl;          // Maps concentration → T/C ratio (PRIMARY)

    // Line definitions
    AssayLine   lines[MAX_LINES];
    uint8_t     num_lines;
};

// ══════════════════════════════════════════════════════════════════════
// ROI CONFIGURATION (from MCP device calibration)
// ══════════════════════════════════════════════════════════════════════

struct ROIConfig {
    // Pixel coordinates in full camera image (1296×812)
    float x_px, y_px, w_px, h_px;

    // Physical coordinates (mm)
    float x_mm, y_mm, w_mm, h_mm;

    // Calibration factors
    float    scale_factor;    // mm per pixel (0.036 from real device)
    uint16_t x_bias;          // Camera X bias (616 from real device)
    uint16_t y_bias;          // Camera Y bias (384 from real device)
};

// ══════════════════════════════════════════════════════════════════════
// MEASUREMENT RESULT (matches MCP session structure)
// ══════════════════════════════════════════════════════════════════════

struct Dx365MeasurementResult {
    // Peak detection results
    PeakDescriptor peaks[MAX_LINES];
    uint8_t        num_peaks;

    // Control line
    float control_line_intensity;

    // Per-assay results
    struct AssayResult {
        String32 assay_id;
        float    intensity;           // Peak value for this assay
        float    original_intensity;  // Before corrections
        float    red_intensity;       // Per-channel (if applicable)
        float    green_intensity;
        float    blue_intensity;
        float    concentration;       // Computed from 5PL inverse
    };
    AssayResult assay_results[MAX_LINES];
    uint8_t     num_assay_results;

    // Histogram (1D profile)
    LineProfile profile;

    // Device info
    String64 device_id;
    String16 fw_version;

    // Timestamps
    uint32_t started_at;
    uint32_t finished_at;

    bool succeeded;
};

// ══════════════════════════════════════════════════════════════════════
// VERIFIED FACTORY CALIBRATIONS FROM MCP DATA
// ══════════════════════════════════════════════════════════════════════

// IgE Assay (from MCP project, verified against 44 measurements)
inline AssayConfig getVerifiedIgEAssay() {
    AssayConfig cfg = {};
    cfg.id = "IgE-LFT";
    cfg.name = "IgE Ab [Presence] in Serum or Plasma";
    cfg.loinc_id = "51651-8";
    cfg.measure_unit_id = 77;
    cfg.signal_type = SignalType::TL_DIV_CL;
    cfg.scale_type = ScaleType::LINEAR;
    cfg.incubation_sec = 600;  // 10 min

    // VERIFIED Test 5PL (concentration → peak value)
    // Verified: 0.6–20.8% error across 1–200 AU range
    cfg.test_5pl = {
        0.01896f,    // A - blank signal
        1.75754f,    // B - Hill slope
        117.834f,    // C - EC50
        5.82638f,    // D - max signal
        3.77055f     // G - asymmetry
    };

    // Control 5PL (concentration → control line value)
    cfg.control_5pl = {
        -12581.79f,   // A
        -0.01987f,    // B
        31697768.0f,  // C
        24.0337f,     // D
        10.0f         // G
    };

    // VERIFIED Division 5PL (concentration → T/C ratio) — PRIMARY
    // Verified: 0.1–16% error, excellent fit at clinical range
    cfg.div_5pl = {
        -0.000202f,   // A - blank T/C ratio
        1.59834f,     // B - Hill slope
        255.486f,     // C - EC50
        0.28367f,     // D - max T/C ratio
        10.0f         // G - asymmetry
    };

    // Line definitions (from MCP project.json)
    cfg.lines[0] = {"ctrl", true,  33.3f, 44.2f, 0, "ctrl"};
    cfg.lines[1] = {"tl1",  false, 138.3f, 45.6f, 2, cfg.id.c_str()};
    cfg.lines[2] = {"tl2",  false, 247.1f, 43.9f, 3, cfg.id.c_str()};
    cfg.num_lines = 3;

    return cfg;
}

// ══════════════════════════════════════════════════════════════════════
// VERIFIED ROI CONFIG FROM REAL DEVICE
// ══════════════════════════════════════════════════════════════════════

inline ROIConfig getVerifiedROI() {
    ROIConfig roi = {};
    roi.x_px = 567.26f;
    roi.y_px = 358.45f;
    roi.w_px = 108.53f;
    roi.h_px = 366.30f;
    roi.x_mm = -1.754f;
    roi.y_mm = 0.919f;
    roi.w_mm = 3.905f;
    roi.h_mm = 13.181f;
    roi.scale_factor = 0.03598f;  // mm/px
    roi.x_bias = 616;
    roi.y_bias = 384;
    return roi;
}

// ══════════════════════════════════════════════════════════════════════
// PEAK DETECTION ALGORITHM
// Matches the 7-point peak detection used by Dx365 firmware
// ══════════════════════════════════════════════════════════════════════

class Dx365PeakDetector {
public:
    // Detect peaks in a 1D profile
    // The profile is INVERTED: lower values = stronger signal
    Result<void> detectPeaks(
        const LineProfile& profile,
        const AssayLine* expected_lines,
        uint8_t num_lines,
        PeakDescriptor* out_peaks,
        uint8_t& out_count);

    // Compute T/C value for a detected peak
    static float computePeakValue(
        const LineProfile& profile,
        const PeakDescriptor& peak);

    // Compute T/C ratio
    static float computeTCRatio(
        float test_value,
        float control_value);

private:
    // Find peak center near expected position
    uint16_t findPeakCenter(
        const LineProfile& profile,
        uint16_t expected_center,
        uint16_t search_radius);

    // Find peak boundaries using gradient analysis
    void findPeakBoundaries(
        const LineProfile& profile,
        uint16_t center,
        uint16_t expected_width,
        PeakDescriptor& peak);

    // Compute peak metrics
    void computePeakMetrics(
        const LineProfile& profile,
        PeakDescriptor& peak);
};

// ══════════════════════════════════════════════════════════════════════
// COMPLETE MEASUREMENT PIPELINE
// ══════════════════════════════════════════════════════════════════════

class Dx365MeasurementPipeline {
public:
    // Configure with assay and ROI
    void configure(const AssayConfig& assay, const ROIConfig& roi);

    // Process a measurement (from camera image to result)
    Result<Dx365MeasurementResult> process(
        const uint8_t* image_data,
        uint32_t image_width,
        uint32_t image_height);

    // Process from pre-extracted profile (for demo/testing)
    Result<Dx365MeasurementResult> processProfile(
        const LineProfile& profile);

private:
    AssayConfig assay_ = {};
    ROIConfig   roi_ = {};
    Dx365PeakDetector detector_;

    // Extract 1D profile from 2D image ROI
    LineProfile extractProfile(
        const uint8_t* image_data,
        uint32_t image_width,
        uint32_t image_height);

    // Apply Color Chart calibration to profile
    void applyCalibration(LineProfile& profile);

    // Convert peak values to concentrations using 5PL
    void computeConcentrations(
        Dx365MeasurementResult& result);
};

} // namespace phoenix
