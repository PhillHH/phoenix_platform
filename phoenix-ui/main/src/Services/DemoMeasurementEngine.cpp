// ==========================================================================
// FILE: src/Services/DemoMeasurementEngine.cpp
// Phoenix v108.0 — Demo measurement engine with realistic simulation
// Generates plausible results for all 4 technologies
// Auto-switches to live mode when Measurement MCU responds on UART
// ==========================================================================
#include "phoenix/Core/AssayTechnology.h"
#include "phoenix/Core/Result.h"
#include <esp_log.h>
#include <esp_random.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <cmath>

namespace phoenix {

static const char* TAG = "DemoEngine";

// ── Operating Mode ────────────────────────────────────────────────────
enum class EngineMode { DEMO, LIVE };
static EngineMode s_mode = EngineMode::DEMO;

bool isDemoMode() { return s_mode == EngineMode::DEMO; }
void setLiveMode() { s_mode = EngineMode::LIVE; ESP_LOGI(TAG, "Switched to LIVE mode"); }

// ── Random helpers ────────────────────────────────────────────────────
static float randFloat(float lo, float hi) {
    uint32_t r = esp_random();
    float norm = static_cast<float>(r) / static_cast<float>(UINT32_MAX);
    return lo + norm * (hi - lo);
}

static float gaussRand(float mean, float stddev) {
    // Box-Muller transform
    float u1 = randFloat(0.001f, 0.999f);
    float u2 = randFloat(0.001f, 0.999f);
    float z = sqrtf(-2.0f * logf(u1)) * cosf(2.0f * 3.14159265f * u2);
    return mean + stddev * z;
}

// ── Measurement Pipeline Stages ───────────────────────────────────────
struct PipelineStage {
    const char* name;
    const char* name_de;
    uint8_t     pct_start;
    uint8_t     pct_end;
    uint16_t    duration_ms;
};

// Technology-specific pipeline definitions
static const PipelineStage STAGES_COLORIMETRIC[] = {
    {"Cassette check",    "Kassetten-Check",     0,   8,   800},
    {"LED warmup",        "LED Aufwärmen",        8,  15,   600},
    {"Pre-scan",          "Vorscan",             15,  22,   500},
    {"Incubation wait",   "Inkubation",          22,  45,  3000},
    {"Image capture",     "Bildaufnahme",        45,  55,   800},
    {"Profile extraction","Profilerfassung",     55,  65,   600},
    {"Baseline correction","Basislinie",         65,  75,   400},
    {"Peak detection",    "Peak-Erkennung",      75,  82,   500},
    {"Quantification",    "Quantifizierung",     82,  90,   600},
    {"QC validation",     "QC-Validierung",      90, 100,   400},
};

static const PipelineStage STAGES_FLUORESCENCE[] = {
    {"Cassette check",    "Kassetten-Check",      0,   6,   800},
    {"UV LED calibration","UV-LED Kalibrierung",  6,  14,  1200},
    {"Dark reference",    "Dunkelreferenz",      14,  20,   600},
    {"UV excitation",     "UV-Anregung",         20,  35,  2000},
    {"Emission capture",  "Emissionsaufnahme",   35,  50,  1500},
    {"Background sub",    "Hintergrundkorrektur",50,  60,   800},
    {"Signal integration","Signalintegration",   60,  72,   600},
    {"Curve fitting",     "Kurvenanpassung",     72,  82,   800},
    {"Concentration calc","Konzentrationsberechn.",82, 92,   600},
    {"QC validation",     "QC-Validierung",      92, 100,   500},
};

static const PipelineStage STAGES_DRY_CHEMISTRY[] = {
    {"Pad recognition",   "Pad-Erkennung",        0,  10,   600},
    {"Reference scan",    "Referenzscan",         10,  20,   500},
    {"Reagent activation","Reagenzaktivierung",   20,  40,  2000},
    {"Reflectance scan",  "Reflexionsmessung",    40,  60,  1200},
    {"Multi-pad analysis","Multi-Pad-Analyse",    60,  75,   800},
    {"Color compensation","Farbkompensation",     75,  85,   500},
    {"Calculation",       "Berechnung",           85,  95,   400},
    {"QC validation",     "QC-Validierung",       95, 100,   300},
};

static const PipelineStage STAGES_MICROFLUIDICS[] = {
    {"Chip recognition",  "Chip-Erkennung",        0,   8,   600},
    {"Channel priming",   "Kanal-Priming",         8,  18,  1500},
    {"Sample loading",    "Probenladung",          18,  28,  1200},
    {"Mixing phase",      "Mischphase",            28,  42,  2000},
    {"Separation",        "Separation",            42,  58,  2500},
    {"Detection window",  "Detektionsfenster",     58,  72,  1500},
    {"Signal processing", "Signalverarbeitung",    72,  82,   800},
    {"Multi-analyte calc","Multi-Analyt-Berechn.", 82,  92,   600},
    {"QC validation",     "QC-Validierung",        92, 100,   500},
};

struct PipelineConfig {
    const PipelineStage* stages;
    size_t               count;
};

static PipelineConfig getPipeline(Technology tech) {
    switch (tech) {
        case Technology::COLORIMETRIC:
            return {STAGES_COLORIMETRIC,
                    sizeof(STAGES_COLORIMETRIC)/sizeof(STAGES_COLORIMETRIC[0])};
        case Technology::IMMUNOFLUORESCENCE:
            return {STAGES_FLUORESCENCE,
                    sizeof(STAGES_FLUORESCENCE)/sizeof(STAGES_FLUORESCENCE[0])};
        case Technology::DRY_CHEMISTRY:
            return {STAGES_DRY_CHEMISTRY,
                    sizeof(STAGES_DRY_CHEMISTRY)/sizeof(STAGES_DRY_CHEMISTRY[0])};
        case Technology::MICROFLUIDICS:
            return {STAGES_MICROFLUIDICS,
                    sizeof(STAGES_MICROFLUIDICS)/sizeof(STAGES_MICROFLUIDICS[0])};
    }
    return {STAGES_COLORIMETRIC, 0};
}

// ── Generate Realistic Demo Result ────────────────────────────────────
MeasurementResult generateDemoResult(const AssayDefinition& assay) {
    MeasurementResult r = {};

    // Generate concentration near normal range (70% normal, 20% borderline, 10% abnormal)
    float dice = randFloat(0, 1);
    float mid = (assay.range_low + assay.range_high) / 2.0f;
    float span = assay.range_high - assay.range_low;

    if (dice < 0.70f) {
        // Normal range
        r.concentration_ng_ml = gaussRand(mid, span * 0.15f);
    } else if (dice < 0.90f) {
        // Borderline (near cutoff)
        r.concentration_ng_ml = gaussRand(assay.cutoff_positive, span * 0.1f);
    } else {
        // Abnormal (clearly out of range)
        float direction = (esp_random() & 1) ? 1.5f : -0.3f;
        r.concentration_ng_ml = mid + direction * span;
    }

    // Clamp to physical limits
    if (r.concentration_ng_ml < 0) r.concentration_ng_ml = 0;

    // Signal characteristics
    r.signal_intensity = randFloat(0.2f, 0.95f);
    r.control_line_signal = randFloat(0.7f, 1.0f);
    r.test_control_ratio = r.signal_intensity / r.control_line_signal;
    r.reference_low = assay.range_low;
    r.reference_high = assay.range_high;
    r.confidence = randFloat(0.92f, 0.99f);
    r.qc_flags = QCFlag::NONE;

    // Set analyte info
    r.analyte_name = assay.name;
    r.unit = assay.unit;

    // Interpretation
    if (assay.quantitative) {
        if (r.concentration_ng_ml < assay.range_low) {
            r.interpretation = "LOW";
        } else if (r.concentration_ng_ml > assay.range_high) {
            r.interpretation = "HIGH";
        } else {
            r.interpretation = "NORMAL";
        }
    } else {
        r.interpretation =
            (r.concentration_ng_ml >= assay.cutoff_positive) ? "POSITIVE" : "NEGATIVE";
    }

    // Occasional QC warnings (5% chance each)
    if (randFloat(0,1) < 0.05f)
        r.qc_flags = r.qc_flags | QCFlag::CONTROL_LINE_WEAK;
    if (randFloat(0,1) < 0.03f)
        r.qc_flags = r.qc_flags | QCFlag::HIGH_BACKGROUND;

    ESP_LOGI(TAG, "Demo result: %s = %.2f %s [%s] conf=%.0f%%",
             assay.name.c_str(),
             static_cast<double>(r.concentration_ng_ml),
             assay.unit.c_str(),
             r.interpretation.c_str(),
             static_cast<double>(r.confidence * 100));

    return r;
}

// ── Callback types ────────────────────────────────────────────────────
using StageCallback = void(*)(uint8_t stage_idx, uint8_t pct,
                               const char* stage_name, void* ctx);
using ResultCallback = void(*)(const MeasurementResult& result, void* ctx);

// ── Run Demo Measurement (blocking, call from task) ───────────────────
void runDemoMeasurement(const AssayDefinition& assay,
                         StageCallback on_stage,
                         ResultCallback on_result,
                         volatile bool* cancel_flag,
                         void* ctx)
{
    ESP_LOGI(TAG, "Starting demo measurement: %s (%s)",
             assay.name.c_str(), getTechInfo(assay.tech).name);

    PipelineConfig pipe = getPipeline(assay.tech);

    for (size_t i = 0; i < pipe.count; i++) {
        if (cancel_flag && *cancel_flag) {
            ESP_LOGW(TAG, "Measurement cancelled at stage %d", (int)i);
            return;
        }

        const auto& stage = pipe.stages[i];
        if (on_stage) on_stage(i, stage.pct_start, stage.name, ctx);

        // Simulate stage duration with progress updates
        uint16_t elapsed = 0;
        uint16_t step_ms = 100;
        while (elapsed < stage.duration_ms) {
            if (cancel_flag && *cancel_flag) return;
            vTaskDelay(pdMS_TO_TICKS(step_ms));
            elapsed += step_ms;

            // Interpolate progress within stage
            float frac = static_cast<float>(elapsed) / stage.duration_ms;
            uint8_t pct = stage.pct_start +
                static_cast<uint8_t>(frac * (stage.pct_end - stage.pct_start));
            if (on_stage) on_stage(i, pct, stage.name, ctx);
        }
    }

    // Generate result
    MeasurementResult result = generateDemoResult(assay);
    if (on_result) on_result(result, ctx);
}

} // namespace phoenix
