// ==========================================================================
// FILE: include/phoenix/Calibration/VerifiedAssayRegistry.h
// Phoenix v108.0 — Complete Assay Registry from Real Dx365 MCP Data
//
// Sources:
//   - MCP/Untitled-1: IgE (verified against 44 measurements)
//   - FERRDENEME2: Ferritin (48 measurements)
//   - Untitled-2: CRP + MxA (53 measurements)
//   - Getein: NT-proBNP, cPL, TSH, T4, fPL (calibration sets)
//   - Projects/GC704: Drug panel (Cocaine, Opiates, Meth, etc.)
//   - Orient-SAA: Serum Amyloid A (fitted from 42 clinical measurements)
//
// All 5PL coefficients extracted from real Dx365 MCP software projects
// 5PL format: y = D + (A - D) / (1 + (x/C)^B)^G
// ==========================================================================
#pragma once

#include "phoenix/Analysis/Dx365Algorithm.h"
#include <cstdint>

namespace phoenix {

// ══════════════════════════════════════════════════════════════════════
// CASSETTE MANUFACTURERS (from STEP files)
// ══════════════════════════════════════════════════════════════════════

enum class CassetteManufacturer : uint8_t {
    GETEIN      = 0,   // DXR-3D-906-00-Getein
    HIGHTOP     = 1,   // DXR-3D-XXX-01-Hightop
    MAXHEALTH   = 2,   // DXR-3D-XXX-01-Maxhealth
    ISIA        = 3,   // DXR-3D-XXX-01-iSIA
    AUKER       = 4,   // DXR-3D-XXX-01-Auker
    ORIENT      = 5,   // Orient Gene
    CHINA_GENERIC = 6, // DXR-3D-9xx-10-NoneChina01
};

// ══════════════════════════════════════════════════════════════════════
// VERIFIED ASSAY DEFINITIONS
// ══════════════════════════════════════════════════════════════════════

// --- CRP (C-Reactive Protein) — from Untitled-2, 53 measurements ---
inline AssayConfig getAssay_CRP() {
    AssayConfig c = {};
    c.id = "CRP"; c.name = "C-Reactive Protein";
    c.loinc_id = "1988-5"; c.measure_unit_id = 77;
    c.signal_type = SignalType::TL_DIV_CL;
    c.scale_type = ScaleType::LINEAR;
    c.incubation_sec = 600;
    c.test_5pl    = {0.094572f, 1.676573f, 198.669f, 7.097542f, 10.0f};
    c.control_5pl = {27.402681f, -4.853945f, 45.283f, 28.534404f, 10.0f};
    c.div_5pl     = {0.004082f, 1.712576f, 202.330f, 0.258841f, 10.0f};
    return c;
}

// --- MxA (Myxovirus Resistance Protein A) — viral infection marker ---
inline AssayConfig getAssay_MxA() {
    AssayConfig c = {};
    c.id = "MxA"; c.name = "MxA Protein";
    c.loinc_id = "MxA"; c.measure_unit_id = 13;
    c.signal_type = SignalType::TL_DIV_CL;
    c.scale_type = ScaleType::LINEAR;
    c.incubation_sec = 600;
    c.test_5pl    = {136.261853f, -0.430723f, 6899.969f, 0.346906f, 2.904495f};
    c.control_5pl = {27.410861f, -4.212682f, 103.838f, 28.533878f, 10.0f};
    c.div_5pl     = {1.569522f, -0.648165f, 3605.066f, 0.012624f, 1.925355f};
    return c;
}

// --- FERRITIN — from FERRDENEME2, 48 measurements ---
inline AssayConfig getAssay_Ferritin() {
    AssayConfig c = {};
    c.id = "FERR"; c.name = "Ferritin";
    c.loinc_id = "FERRITIN"; c.measure_unit_id = 13;
    c.signal_type = SignalType::TL_ONLY;
    c.scale_type = ScaleType::LINEAR;
    c.incubation_sec = 600;
    c.test_5pl    = {606.414693f, -0.402824f, 44.760f, 1.548589f, 10.0f};
    c.control_5pl = {9.748060f, -9.916224f, 396.158f, 15.536573f, 6.708629f};
    c.div_5pl     = {3722.534919f, -0.273953f, 770.634f, 0.104635f, 10.0f};
    return c;
}

// --- SAA (Serum Amyloid A) — fitted from Orient clinical data, R²=0.994 ---
inline AssayConfig getAssay_SAA() {
    AssayConfig c = {};
    c.id = "SAA"; c.name = "Serum Amyloid A";
    c.loinc_id = "SAA"; c.measure_unit_id = 77; // mg/L
    c.signal_type = SignalType::TL_DIV_CL;
    c.scale_type = ScaleType::LINEAR;
    c.incubation_sec = 600;
    // Fitted from Orient calibration data (42 measurements, R²=0.994)
    c.div_5pl = {0.069260f, 1.957454f, 41.521f, 3.799849f, 0.265768f};
    return c;
}

// --- IgE — from MCP, verified against 44 measurements ---
inline AssayConfig getAssay_IgE() {
    AssayConfig c = {};
    c.id = "IgE"; c.name = "IgE Antibody";
    c.loinc_id = "51651-8"; c.measure_unit_id = 77;
    c.signal_type = SignalType::TL_DIV_CL;
    c.scale_type = ScaleType::LINEAR;
    c.incubation_sec = 600;
    c.test_5pl    = {0.018964f, 1.757542f, 117.834f, 5.826381f, 3.770548f};
    c.control_5pl = {-12581.791869f, -0.019873f, 31697768.0f, 24.033658f, 10.0f};
    c.div_5pl     = {-0.000202f, 1.598344f, 255.486f, 0.283666f, 10.0f};
    c.lines[0] = {"ctrl", true,  33.3f, 44.2f, 0, "ctrl"};
    c.lines[1] = {"tl1",  false, 138.3f, 45.6f, 2, "IgE"};
    c.lines[2] = {"tl2",  false, 247.1f, 43.9f, 3, "IgE"};
    c.num_lines = 3;
    return c;
}

// --- NT-proBNP (Heart failure marker) — from Getein, 32 measurements ---
inline AssayConfig getAssay_NTproBNP() {
    AssayConfig c = {};
    c.id = "NT-proBNP"; c.name = "NT-proBNP";
    c.loinc_id = "27100-7"; c.measure_unit_id = 16;
    c.signal_type = SignalType::TL_ONLY;
    c.scale_type = ScaleType::LINEAR;
    c.incubation_sec = 600;
    c.test_5pl    = {0.820005f, 1.079656f, 182282.028f, 46.457878f, 10.0f};
    c.control_5pl = {174.642547f, 10.0f, 159.977f, 10.213186f, 0.387665f};
    c.div_5pl     = {2.198558f, -9.740198f, 13238.192f, 0.064622f, 0.100f};
    return c;
}

// --- TSH — from Getein cTSH, 25 measurements ---
inline AssayConfig getAssay_TSH() {
    AssayConfig c = {};
    c.id = "TSH"; c.name = "Thyroid Stimulating Hormone";
    c.loinc_id = "3014-8"; c.measure_unit_id = 13;
    c.signal_type = SignalType::TL_DIV_CL;
    c.scale_type = ScaleType::LINEAR;
    c.incubation_sec = 900;
    c.test_5pl    = {1.251216f, 1.118971f, 112221.411f, 298774.764905f, 2.704990f};
    c.control_5pl = {9.975826f, 5.865868f, 0.920f, 9.118923f, 3.007785f};
    c.div_5pl     = {0.119745f, 1.089739f, 131674.580f, 35473.463988f, 2.237675f};
    return c;
}

// --- T4 (Thyroxine) — from Getein, 33 measurements ---
inline AssayConfig getAssay_T4() {
    AssayConfig c = {};
    c.id = "T4"; c.name = "Thyroxine (T4)";
    c.loinc_id = "83120-6"; c.measure_unit_id = 14;
    c.signal_type = SignalType::TL_ONLY;
    c.scale_type = ScaleType::LINEAR;
    c.incubation_sec = 900;
    c.test_5pl    = {25.453179f, 2.430380f, 19.129f, 0.999081f, 0.945521f};
    c.control_5pl = {3.516926f, 6.456974f, 8.637f, 8.572529f, 5.372042f};
    c.div_5pl     = {4.099097f, 1.136356f, 134.287f, 0.165722f, 10.0f};
    return c;
}

// --- cPL (Canine Pancreas Lipase) — from Getein, 36 measurements ---
inline AssayConfig getAssay_cPL() {
    AssayConfig c = {};
    c.id = "cPL"; c.name = "Canine Pancreas Lipase";
    c.loinc_id = "48497-2"; c.measure_unit_id = 13;
    c.signal_type = SignalType::TL_ONLY;
    c.scale_type = ScaleType::LINEAR;
    c.incubation_sec = 600;
    c.test_5pl    = {61.800100f, -6.481130f, 14824.309f, -0.422975f, 0.115484f};
    c.control_5pl = {5.087916f, 6.466108f, 47.855f, 12.605554f, 5.416972f};
    c.div_5pl     = {84.903163f, -0.184366f, 84.414f, -0.013556f, 10.0f};
    return c;
}

// --- fPL (Feline Pancreas Lipase) — from Getein, 32 measurements ---
inline AssayConfig getAssay_fPL() {
    AssayConfig c = {};
    c.id = "fPL"; c.name = "Feline Pancreas Lipase";
    c.loinc_id = "23726-3"; c.measure_unit_id = 13;
    c.signal_type = SignalType::TL_DIV_CL;
    c.scale_type = ScaleType::LINEAR;
    c.incubation_sec = 600;
    c.test_5pl    = {13.916211f, -2.846320f, 18.571f, 0.022580f, 0.486193f};
    c.control_5pl = {8.090615f, 7.917908f, 5.495f, 7.719366f, 5.742978f};
    c.div_5pl     = {1.926967f, -1.840021f, 15.134f, 0.005988f, 0.870945f};
    return c;
}

// ── Drug Panel (from GC704 multi-drug cassette) ──────────────────────

inline AssayConfig getAssay_Cocaine() {
    AssayConfig c = {};
    c.id = "COC"; c.name = "Cocaine";
    c.loinc_id = "3398-5"; c.measure_unit_id = 13;
    c.signal_type = SignalType::TL_ONLY;
    c.scale_type = ScaleType::LINEAR;
    c.test_5pl = {0.179943f, -1.291773f, 0.704f, 7.899494f, 10.0f};
    c.div_5pl  = {1.668072f, 10.0f, 3.814f, 0.038576f, 0.115080f};
    return c;
}

inline AssayConfig getAssay_Opiates() {
    AssayConfig c = {};
    c.id = "OPI"; c.name = "Opiates";
    c.loinc_id = "48961-7"; c.measure_unit_id = 13;
    c.signal_type = SignalType::TL_ONLY;
    c.scale_type = ScaleType::LINEAR;
    c.test_5pl = {0.190343f, -1.736033f, 43.353f, 6.201709f, 0.100f};
    c.div_5pl  = {0.029503f, -1.297210f, 30.395f, 1.316064f, 0.199826f};
    return c;
}

inline AssayConfig getAssay_Methamphetamine() {
    AssayConfig c = {};
    c.id = "METH"; c.name = "Methamphetamine";
    c.loinc_id = "3780-4"; c.measure_unit_id = 13;
    c.signal_type = SignalType::TL_ONLY;
    c.scale_type = ScaleType::LINEAR;
    c.test_5pl = {2.505042f, 0.795623f, 210.430f, 0.315211f, 10.0f};
    c.div_5pl  = {0.533878f, 7.856861f, 5.087f, 0.032589f, 0.100f};
    return c;
}

inline AssayConfig getAssay_Amphetamine() {
    AssayConfig c = {};
    c.id = "AMP"; c.name = "Amphetamine";
    c.loinc_id = "19346-6"; c.measure_unit_id = 13;
    c.signal_type = SignalType::TL_ONLY;
    c.scale_type = ScaleType::LINEAR;
    c.test_5pl = {5.321170f, 1.724176f, 9.014f, 0.333317f, 1.154309f};
    return c;
}

inline AssayConfig getAssay_THC() {
    AssayConfig c = {};
    c.id = "THC"; c.name = "THC (Cannabis)";
    c.loinc_id = "3530-3"; c.measure_unit_id = 13;
    c.signal_type = SignalType::TL_DIV_CL;
    c.scale_type = ScaleType::LINEAR;
    c.test_5pl = {0.455815f, -6.167996f, 9.038f, 1.205055f, 10.0f};
    c.div_5pl  = {0.563043f, 0.344554f, 3011276.545f, -273.811838f, 0.100f};
    return c;
}

inline AssayConfig getAssay_Benzodiazepines() {
    AssayConfig c = {};
    c.id = "BZO"; c.name = "Benzodiazepines";
    c.loinc_id = "9428-4"; c.measure_unit_id = 13;
    c.signal_type = SignalType::TL_DIV_CL;
    c.scale_type = ScaleType::LINEAR;
    c.test_5pl = {3.276724f, 22.656376f, 1.799f, 1.629087f, 0.100f};
    c.div_5pl  = {2.395121f, -4.997755f, 9.083f, 0.688641f, 0.471242f};
    return c;
}

// --- CPV (Canine Parvovirus) — 32 measurements, Vet diagnostic ---
inline AssayConfig getAssay_CPV() {
    AssayConfig c = {};
    c.id = "CPV"; c.name = "Canine Parvovirus Ag";
    c.loinc_id = "23793-3"; c.measure_unit_id = 13;
    c.signal_type = SignalType::TL_ONLY;
    c.scale_type = ScaleType::LINEAR;
    c.incubation_sec = 600;
    c.test_5pl = {209.886553f, -5.178043f, 115.666f, 0.018323f, 0.184906f};
    c.div_5pl  = {3.381441f, -6.578900f, 111.752f, 0.001426f, 0.147953f};
    return c;
}

// --- fSAA (Feline SAA) — 25 measurements, Vet inflammation marker ---
inline AssayConfig getAssay_fSAA() {
    AssayConfig c = {};
    c.id = "fSAA"; c.name = "Feline Serum Amyloid A";
    c.loinc_id = "25585-1"; c.measure_unit_id = 13;
    c.signal_type = SignalType::TL_DIV_CL;
    c.scale_type = ScaleType::LINEAR;
    c.incubation_sec = 300;
    c.test_5pl = {13.393221f, 1.072373f, 346.662f, 356.004137f, 10.0f};
    c.div_5pl  = {3.247532f, -8.897788f, 174.334f, 0.020360f, 0.100f};
    return c;
}

// --- SDMA (Symmetric Dimethylarginine) — 44 meas, kidney biomarker ---
inline AssayConfig getAssay_SDMA() {
    AssayConfig c = {};
    c.id = "SDMA"; c.name = "SDMA (Dimethylarginine)";
    c.loinc_id = "80981-4"; c.measure_unit_id = 13;
    c.signal_type = SignalType::TL_DIV_CL;
    c.scale_type = ScaleType::LINEAR;
    c.incubation_sec = 600;
    c.test_5pl = {2.323052f, -0.649352f, 60.489f, 33.638022f, 3.024731f};
    c.div_5pl  = {-0.046970f, -0.685994f, 152.014f, 0.978479f, 2.221947f};
    return c;
}

// --- Pepsinogen — 64 measurements, GI/gastric diagnostic ---
inline AssayConfig getAssay_Pepsinogen() {
    AssayConfig c = {};
    c.id = "PEP"; c.name = "Pepsinogen";
    c.loinc_id = "2739-1"; c.measure_unit_id = 13;
    c.signal_type = SignalType::TL_DIV_CL;
    c.scale_type = ScaleType::LINEAR;
    c.incubation_sec = 600;
    c.test_5pl = {28.151365f, -1.400219f, 589.751f, 0.352811f, 0.680722f};
    c.div_5pl  = {0.853664f, -1.939964f, 722.232f, 0.009305f, 0.433131f};
    return c;
}

// --- S Antibody — 31 measurements ---
inline AssayConfig getAssay_SAb() {
    AssayConfig c = {};
    c.id = "S-Ab"; c.name = "S Antibody";
    c.loinc_id = "1317-7"; c.measure_unit_id = 13;
    c.signal_type = SignalType::TL_DIV_CL;
    c.scale_type = ScaleType::LINEAR;
    c.test_5pl = {52.288575f, -0.549698f, 2.577f, 1.503141f, 6.478126f};
    c.div_5pl  = {0.021988f, 1.424762f, 19.805f, 0.866011f, 0.224718f};
    return c;
}

// --- Buprenorphine (Drug panel) — 47 measurements ---
inline AssayConfig getAssay_Buprenorphine() {
    AssayConfig c = {};
    c.id = "BUP"; c.name = "Buprenorphine";
    c.loinc_id = "3415-7"; c.measure_unit_id = 13;
    c.signal_type = SignalType::TL_ONLY;
    c.scale_type = ScaleType::LINEAR;
    c.test_5pl = {3.223860f, -59.180473f, 4.690f, 2.794652f, 4.573977f};
    c.div_5pl  = {4.840312f, -3.154201f, 6.162f, 0.639306f, 1.095729f};
    return c;
}

// --- Dengue NS1 Ag — 148 measurements, tropical infectious disease ---
inline AssayConfig getAssay_Dengue() {
    AssayConfig c = {};
    c.id = "DENGUE"; c.name = "Dengue NS1 Ag";
    c.loinc_id = "91064-6"; c.measure_unit_id = 13;
    c.signal_type = SignalType::TL_DIV_CL;
    c.scale_type = ScaleType::LINEAR;
    c.test_5pl = {81.023015f, -0.658090f, 1.831f, -0.363614f, 10.0f};
    c.div_5pl  = {2.171032f, -0.812686f, 1.980f, -0.012668f, 10.0f};
    return c;
}

// --- D-Dimer — 31 measurements, coagulation/thrombosis ---
inline AssayConfig getAssay_DDimer() {
    AssayConfig c = {};
    c.id = "D-DIMER"; c.name = "D-Dimer (DDU)";
    c.loinc_id = "91556-1"; c.measure_unit_id = 13;
    c.signal_type = SignalType::TL_ONLY;
    c.scale_type = ScaleType::LINEAR;
    c.test_5pl = {738.468029f, -0.294764f, 0.133f, -0.798758f, 10.0f};
    c.div_5pl  = {36.753109f, -6.053095f, 168.163f, -0.057817f, 0.146949f};
    return c;
}

// --- HbA1c — 30 measurements, diabetes monitoring ---
inline AssayConfig getAssay_HbA1c() {
    AssayConfig c = {};
    c.id = "HBA1C"; c.name = "HbA1c";
    c.loinc_id = "4548-4"; c.measure_unit_id = 13;
    c.signal_type = SignalType::TL_ONLY;
    c.scale_type = ScaleType::LINEAR;
    c.test_5pl = {123.989334f, -0.150983f, 0.005f, -162.576993f, 1.769322f};
    c.div_5pl  = {-1.787906f, 0.165878f, 124393.504f, 112.761502f, 0.100f};
    return c;
}

// --- Cardiac Troponin I — 30 measurements, acute MI ---
inline AssayConfig getAssay_cTnI() {
    AssayConfig c = {};
    c.id = "CTNI"; c.name = "Cardiac Troponin I";
    c.loinc_id = "14723-1"; c.measure_unit_id = 13;
    c.signal_type = SignalType::TL_ONLY;
    c.scale_type = ScaleType::LINEAR;
    c.test_5pl = {30.142781f, -10.052006f, 20.465f, 0.024936f, 0.100f};
    c.div_5pl  = {18929.198983f, -0.147196f, 792.049f, 0.002045f, 10.0f};
    return c;
}

// --- Interleukin-6 — 23 measurements, sepsis/inflammation ---
inline AssayConfig getAssay_IL6() {
    AssayConfig c = {};
    c.id = "IL6"; c.name = "Interleukin-6";
    c.loinc_id = "49919-4"; c.measure_unit_id = 13;
    c.signal_type = SignalType::TL_ONLY;
    c.scale_type = ScaleType::LINEAR;
    c.test_5pl = {680801.140768f, -0.139297f, 212726.660f, 0.144272f, 9.494045f};
    c.div_5pl  = {20346.457814f, -0.146755f, 430705.866f, 0.005171f, 8.599626f};
    return c;
}

// --- Procalcitonin — 18 measurements, bacterial sepsis ---
inline AssayConfig getAssay_PCT() {
    AssayConfig c = {};
    c.id = "PCT"; c.name = "Procalcitonin";
    c.loinc_id = "75241-0"; c.measure_unit_id = 13;
    c.signal_type = SignalType::TL_ONLY;
    c.scale_type = ScaleType::LINEAR;
    c.test_5pl = {62.881805f, -10.360543f, 29.608f, 0.257290f, 0.100f};
    c.div_5pl  = {2.533818f, -3.267066f, 33.626f, 0.005643f, 0.305793f};
    return c;
}

// --- CRP German Calibration — 62 measurements, DE market ---
inline AssayConfig getAssay_CRP_DE() {
    AssayConfig c = {};
    c.id = "CRP-DE"; c.name = "CRP (German Calibration)";
    c.loinc_id = "16503-5"; c.measure_unit_id = 13;
    c.signal_type = SignalType::TL_ONLY;
    c.scale_type = ScaleType::LINEAR;
    c.div_5pl  = {6.518555f, -0.337932f, 0.224f, 0.055623f, 10.0f};
    return c;
}

// --- Ferritin German Calibration — 27 measurements, DE market ---
inline AssayConfig getAssay_Ferritin_DE() {
    AssayConfig c = {};
    c.id = "FER-DE"; c.name = "Ferritin (German Calibration)";
    c.loinc_id = "24373-3"; c.measure_unit_id = 13;
    c.signal_type = SignalType::TL_ONLY;
    c.scale_type = ScaleType::LINEAR;
    c.div_5pl  = {25.147761f, -0.261721f, 24.061f, -0.003529f, 10.0f};
    return c;
}

// ══════════════════════════════════════════════════════════════════════
// ASSAY REGISTRY
// ══════════════════════════════════════════════════════════════════════

static constexpr size_t MAX_REGISTRY_ASSAYS = 32;

struct VerifiedAssayRegistry {
    AssayConfig assays[MAX_REGISTRY_ASSAYS];
    size_t count = 0;

    void registerAll() {
        // Clinical Inflammation (4)
        assays[count++] = getAssay_CRP();
        assays[count++] = getAssay_CRP_DE();
        assays[count++] = getAssay_MxA();
        assays[count++] = getAssay_PCT();
        // Cardiac (3)
        assays[count++] = getAssay_cTnI();
        assays[count++] = getAssay_NTproBNP();
        assays[count++] = getAssay_DDimer();
        // Hematology/Iron (3)
        assays[count++] = getAssay_Ferritin();
        assays[count++] = getAssay_Ferritin_DE();
        assays[count++] = getAssay_HbA1c();
        // Sepsis (1)
        assays[count++] = getAssay_IL6();
        // Infectious Disease (2)
        assays[count++] = getAssay_Dengue();
        assays[count++] = getAssay_SAA();
        // Thyroid (2)
        assays[count++] = getAssay_TSH();
        assays[count++] = getAssay_T4();
        // Immunology (2)
        assays[count++] = getAssay_IgE();
        assays[count++] = getAssay_SAb();
        // Gastroenterology (1)
        assays[count++] = getAssay_Pepsinogen();
        // Veterinary (5)
        assays[count++] = getAssay_cPL();
        assays[count++] = getAssay_fPL();
        assays[count++] = getAssay_fSAA();
        assays[count++] = getAssay_CPV();
        assays[count++] = getAssay_SDMA();
        // Drug Panel (7)
        assays[count++] = getAssay_Cocaine();
        assays[count++] = getAssay_Opiates();
        assays[count++] = getAssay_Methamphetamine();
        assays[count++] = getAssay_Amphetamine();
        assays[count++] = getAssay_THC();
        assays[count++] = getAssay_Benzodiazepines();
        assays[count++] = getAssay_Buprenorphine();
    }

    const AssayConfig* findById(const char* id) const {
        for (size_t i = 0; i < count; i++) {
            if (strcmp(assays[i].id.c_str(), id) == 0)
                return &assays[i];
        }
        return nullptr;
    }

    const AssayConfig* findByLoinc(const char* loinc) const {
        for (size_t i = 0; i < count; i++) {
            if (strcmp(assays[i].loinc_id.c_str(), loinc) == 0)
                return &assays[i];
        }
        return nullptr;
    }
};

inline VerifiedAssayRegistry& getVerifiedAssays() {
    static VerifiedAssayRegistry reg;
    static bool init = false;
    if (!init) { reg.registerAll(); init = true; }
    return reg;
}

// ══════════════════════════════════════════════════════════════════════
// STATISTICS
// Total: 30 verified assays from 43 MCP projects
//   Inflammation:   CRP, CRP-DE, MxA, PCT
//   Cardiac:        cTnI, NT-proBNP, D-Dimer
//   Hematology:     Ferritin, Ferritin-DE, HbA1c
//   Sepsis:         IL-6
//   Infectious:     Dengue NS1, SAA
//   Thyroid:        TSH, T4
//   Immunology:     IgE, S-Ab
//   Gastro:         Pepsinogen
//   Veterinary:     cPL, fPL, fSAA, CPV, SDMA
//   Drug Panel:     Cocaine, Opiates, Meth, Amp, THC, BZO, Buprenorphine
//
// Data: 43 projects, 1594 sessions, 1266+ camera images
// Cassettes: 9 manufacturers (Getein, Hightop, Maxhealth, iSIA, Auker,
//            Orient, Kaimi, Agappe, Generic)
// Markets: EU (DE calibrations), Asia, Vet, Forensic
// ══════════════════════════════════════════════════════════════════════

} // namespace phoenix
