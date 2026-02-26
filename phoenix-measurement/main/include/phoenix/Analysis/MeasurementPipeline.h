// ==========================================================================
// FILE: include/phoenix/Analysis/MeasurementPipeline.h
// Central measurement orchestrator
// REQ-MEAS-001: System SHALL execute measurement in defined sequence
//
// Pipeline: LED On → Capture → Extract Profile → Baseline → Peaks
//           → Calibrate → QC Validate → Result → LED Off
// ==========================================================================
#pragma once

#include "phoenix/Core/Result.h"
#include "phoenix/Core/FixedString.h"
#include "phoenix/HAL/Interfaces.h"
#include "phoenix/Analysis/Interfaces.h"
#include <cstdint>

namespace phoenix {

// Forward declarations
class CalibrationService;
class BenchmarkValidator;

// ─── Pipeline Configuration ───────────────────────────────────────────
struct PipelineConfig {
    // Image capture
    uint8_t  num_captures     = 3;       // Average N frames for noise reduction
    uint16_t stabilize_ms     = 500;     // LED warmup time before capture
    uint8_t  led_intensity    = 200;     // White LED intensity

    // ROI detection
    bool     auto_roi         = true;    // Auto-detect strip position
    ROI      manual_roi       = {};      // Fallback ROI if auto fails

    // Analysis parameters
    float    min_peak_height  = 0.05f;   // Minimum peak height (AU)
    float    min_peak_dist    = 0.5f;    // Minimum peak distance (mm)
    float    min_control_snr  = 5.0f;    // Minimum control line SNR

    // Calibration
    String32 calibration_id;              // Which calibration to apply
};

// ─── Pipeline State (for progress reporting) ──────────────────────────
enum class PipelineState : uint8_t {
    IDLE              = 0,
    VALIDATING        = 1,   // Pre-flight checks
    LED_WARMUP        = 2,   // LED stabilization
    CAPTURING         = 3,   // Image acquisition
    EXTRACTING        = 4,   // Profile extraction
    BASELINE          = 5,   // Baseline estimation
    PEAK_DETECTION    = 6,   // Finding peaks
    CALIBRATING       = 7,   // Applying calibration curve
    QC_VALIDATION     = 8,   // Quality control checks
    COMPLETE          = 9,
    ERROR             = 10,
};

struct PipelineProgress {
    PipelineState state       = PipelineState::IDLE;
    uint8_t       percentage  = 0;
    String128     message;
};

// ─── Pipeline Callbacks ───────────────────────────────────────────────
using ProgressCallback = void(*)(const PipelineProgress& progress, void* ctx);

// ─── Measurement Pipeline ─────────────────────────────────────────────
class MeasurementPipeline {
public:
    MeasurementPipeline(
        ICameraController*  camera,
        ILEDController*     led,
        IProfileExtractor*  extractor,
        IBaselineEstimator* baseline,
        IPeakFinder*        peaks,
        CalibrationService* calibration,
        BenchmarkValidator* benchmark = nullptr);

    ~MeasurementPipeline() = default;

    // Run full measurement pipeline
    Result<MeasurementResult> runMeasurement(const PipelineConfig& config);

    // Cancel running measurement (thread-safe)
    void cancel();

    // Get current state
    PipelineState getState() const { return state_; }

    // Set progress callback
    void setProgressCallback(ProgressCallback cb, void* ctx) {
        progress_cb_  = cb;
        progress_ctx_ = ctx;
    }

    // Get last captured image (for UI preview via UART)
    const ImageBuffer* getLastImage() const { return &last_image_; }

    // Get last extracted profile (for debug)
    const Profile1D* getLastProfile() const { return &last_profile_; }

private:
    // Hardware
    ICameraController*  camera_     = nullptr;
    ILEDController*     led_        = nullptr;

    // Algorithm strategies
    IProfileExtractor*  extractor_  = nullptr;
    IBaselineEstimator* baseline_   = nullptr;
    IPeakFinder*        peaks_      = nullptr;

    // Services
    CalibrationService* calibration_ = nullptr;
    BenchmarkValidator* benchmark_   = nullptr;

    // State
    PipelineState    state_         = PipelineState::IDLE;
    volatile bool    cancel_flag_   = false;
    ProgressCallback progress_cb_   = nullptr;
    void*            progress_ctx_  = nullptr;

    // Cached results
    ImageBuffer last_image_   = {};
    Profile1D   last_profile_ = {};

    // Pipeline steps
    Result<void>        preflight(const PipelineConfig& config);
    Result<ImageBuffer> captureWithAveraging(const PipelineConfig& config);
    Result<ROI>         detectROI(const ImageBuffer& image);
    Result<MeasurementResult> analyzeProfile(
        const Profile1D& corrected,
        const PeakResult& peaks,
        const PipelineConfig& config);
    Result<void>        validateQC(MeasurementResult& result);

    void reportProgress(PipelineState state, uint8_t pct, const char* msg);
    bool isCancelled() const { return cancel_flag_; }
};

} // namespace phoenix
