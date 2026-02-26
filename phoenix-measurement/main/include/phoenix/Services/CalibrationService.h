// ==========================================================================
// FILE: include/phoenix/Services/CalibrationService.h
// 5-Parameter Logistic (5PL) calibration for LFA quantitation
// REQ-CAL-001: System SHALL support 5PL curve fitting
// ==========================================================================
#pragma once

#include "phoenix/Core/Result.h"
#include "phoenix/Core/FixedString.h"
#include <cstdint>
#include <cmath>

namespace phoenix {

// ─── 5PL Parameters ───────────────────────────────────────────────────
// y = D + (A - D) / (1 + (x/C)^B)^E
//
// A = minimum asymptote (background)
// B = Hill slope
// C = inflection point (EC50)
// D = maximum asymptote
// E = asymmetry factor (1.0 = symmetric = 4PL)
struct FivePLParams {
    float A = 0.0f;     // min asymptote
    float B = 1.0f;     // slope
    float C = 10.0f;    // EC50
    float D = 100.0f;   // max asymptote
    float E = 1.0f;     // asymmetry

    // R² goodness of fit
    float r_squared = 0.0f;

    // Evaluate forward: signal → concentration
    float evaluate(float x) const {
        if (x <= 0.0f) return A;
        float ratio = powf(x / C, B);
        return D + (A - D) / powf(1.0f + ratio, E);
    }

    // Inverse: concentration → signal (needed for calibration)
    float inverse(float y) const {
        if (fabsf(A - D) < 1e-6f) return C;  // degenerate
        float inner = (A - D) / (y - D);
        if (inner <= 0.0f) return 0.0f;
        float powered = powf(inner, 1.0f / E) - 1.0f;
        if (powered <= 0.0f) return 0.0f;
        return C * powf(powered, 1.0f / B);
    }
};

// ─── Calibration Point ────────────────────────────────────────────────
static constexpr size_t MAX_CAL_POINTS = 12;

struct CalibrationPoint {
    float concentration = 0.0f;   // Known concentration
    float signal        = 0.0f;   // Measured signal
    uint8_t replicates  = 1;      // Number of replicates
    float cv            = 0.0f;   // Coefficient of variation
};

// ─── Stored Calibration ───────────────────────────────────────────────
struct CalibrationData {
    static constexpr uint16_t SERIAL_VERSION = 1;
    static constexpr uint32_t MAGIC = 0x43414C31;  // "CAL1"

    uint32_t          magic     = MAGIC;
    uint16_t          version   = SERIAL_VERSION;
    String32          id;
    String64          analyte;
    FivePLParams      params;
    CalibrationPoint  points[MAX_CAL_POINTS] = {};
    uint8_t           num_points = 0;
    uint32_t          created_at = 0;  // Unix timestamp
    uint32_t          expires_at = 0;  // Unix timestamp
    float             lod        = 0.0f;  // Limit of detection
    float             loq        = 0.0f;  // Limit of quantitation
    float             range_low  = 0.0f;
    float             range_high = 0.0f;

    bool isValid() const {
        return magic == MAGIC && version == SERIAL_VERSION && num_points >= 4;
    }

    bool isExpired(uint32_t now) const {
        return expires_at > 0 && now > expires_at;
    }
};

// ─── Calibration Service ──────────────────────────────────────────────
class CalibrationService {
public:
    // Workflow
    Result<void> startCalibrationWorkflow(const char* analyte);
    Result<void> addCalibrationPoint(float concentration, float signal);
    Result<FivePLParams> fitCurve();
    Result<void> validateCalibration();
    Result<void> saveCalibration(const char* id);

    // Retrieve
    Result<CalibrationData> getActiveCalibration() const;
    Result<CalibrationData> loadCalibration(const char* id);

    // Apply to measurement
    Result<float> signalToConcentration(float signal) const;
    Result<float> concentrationToSignal(float conc) const;

    // Status
    bool hasActiveCalibration() const { return active_.isValid(); }
    uint8_t getPointCount() const { return workflow_count_; }

private:
    CalibrationData active_         = {};
    CalibrationPoint workflow_pts_[MAX_CAL_POINTS] = {};
    uint8_t workflow_count_         = 0;
    bool    workflow_active_        = false;

    // Levenberg-Marquardt for 5PL fitting (simplified)
    Result<FivePLParams> levenbergMarquardt(
        const CalibrationPoint* pts,
        uint8_t n,
        uint16_t max_iter = 200);

    float computeResidual(
        const FivePLParams& p,
        const CalibrationPoint* pts,
        uint8_t n);
};

} // namespace phoenix
