// ==========================================================================
// FILE: include/phoenix/Services/BenchmarkValidator.h
// Benchmark validation against clinical reference ranges
// REQ-BENCH-001: System SHALL validate results against known benchmarks
// ==========================================================================
#pragma once

#include "phoenix/Core/Result.h"
#include "phoenix/Core/FixedString.h"
#include "phoenix/Services/CalibrationService.h"
#include "phoenix/Analysis/Interfaces.h"
#include <cstdint>

namespace phoenix {

// ─── Clinical Reference Range ─────────────────────────────────────────
struct ReferenceRange {
    float    low          = 0.0f;
    float    high         = 0.0f;
    String32 unit;
    String64 population;          // "Adult", "Pediatric", "Canine"
    String64 interpretation_low;  // "NEGATIVE", "NORMAL"
    String64 interpretation_mid;  // "BORDERLINE", "ELEVATED"
    String64 interpretation_high; // "POSITIVE", "CRITICAL"
};

// ─── Benchmark Data ───────────────────────────────────────────────────
struct BenchmarkData {
    String32       analyte;
    String32       method;        // "LFA", "ELISA", "CLIA"
    ReferenceRange reference;
    FivePLParams   expected_curve;  // Manufacturer reference curve
    float          cv_threshold = 15.0f;  // Max acceptable CV%
    float          bias_threshold = 10.0f; // Max acceptable bias%
};

// ─── Benchmark Validator ──────────────────────────────────────────────
class BenchmarkValidator {
public:
    // Validate a measurement result against benchmark
    Result<void> validateMeasurement(
        MeasurementResult& result,
        const char* analyte);

    // Validate a calibration curve against expected
    Result<void> validateCalibration(
        const FivePLParams& measured,
        const char* analyte);

    // Get clinical interpretation
    Result<String64> getClinicalInterpretation(
        float concentration,
        const char* analyte);

    // Get reference range for analyte
    Result<ReferenceRange> getReferenceRange(const char* analyte);

    // Register a benchmark (loaded from NVS or RFID)
    Result<void> registerBenchmark(const BenchmarkData& data);

    // List available benchmarks
    uint8_t getAvailableCount() const { return benchmark_count_; }

private:
    static constexpr size_t MAX_BENCHMARKS = 32;

    BenchmarkData benchmarks_[MAX_BENCHMARKS] = {};
    uint8_t       benchmark_count_ = 0;

    const BenchmarkData* findBenchmark(const char* analyte) const;
};

} // namespace phoenix
