// ==========================================================================
// FILE: src/Services/BenchmarkValidator.cpp
// Clinical benchmark validation
// ==========================================================================

#include "phoenix/Services/BenchmarkValidator.h"
#include "phoenix/Services/CalibrationService.h"
#include <esp_log.h>
#include <cstring>
#include <cmath>

namespace phoenix {

static const char* TAG = "Benchmark";

Result<void> BenchmarkValidator::validateMeasurement(
    MeasurementResult& result, const char* analyte)
{
    const BenchmarkData* bm = findBenchmark(analyte);
    if (!bm) {
        return Err(ErrorCategory::NOT_FOUND, "No benchmark for analyte");
    }

    // Set reference range on result
    result.reference_low  = bm->reference.low;
    result.reference_high = bm->reference.high;
    result.unit           = bm->reference.unit;
    result.analyte_name   = analyte;

    // Clinical interpretation
    float conc = result.concentration_ng_ml;
    if (conc < bm->reference.low) {
        result.interpretation = bm->reference.interpretation_low;
    } else if (conc > bm->reference.high) {
        result.interpretation = bm->reference.interpretation_high;
    } else {
        result.interpretation = bm->reference.interpretation_mid;
    }

    ESP_LOGI(TAG, "%s: %.2f %s → %s [ref: %.1f-%.1f]",
             analyte,
             static_cast<double>(conc),
             result.unit.c_str(),
             result.interpretation.c_str(),
             static_cast<double>(bm->reference.low),
             static_cast<double>(bm->reference.high));

    return Ok();
}

Result<void> BenchmarkValidator::validateCalibration(
    const FivePLParams& measured, const char* analyte)
{
    const BenchmarkData* bm = findBenchmark(analyte);
    if (!bm) {
        return Err(ErrorCategory::NOT_FOUND, "No benchmark for analyte");
    }

    // Compare EC50 (C parameter)
    float ec50_diff = fabsf(measured.C - bm->expected_curve.C);
    float ec50_pct  = (bm->expected_curve.C > 0.0f)
        ? (ec50_diff / bm->expected_curve.C) * 100.0f : 0.0f;

    if (ec50_pct > bm->bias_threshold) {
        ESP_LOGW(TAG, "EC50 bias %.1f%% exceeds threshold %.1f%%",
                 static_cast<double>(ec50_pct),
                 static_cast<double>(bm->bias_threshold));
        return Err(ErrorCategory::CALIBRATION_ERROR, "EC50 bias too high");
    }

    ESP_LOGI(TAG, "Calibration validated: EC50 bias=%.1f%%",
             static_cast<double>(ec50_pct));
    return Ok();
}

Result<String64> BenchmarkValidator::getClinicalInterpretation(
    float concentration, const char* analyte)
{
    const BenchmarkData* bm = findBenchmark(analyte);
    if (!bm) {
        return Err<String64>(ErrorCategory::NOT_FOUND, "No benchmark");
    }

    String64 interp;
    if (concentration < bm->reference.low) {
        interp = bm->reference.interpretation_low;
    } else if (concentration > bm->reference.high) {
        interp = bm->reference.interpretation_high;
    } else {
        interp = bm->reference.interpretation_mid;
    }
    return Ok(interp);
}

Result<ReferenceRange> BenchmarkValidator::getReferenceRange(const char* analyte) {
    const BenchmarkData* bm = findBenchmark(analyte);
    if (!bm) {
        return Err<ReferenceRange>(ErrorCategory::NOT_FOUND, "No benchmark");
    }
    return Ok(bm->reference);
}

Result<void> BenchmarkValidator::registerBenchmark(const BenchmarkData& data) {
    if (benchmark_count_ >= MAX_BENCHMARKS) {
        return Err(ErrorCategory::MEMORY_ERROR, "Benchmark table full");
    }

    // Check for existing entry with same analyte — update it
    for (uint8_t i = 0; i < benchmark_count_; ++i) {
        if (strcmp(benchmarks_[i].analyte.c_str(), data.analyte.c_str()) == 0) {
            benchmarks_[i] = data;
            ESP_LOGI(TAG, "Updated benchmark: %s", data.analyte.c_str());
            return Ok();
        }
    }

    benchmarks_[benchmark_count_++] = data;
    ESP_LOGI(TAG, "Registered benchmark: %s (total=%u)",
             data.analyte.c_str(), benchmark_count_);
    return Ok();
}

const BenchmarkData* BenchmarkValidator::findBenchmark(const char* analyte) const {
    for (uint8_t i = 0; i < benchmark_count_; ++i) {
        if (strcmp(benchmarks_[i].analyte.c_str(), analyte) == 0) {
            return &benchmarks_[i];
        }
    }
    return nullptr;
}

} // namespace phoenix
