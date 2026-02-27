// ==========================================================================
// FILE: src/Analysis/MeasurementPipeline.cpp
// Full measurement pipeline implementation
// ==========================================================================

#include "phoenix/Analysis/MeasurementPipeline.h"
#include "phoenix/Services/CalibrationService.h"
#include "phoenix/Services/BenchmarkValidator.h"
#include <esp_log.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <cstring>
#include <cmath>

namespace phoenix {

static const char* TAG = "Pipeline";

MeasurementPipeline::MeasurementPipeline(
    ICameraController*  camera,
    ILEDController*     led,
    IProfileExtractor*  extractor,
    IBaselineEstimator* baseline,
    IPeakFinder*        peaks,
    CalibrationService* calibration,
    BenchmarkValidator* benchmark)
    : camera_(camera)
    , led_(led)
    , extractor_(extractor)
    , baseline_(baseline)
    , peaks_(peaks)
    , calibration_(calibration)
    , benchmark_(benchmark)
{}

// ─── Main Entry Point ─────────────────────────────────────────────────
Result<MeasurementResult> MeasurementPipeline::runMeasurement(
    const PipelineConfig& config)
{
    cancel_flag_ = false;
    MeasurementResult result = {};
    result.timestamp = static_cast<uint32_t>(
        esp_timer_get_time() / 1000000ULL);

    // ── Step 1: Pre-flight validation ──────────────────────────────
    reportProgress(PipelineState::VALIDATING, 5, "Validating system...");
    auto pf = preflight(config);
    if (pf.is_err()) {
        state_ = PipelineState::ERROR;
        return Err<MeasurementResult>(pf.error());
    }
    if (isCancelled()) return Err<MeasurementResult>(
        ErrorCategory::BUSY, "Measurement cancelled");

    // ── Step 2: LED warmup ─────────────────────────────────────────
    reportProgress(PipelineState::LED_WARMUP, 10, "LED stabilizing...");
    LEDConfig led_cfg = {};
    led_cfg.mode      = LEDMode::WHITE;
    led_cfg.intensity = config.led_intensity;
    auto led_res = led_->setMode(led_cfg);
    if (led_res.is_err()) {
        led_->off();
        return Err<MeasurementResult>(led_res.error());
    }
    vTaskDelay(pdMS_TO_TICKS(config.stabilize_ms));
    if (isCancelled()) { led_->off(); return Err<MeasurementResult>(
        ErrorCategory::BUSY, "Measurement cancelled"); }

    // ── Step 3: Image capture with averaging ───────────────────────
    reportProgress(PipelineState::CAPTURING, 25, "Capturing image...");
    auto img_res = captureWithAveraging(config);
    led_->off();  // LED off immediately after capture
    if (img_res.is_err()) {
        return Err<MeasurementResult>(img_res.error());
    }
    last_image_ = img_res.value();
    if (isCancelled()) return Err<MeasurementResult>(
        ErrorCategory::BUSY, "Measurement cancelled");

    // ── Step 4: ROI detection ──────────────────────────────────────
    reportProgress(PipelineState::EXTRACTING, 40, "Detecting strip...");
    ROI roi = config.manual_roi;
    if (config.auto_roi) {
        auto roi_res = detectROI(last_image_);
        if (roi_res.is_ok()) {
            roi = roi_res.value();
        } else {
            ESP_LOGW(TAG, "Auto-ROI failed, using manual: %s",
                     roi_res.error().message);
        }
    }

    // ── Step 5: Profile extraction ─────────────────────────────────
    reportProgress(PipelineState::EXTRACTING, 50, "Extracting profile...");
    auto profile_res = extractor_->extractProfile(last_image_, roi);
    if (profile_res.is_err()) {
        return Err<MeasurementResult>(profile_res.error());
    }
    Profile1D raw_profile = profile_res.value();

    // ── Step 6: Baseline estimation & subtraction ──────────────────
    reportProgress(PipelineState::BASELINE, 60, "Baseline correction...");
    auto bl_res = baseline_->estimateBaseline(raw_profile);
    if (bl_res.is_err()) {
        return Err<MeasurementResult>(bl_res.error());
    }
    auto corrected_res = baseline_->subtractBaseline(raw_profile, bl_res.value());
    if (corrected_res.is_err()) {
        return Err<MeasurementResult>(corrected_res.error());
    }
    last_profile_ = corrected_res.value();
    if (isCancelled()) return Err<MeasurementResult>(
        ErrorCategory::BUSY, "Measurement cancelled");

    // ── Step 7: Peak detection ─────────────────────────────────────
    reportProgress(PipelineState::PEAK_DETECTION, 70, "Finding peaks...");
    auto peak_res = peaks_->findPeaks(
        last_profile_, config.min_peak_height, config.min_peak_dist);
    if (peak_res.is_err()) {
        return Err<MeasurementResult>(peak_res.error());
    }
    PeakResult peak_data = peak_res.value();

    // Validate control line
    if (peak_data.count == 0) {
        return Err<MeasurementResult>(
            ErrorCategory::MEASUREMENT_ERROR, "No peaks detected");
    }

    // Control line is typically the last peak on the strip
    const Peak& control_peak = peak_data.peaks[peak_data.count - 1];
    if (control_peak.snr < config.min_control_snr) {
        result.qc_flags = result.qc_flags | QCFlag::CONTROL_LINE_WEAK;
        ESP_LOGW(TAG, "Control line weak: SNR=%.1f (min=%.1f)",
                 static_cast<double>(control_peak.snr),
                 static_cast<double>(config.min_control_snr));
    }

    // ── Step 8: Calibration ────────────────────────────────────────
    reportProgress(PipelineState::CALIBRATING, 80, "Applying calibration...");
    result = analyzeProfile(last_profile_, peak_data, config).value_or(result);

    if (calibration_ && calibration_->hasActiveCalibration()) {
        // Test line = first peak, signal = T/C ratio
        if (peak_data.count >= 2) {
            const Peak& test_peak = peak_data.peaks[0];
            result.signal_intensity    = test_peak.area;
            result.control_line_signal = control_peak.area;
            result.test_control_ratio  = test_peak.area / control_peak.area;

            auto conc_res = calibration_->signalToConcentration(
                result.test_control_ratio);
            if (conc_res.is_ok()) {
                result.concentration_ng_ml = conc_res.value();
            } else {
                result.qc_flags = result.qc_flags | QCFlag::OUT_OF_RANGE;
                ESP_LOGW(TAG, "Calibration failed: %s",
                         conc_res.error().message);
            }
        }
    }

    // ── Step 9: QC Validation & Benchmark ──────────────────────────
    reportProgress(PipelineState::QC_VALIDATION, 90, "QC validation...");
    if (benchmark_) {
        auto bm_res = benchmark_->validateMeasurement(
            result, result.analyte_name.c_str());
        if (bm_res.is_err()) {
            ESP_LOGW(TAG, "Benchmark validation: %s",
                     bm_res.error().message);
        }
    }
    validateQC(result);

    // ── Done ───────────────────────────────────────────────────────
    reportProgress(PipelineState::COMPLETE, 100, "Measurement complete");
    ESP_LOGI(TAG, "Result: %.2f %s [%s] QC=0x%02X",
             static_cast<double>(result.concentration_ng_ml),
             result.unit.c_str(),
             result.interpretation.c_str(),
             static_cast<int>(result.qc_flags));

    return Ok(result);
}

void MeasurementPipeline::cancel() {
    cancel_flag_ = true;
    ESP_LOGW(TAG, "Measurement cancel requested");
}

// ─── Pre-flight Checks ────────────────────────────────────────────────
Result<void> MeasurementPipeline::preflight(const PipelineConfig& config) {
    if (!camera_) return Err(ErrorCategory::NOT_INITIALIZED, "No camera");
    if (!led_)    return Err(ErrorCategory::NOT_INITIALIZED, "No LED");
    if (!extractor_ || !baseline_ || !peaks_) {
        return Err(ErrorCategory::NOT_INITIALIZED, "Missing algorithm");
    }

    // Camera self-test
    PHOENIX_TRY(camera_->selfTest());

    // LED self-test
    PHOENIX_TRY(led_->selfTest());

    // Check calibration if required
    if (!config.calibration_id.empty() && calibration_) {
        auto cal = calibration_->loadCalibration(config.calibration_id.c_str());
        if (cal.is_err()) {
            return Err(ErrorCategory::CALIBRATION_ERROR,
                      "Calibration not found");
        }
    }

    return Ok();
}

// ─── Multi-frame Capture with Averaging ───────────────────────────────
Result<ImageBuffer> MeasurementPipeline::captureWithAveraging(
    const PipelineConfig& config)
{
    if (config.num_captures <= 1) {
        return camera_->captureImage();
    }

    // Capture first frame as base
    auto first = camera_->captureImage();
    if (first.is_err()) return first;

    ImageBuffer base = first.value();

    // We accumulate in-place using uint16_t to prevent overflow
    // For simplicity, average the raw bytes
    uint16_t* accum = static_cast<uint16_t*>(
        heap_caps_malloc(base.size * sizeof(uint16_t), MALLOC_CAP_SPIRAM));
    if (!accum) {
        return Err<ImageBuffer>(ErrorCategory::MEMORY_ERROR,
                                "PSRAM alloc failed for averaging");
    }

    // Initialize accumulator from first frame
    for (size_t i = 0; i < base.size; ++i) {
        accum[i] = base.data[i];
    }

    // Capture and accumulate remaining frames
    for (uint8_t f = 1; f < config.num_captures; ++f) {
        vTaskDelay(pdMS_TO_TICKS(50));  // Brief delay between captures
        auto frame = camera_->captureImage();
        if (frame.is_err()) {
            heap_caps_free(accum);
            return frame;
        }
        const ImageBuffer& fb = frame.value();
        for (size_t i = 0; i < base.size && i < fb.size; ++i) {
            accum[i] += fb.data[i];
        }
    }

    // Average back into base buffer
    for (size_t i = 0; i < base.size; ++i) {
        base.data[i] = static_cast<uint8_t>(accum[i] / config.num_captures);
    }
    heap_caps_free(accum);

    return Ok(base);
}

// ─── Auto-detect strip ROI ────────────────────────────────────────────
Result<ROI> MeasurementPipeline::detectROI(const ImageBuffer& image) {
    // Simple algorithm: find brightest horizontal band
    // The LFA strip reflects LED light differently than background
    if (!image.data || image.width == 0 || image.height == 0) {
        return Err<ROI>(ErrorCategory::MEASUREMENT_ERROR, "Invalid image");
    }

    // Compute row sums
    uint16_t best_row = 0;
    uint32_t best_sum = 0;

    for (uint16_t y = image.height / 4; y < image.height * 3 / 4; ++y) {
        uint32_t row_sum = 0;
        for (uint16_t x = image.width / 4; x < image.width * 3 / 4; ++x) {
            row_sum += image.data[y * image.width + x];
        }
        if (row_sum > best_sum) {
            best_sum = row_sum;
            best_row = y;
        }
    }

    // ROI centered on brightest row, spanning most of width
    ROI roi;
    roi.x      = static_cast<uint16_t>(image.width / 8);
    roi.width  = static_cast<uint16_t>(image.width * 6 / 8);
    roi.y      = static_cast<uint16_t>(best_row > 20 ? best_row - 20 : 0);
    roi.height = 40;

    ESP_LOGI(TAG, "Auto-ROI: x=%u y=%u w=%u h=%u",
             roi.x, roi.y, roi.width, roi.height);

    return Ok(roi);
}

// ─── Analyze Profile and Build Result ─────────────────────────────────
Result<MeasurementResult> MeasurementPipeline::analyzeProfile(
    const Profile1D& /* corrected */,
    const PeakResult& peak_data,
    const PipelineConfig& /* config */)
{
    MeasurementResult result = {};
    result.timestamp = static_cast<uint32_t>(
        esp_timer_get_time() / 1000000ULL);

    if (peak_data.count == 0) {
        result.interpretation = "INVALID";
        return Ok(result);
    }

    // For standard LFA: peak[0] = test line, peak[last] = control
    if (peak_data.count >= 2) {
        result.signal_intensity    = peak_data.peaks[0].area;
        result.control_line_signal = peak_data.peaks[peak_data.count - 1].area;
        result.test_control_ratio  = result.signal_intensity /
                                     result.control_line_signal;
    } else {
        // Only control line visible → negative
        result.control_line_signal = peak_data.peaks[0].area;
        result.test_control_ratio  = 0.0f;
        result.interpretation = "NEGATIVE";
    }

    result.confidence = fminf(1.0f,
        peak_data.peaks[0].snr / 20.0f);  // Normalize SNR to confidence

    return Ok(result);
}

// ─── QC Validation ────────────────────────────────────────────────────
Result<void> MeasurementPipeline::validateQC(MeasurementResult& result) {
    // Check background level
    if (last_profile_.length > 0) {
        float bg_mean = 0.0f;
        uint16_t bg_count = 0;
        // Sample first 10% as background
        uint16_t bg_end = last_profile_.length / 10;
        for (uint16_t i = 0; i < bg_end; ++i) {
            bg_mean += last_profile_.values[i];
            bg_count++;
        }
        if (bg_count > 0) {
            bg_mean /= static_cast<float>(bg_count);
            if (bg_mean > 0.2f) {
                result.qc_flags = result.qc_flags | QCFlag::HIGH_BACKGROUND;
            }
        }
    }

    return Ok();
}

// ─── Progress Reporting ───────────────────────────────────────────────
void MeasurementPipeline::reportProgress(
    PipelineState state, uint8_t pct, const char* msg)
{
    state_ = state;
    if (progress_cb_) {
        PipelineProgress p;
        p.state      = state;
        p.percentage = pct;
        p.message    = msg;
        progress_cb_(p, progress_ctx_);
    }
    ESP_LOGD(TAG, "[%u%%] %s", pct, msg);
}

} // namespace phoenix
