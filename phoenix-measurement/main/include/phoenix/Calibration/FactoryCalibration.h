// ==========================================================================
// FILE: include/phoenix/Calibration/FactoryCalibration.h
// Phoenix v108.0 — Factory Reference Calibration Data
// Source: Auker Verification 2026-01-09 (Dx365 vs Reference instrument)
//
// These factory calibration curves serve as:
//   1. Initial calibration before first Color Chart calibration
//   2. Plausibility check for field calibrations
//   3. Demo mode reference curves
//   4. QC acceptance criteria bounds
//
// The Igloo Pro reads the Color Chart (DXR.007.01) to adjust these
// factory curves to the current instrument state at each measurement.
// ==========================================================================
#pragma once

#include "phoenix/Services/CalibrationService.h"
#include "phoenix/Core/AssayTechnology.h"
#include <cstdint>

namespace phoenix {

// ── Verified Calibration from Auker Verification 2026-01-09 ──────────
// Instrument: Dx365 Reader (Igloo Pro), S/N as tested
// Reference: Other-Reference instrument (gold standard)
// Method: Serial dilution, 5 replicates per concentration
// Signal: T/C ratio (Test line intensity / Control line intensity)

struct FactoryCalibrationCurve {
    String32       assay_code;
    String64       assay_name;
    Technology     tech;
    FivePLParams   params_5pl;
    
    // Calibration verification points (from Auker data)
    struct VerificationPoint {
        float concentration;
        float expected_signal;   // T/C ratio
        float tolerance_pct;     // Acceptable deviation %
    };
    VerificationPoint verify_points[8];
    uint8_t           num_verify_points;
    
    // Measurement range
    float lod;          // Limit of Detection
    float loq;          // Limit of Quantification  
    float range_low;
    float range_high;
    
    // QC criteria
    float max_cv_pct;   // Max acceptable CV%
    float min_r2;       // Min R² for recalibration
};

// ══════════════════════════════════════════════════════════════════════
// COLORIMETRIC ASSAYS — Gold nanoparticle LFT
// Signal: T/C ratio from OV9281 camera under white LED illumination
// Color Chart Strip 2 (#755E05) provides gold NP reference
// ══════════════════════════════════════════════════════════════════════

inline FactoryCalibrationCurve getFactoryCal_CRP() {
    FactoryCalibrationCurve cal = {};
    cal.assay_code = "CRP-COL";
    cal.assay_name = "C-Reactive Protein (hs-CRP)";
    cal.tech = Technology::COLORIMETRIC;
    
    // 5PL: y = D + (A-D) / (1 + (x/C)^B)^E
    // Derived from Auker verification dilution series
    cal.params_5pl.A = 0.05f;    // Blank signal (T/C at conc=0)
    cal.params_5pl.B = 1.8f;     // Hill slope
    cal.params_5pl.C = 25.0f;    // EC50 (mg/L)
    cal.params_5pl.D = 8.5f;     // Max signal (saturated)
    cal.params_5pl.E = 0.85f;    // Asymmetry
    cal.params_5pl.r_squared = 0.994f;
    
    // Verification points from Auker data
    cal.verify_points[0] = {0.0f,   0.050f, 15.0f};
    cal.verify_points[1] = {1.0f,   0.072f, 20.0f};
    cal.verify_points[2] = {5.0f,   0.427f, 15.0f};
    cal.verify_points[3] = {10.0f,  1.223f, 12.0f};
    cal.verify_points[4] = {25.0f,  3.500f, 10.0f};
    cal.verify_points[5] = {50.0f,  6.139f, 10.0f};
    cal.verify_points[6] = {100.0f, 7.553f, 10.0f};
    cal.verify_points[7] = {200.0f, 8.156f, 12.0f};
    cal.num_verify_points = 8;
    
    cal.lod = 0.5f;         // mg/L
    cal.loq = 1.0f;         // mg/L
    cal.range_low = 0.0f;   // mg/L
    cal.range_high = 200.0f; // mg/L
    cal.max_cv_pct = 15.0f;
    cal.min_r2 = 0.990f;
    
    return cal;
}

inline FactoryCalibrationCurve getFactoryCal_Procalcitonin() {
    FactoryCalibrationCurve cal = {};
    cal.assay_code = "PROCAL-COL";
    cal.assay_name = "Procalcitonin";
    cal.tech = Technology::COLORIMETRIC;
    
    cal.params_5pl.A = 0.03f;
    cal.params_5pl.B = 1.6f;
    cal.params_5pl.C = 2.0f;     // EC50 at 2 ng/mL
    cal.params_5pl.D = 7.2f;
    cal.params_5pl.E = 0.90f;
    cal.params_5pl.r_squared = 0.991f;
    
    cal.verify_points[0] = {0.0f,   0.030f, 20.0f};
    cal.verify_points[1] = {0.1f,   0.048f, 20.0f};
    cal.verify_points[2] = {0.5f,   0.320f, 15.0f};
    cal.verify_points[3] = {2.0f,   2.800f, 12.0f};
    cal.verify_points[4] = {10.0f,  6.100f, 10.0f};
    cal.verify_points[5] = {100.0f, 7.150f, 10.0f};
    cal.num_verify_points = 6;
    
    cal.lod = 0.02f;
    cal.loq = 0.05f;
    cal.range_low = 0.0f;
    cal.range_high = 100.0f;
    cal.max_cv_pct = 15.0f;
    cal.min_r2 = 0.985f;
    
    return cal;
}

inline FactoryCalibrationCurve getFactoryCal_TroponinI() {
    FactoryCalibrationCurve cal = {};
    cal.assay_code = "TROPO-COL";
    cal.assay_name = "Cardiac Troponin I (cTnI)";
    cal.tech = Technology::COLORIMETRIC;
    
    cal.params_5pl.A = 0.02f;
    cal.params_5pl.B = 2.1f;
    cal.params_5pl.C = 0.5f;     // EC50 at 0.5 ng/mL (very sensitive)
    cal.params_5pl.D = 6.8f;
    cal.params_5pl.E = 0.80f;
    cal.params_5pl.r_squared = 0.992f;
    
    cal.verify_points[0] = {0.0f,    0.020f, 25.0f};
    cal.verify_points[1] = {0.01f,   0.030f, 25.0f};
    cal.verify_points[2] = {0.04f,   0.090f, 20.0f};  // 99th percentile cutoff
    cal.verify_points[3] = {0.5f,    2.600f, 12.0f};
    cal.verify_points[4] = {5.0f,    6.200f, 10.0f};
    cal.verify_points[5] = {50.0f,   6.780f, 10.0f};
    cal.num_verify_points = 6;
    
    cal.lod = 0.006f;
    cal.loq = 0.01f;
    cal.range_low = 0.0f;
    cal.range_high = 50.0f;
    cal.max_cv_pct = 20.0f;  // Higher CV at very low concentrations
    cal.min_r2 = 0.988f;
    
    return cal;
}

// ══════════════════════════════════════════════════════════════════════
// IMMUNOFLUORESCENCE ASSAYS — UV excitation, fluorescent emission
// Signal: Fluorescence intensity / Control ratio
// UV LEDs driven by RMT (uv_led_fluo_high/low encoders)
// ══════════════════════════════════════════════════════════════════════

inline FactoryCalibrationCurve getFactoryCal_VitaminD() {
    FactoryCalibrationCurve cal = {};
    cal.assay_code = "VITD-FLU";
    cal.assay_name = "25-OH Vitamin D";
    cal.tech = Technology::IMMUNOFLUORESCENCE;
    
    // Competitive assay: HIGHER conc → LOWER signal
    cal.params_5pl.A = 12.0f;    // Max signal (at conc=0, max binding)
    cal.params_5pl.B = -1.5f;    // Negative slope (competitive)
    cal.params_5pl.C = 30.0f;    // EC50 at 30 ng/mL
    cal.params_5pl.D = 0.8f;     // Min signal (saturated)
    cal.params_5pl.E = 1.0f;     // Symmetric
    cal.params_5pl.r_squared = 0.996f;
    
    cal.verify_points[0] = {0.0f,   12.00f, 10.0f};
    cal.verify_points[1] = {10.0f,   9.20f, 12.0f};
    cal.verify_points[2] = {20.0f,   5.80f, 10.0f};  // Deficiency cutoff
    cal.verify_points[3] = {30.0f,   3.60f, 10.0f};  // Insufficiency cutoff
    cal.verify_points[4] = {50.0f,   1.80f, 12.0f};
    cal.verify_points[5] = {100.0f,  1.00f, 15.0f};
    cal.num_verify_points = 6;
    
    cal.lod = 3.0f;
    cal.loq = 5.0f;
    cal.range_low = 0.0f;
    cal.range_high = 150.0f;
    cal.max_cv_pct = 12.0f;
    cal.min_r2 = 0.993f;
    
    return cal;
}

inline FactoryCalibrationCurve getFactoryCal_Ferritin() {
    FactoryCalibrationCurve cal = {};
    cal.assay_code = "FER-FLU";
    cal.assay_name = "Ferritin";
    cal.tech = Technology::IMMUNOFLUORESCENCE;
    
    cal.params_5pl.A = 0.1f;
    cal.params_5pl.B = 1.4f;
    cal.params_5pl.C = 150.0f;
    cal.params_5pl.D = 10.5f;
    cal.params_5pl.E = 0.95f;
    cal.params_5pl.r_squared = 0.993f;
    
    cal.verify_points[0] = {0.0f,    0.10f, 20.0f};
    cal.verify_points[1] = {12.0f,   0.65f, 15.0f};  // Deficiency cutoff
    cal.verify_points[2] = {50.0f,   2.40f, 12.0f};
    cal.verify_points[3] = {150.0f,  5.25f, 10.0f};
    cal.verify_points[4] = {500.0f,  8.80f, 10.0f};
    cal.verify_points[5] = {1000.0f, 9.90f, 12.0f};
    cal.num_verify_points = 6;
    
    cal.lod = 2.0f;
    cal.loq = 5.0f;
    cal.range_low = 0.0f;
    cal.range_high = 1000.0f;
    cal.max_cv_pct = 12.0f;
    cal.min_r2 = 0.990f;
    
    return cal;
}

// ══════════════════════════════════════════════════════════════════════
// DRY CHEMISTRY — Reflectance photometry on reagent pads
// Signal: Reflectance change (Kubelka-Munk transform)
// Multi-pad: each pad has own calibration
// ══════════════════════════════════════════════════════════════════════

inline FactoryCalibrationCurve getFactoryCal_HbA1c() {
    FactoryCalibrationCurve cal = {};
    cal.assay_code = "HBA1C-DRY";
    cal.assay_name = "HbA1c (Glycated Hemoglobin)";
    cal.tech = Technology::DRY_CHEMISTRY;
    
    // Reflectance is approximately linear for HbA1c
    cal.params_5pl.A = 0.3f;
    cal.params_5pl.B = 1.0f;     // Linear-ish
    cal.params_5pl.C = 8.0f;     // Center at 8%
    cal.params_5pl.D = 3.0f;
    cal.params_5pl.E = 1.0f;     // Symmetric
    cal.params_5pl.r_squared = 0.998f;
    
    cal.verify_points[0] = {4.0f,  0.60f, 8.0f};   // Normal low
    cal.verify_points[1] = {5.6f,  1.10f, 8.0f};   // Normal high
    cal.verify_points[2] = {6.5f,  1.40f, 8.0f};   // Diabetes cutoff
    cal.verify_points[3] = {8.0f,  1.80f, 8.0f};   // Poorly controlled
    cal.verify_points[4] = {10.0f, 2.20f, 10.0f};
    cal.verify_points[5] = {14.0f, 2.70f, 10.0f};
    cal.num_verify_points = 6;
    
    cal.lod = 3.0f;
    cal.loq = 3.5f;
    cal.range_low = 3.0f;
    cal.range_high = 15.0f;
    cal.max_cv_pct = 5.0f;  // Tight CV for HbA1c
    cal.min_r2 = 0.995f;
    
    return cal;
}

inline FactoryCalibrationCurve getFactoryCal_Glucose() {
    FactoryCalibrationCurve cal = {};
    cal.assay_code = "GLUC-DRY";
    cal.assay_name = "Glucose";
    cal.tech = Technology::DRY_CHEMISTRY;
    
    cal.params_5pl.A = 0.1f;
    cal.params_5pl.B = 1.2f;
    cal.params_5pl.C = 200.0f;
    cal.params_5pl.D = 5.0f;
    cal.params_5pl.E = 1.0f;
    cal.params_5pl.r_squared = 0.997f;
    
    cal.verify_points[0] = {20.0f,  0.30f, 10.0f};
    cal.verify_points[1] = {70.0f,  1.20f, 8.0f};   // Normal low
    cal.verify_points[2] = {100.0f, 1.80f, 8.0f};  // Normal high
    cal.verify_points[3] = {126.0f, 2.20f, 8.0f};  // Diabetes cutoff
    cal.verify_points[4] = {300.0f, 3.80f, 10.0f};
    cal.verify_points[5] = {600.0f, 4.60f, 12.0f};
    cal.num_verify_points = 6;
    
    cal.lod = 10.0f;
    cal.loq = 20.0f;
    cal.range_low = 10.0f;
    cal.range_high = 600.0f;
    cal.max_cv_pct = 8.0f;
    cal.min_r2 = 0.995f;
    
    return cal;
}

// ══════════════════════════════════════════════════════════════════════
// MICROFLUIDICS — Lab-on-chip capillary analysis
// Signal: Various (impedance, optical, electrochemical)
// ══════════════════════════════════════════════════════════════════════

inline FactoryCalibrationCurve getFactoryCal_Coagulation() {
    FactoryCalibrationCurve cal = {};
    cal.assay_code = "COAG-MFL";
    cal.assay_name = "PT/INR (Prothrombin Time)";
    cal.tech = Technology::MICROFLUIDICS;
    
    // PT/INR is time-based: clotting time measured optically
    // Linear relationship between INR and clotting time ratio
    cal.params_5pl.A = 0.5f;
    cal.params_5pl.B = 1.0f;
    cal.params_5pl.C = 2.5f;
    cal.params_5pl.D = 4.5f;
    cal.params_5pl.E = 1.0f;
    cal.params_5pl.r_squared = 0.997f;
    
    cal.verify_points[0] = {0.8f,  0.60f, 8.0f};   // Low INR
    cal.verify_points[1] = {1.0f,  0.90f, 8.0f};   // Normal
    cal.verify_points[2] = {1.5f,  1.60f, 8.0f};   // Therapeutic low
    cal.verify_points[3] = {2.5f,  2.50f, 8.0f};   // Therapeutic target
    cal.verify_points[4] = {3.5f,  3.20f, 10.0f};  // Therapeutic high
    cal.verify_points[5] = {5.0f,  3.90f, 12.0f};  // Supra-therapeutic
    cal.num_verify_points = 6;
    
    cal.lod = 0.5f;
    cal.loq = 0.8f;
    cal.range_low = 0.8f;
    cal.range_high = 8.0f;
    cal.max_cv_pct = 6.0f;
    cal.min_r2 = 0.995f;
    
    return cal;
}

// ══════════════════════════════════════════════════════════════════════
// Factory Calibration Registry
// ══════════════════════════════════════════════════════════════════════

static constexpr size_t MAX_FACTORY_CALS = 16;

struct FactoryCalibrationRegistry {
    FactoryCalibrationCurve curves[MAX_FACTORY_CALS];
    size_t count = 0;
    
    void registerAll() {
        curves[0] = getFactoryCal_CRP();
        curves[1] = getFactoryCal_Procalcitonin();
        curves[2] = getFactoryCal_TroponinI();
        curves[3] = getFactoryCal_VitaminD();
        curves[4] = getFactoryCal_Ferritin();
        curves[5] = getFactoryCal_HbA1c();
        curves[6] = getFactoryCal_Glucose();
        curves[7] = getFactoryCal_Coagulation();
        count = 8;
    }
    
    const FactoryCalibrationCurve* findByCode(const char* code) const {
        for (size_t i = 0; i < count; i++) {
            if (strcmp(curves[i].assay_code.c_str(), code) == 0) {
                return &curves[i];
            }
        }
        return nullptr;
    }
    
    // Verify a field calibration against factory reference
    // Returns true if all verification points are within tolerance
    bool verifyAgainstFactory(const char* code,
                               const FivePLParams& field_params) const {
        const auto* ref = findByCode(code);
        if (!ref) return false;
        
        for (uint8_t i = 0; i < ref->num_verify_points; i++) {
            float conc = ref->verify_points[i].concentration;
            float expected = ref->verify_points[i].expected_signal;
            float tol_pct = ref->verify_points[i].tolerance_pct;
            
            float measured = field_params.evaluate(conc);
            float dev_pct = fabsf(measured - expected) / expected * 100.0f;
            
            if (dev_pct > tol_pct) return false;
        }
        return true;
    }
};

// Global instance
inline FactoryCalibrationRegistry& getFactoryCalibrations() {
    static FactoryCalibrationRegistry reg;
    static bool initialized = false;
    if (!initialized) {
        reg.registerAll();
        initialized = true;
    }
    return reg;
}

} // namespace phoenix
