// ==========================================================================
// FILE: include/phoenix/Core/MeasurementTypes.h
// Measurement types, analysis interfaces, and algorithm strategy pattern
// REQ-ALGO-001: System SHALL support extensible algorithm architecture
// ==========================================================================
#pragma once

#include "phoenix/Core/Result.h"
#include "phoenix/Core/FixedString.h"
#include <cstdint>
#include <cstddef>

namespace phoenix {

// ─── Minimal ImageBuffer for UI-side compilation ──────────────────────
// Full definition lives in the Measurement MCU HAL. The UI MCU only
// needs the type to exist so that the shared Analysis interfaces compile.
struct ImageBuffer {
    const uint8_t* data   = nullptr;
    uint32_t       width  = 0;
    uint32_t       height = 0;
    uint8_t        bpp    = 8;   // bits per pixel
};

// ─── 1D Signal Profile (extracted from LFA strip image) ───────────────
static constexpr size_t MAX_PROFILE_LENGTH = 1024;

struct Profile1D {
    float    values[MAX_PROFILE_LENGTH] = {};
    uint16_t length  = 0;
    float    x_start = 0.0f;   // mm from strip start
    float    x_step  = 0.0f;   // mm per sample
};

// ─── Peak Detection Result ────────────────────────────────────────────
static constexpr size_t MAX_PEAKS = 8;

struct Peak {
    float position;     // x position (mm)
    float height;       // signal intensity (AU)
    float width;        // FWHM (mm)
    float area;         // integrated area
    float snr;          // signal-to-noise ratio
};

struct PeakResult {
    Peak     peaks[MAX_PEAKS] = {};
    uint8_t  count            = 0;
    float    noise_floor      = 0.0f;
    float    baseline_mean    = 0.0f;
};

// ─── ROI (Region of Interest) ─────────────────────────────────────────
struct ROI {
    uint16_t x      = 0;
    uint16_t y      = 0;
    uint16_t width  = 0;
    uint16_t height = 0;
};

// ─── Measurement Result ───────────────────────────────────────────────
enum class QCFlag : uint8_t {
    NONE            = 0x00,
    CONTROL_LINE_WEAK  = 0x01,
    HIGH_BACKGROUND = 0x02,
    LOW_SNR         = 0x04,
    OUT_OF_RANGE    = 0x08,
    CALIBRATION_OLD = 0x10,
};

// Bitwise ops for QCFlag
inline QCFlag operator|(QCFlag a, QCFlag b) {
    return static_cast<QCFlag>(static_cast<uint8_t>(a) | static_cast<uint8_t>(b));
}
inline QCFlag operator&(QCFlag a, QCFlag b) {
    return static_cast<QCFlag>(static_cast<uint8_t>(a) & static_cast<uint8_t>(b));
}
inline bool hasFlag(QCFlag flags, QCFlag check) {
    return (static_cast<uint8_t>(flags) & static_cast<uint8_t>(check)) != 0;
}

struct MeasurementResult {
    float    concentration_ng_ml = 0.0f;
    float    signal_intensity    = 0.0f;
    float    control_line_signal = 0.0f;
    float    test_control_ratio  = 0.0f;
    float    reference_low       = 0.0f;
    float    reference_high      = 0.0f;
    QCFlag   qc_flags            = QCFlag::NONE;
    String64 interpretation;      // "POSITIVE", "NEGATIVE", "BORDERLINE"
    String64 analyte_name;        // "TSH", "CRP", "cTnI" etc.
    String32 unit;                // "ng/mL", "mIU/L" etc.
    uint32_t timestamp           = 0;
    uint32_t measurement_id      = 0;
    float    confidence          = 0.0f;   // 0..1

    // Version tag for serialization compatibility
    static constexpr uint16_t SERIAL_VERSION = 1;
};

// ─── Algorithm Interfaces (Strategy Pattern) ──────────────────────────

/// Extract 1D profile from image along the LFA strip
class IProfileExtractor {
public:
    virtual ~IProfileExtractor() = default;
    virtual Result<Profile1D> extractProfile(
        const ImageBuffer& image,
        const ROI& roi) = 0;
};

/// Estimate and remove baseline from signal
class IBaselineEstimator {
public:
    virtual ~IBaselineEstimator() = default;
    virtual Result<Profile1D> estimateBaseline(const Profile1D& raw) = 0;
    virtual Result<Profile1D> subtractBaseline(
        const Profile1D& raw,
        const Profile1D& baseline) = 0;
};

/// Find peaks in corrected signal
class IPeakFinder {
public:
    virtual ~IPeakFinder() = default;
    virtual Result<PeakResult> findPeaks(
        const Profile1D& corrected,
        float min_height   = 0.05f,
        float min_distance = 0.5f) = 0;
};

/// Apply correction (e.g., polynomial background) to signal
class ICurveCorrection {
public:
    virtual ~ICurveCorrection() = default;
    virtual Result<Profile1D> correct(const Profile1D& raw) = 0;
};

} // namespace phoenix
