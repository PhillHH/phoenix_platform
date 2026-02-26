// ==========================================================================
// FILE: include/phoenix/Core/AssayTechnology.h
// Phoenix v108.0 — Assay technology definitions for Igloo Pro
// Supports: Colorimetric, Immunofluorescence, Dry Chemistry, Microfluidics
// ==========================================================================
#pragma once
#include "phoenix/Core/FixedString.h"
#include <cstdint>

namespace phoenix {

// ── Measurement Technologies ──────────────────────────────────────────
enum class Technology : uint8_t {
    COLORIMETRIC       = 0,  // Lateral Flow Test (LFT), gold nanoparticle
    IMMUNOFLUORESCENCE = 1,  // Fluorescent label detection (UV excitation)
    DRY_CHEMISTRY      = 2,  // Reagent pad reflectance (Trockenchemie)
    MICROFLUIDICS      = 3,  // Lab-on-chip, capillary electrophoresis
};

static constexpr size_t NUM_TECHNOLOGIES = 4;

// ── Assay Definition ──────────────────────────────────────────────────
// Each test cassette type is defined by its assay profile
struct AssayDefinition {
    String32   code;              // e.g. "CRP-COL-01"
    String64   name;              // e.g. "C-Reactive Protein"
    String64   name_de;           // German name
    Technology tech;
    String16   unit;              // e.g. "mg/L", "ng/mL", "mmol/L"
    float      range_low;         // clinical reference low
    float      range_high;        // clinical reference high
    float      cutoff_positive;   // positive threshold
    float      lod;               // limit of detection
    float      loq;               // limit of quantification
    uint16_t   incubation_sec;    // incubation time
    uint16_t   read_delay_sec;    // delay before reading
    uint8_t    num_lines;         // expected lines (LFT: C + T lines)
    bool       quantitative;      // true = concentration, false = pos/neg
};

// ── Technology Metadata ───────────────────────────────────────────────
struct TechInfo {
    const char*  name;
    const char*  name_de;
    const char*  icon;            // LVGL symbol
    const char*  description;
    const char*  description_de;
    uint32_t     color_hex;       // UI accent color
    uint16_t     typical_time_sec;
    bool         needs_uv;        // UV LED excitation needed
};

// ── Built-in Technology Metadata ──────────────────────────────────────
inline const TechInfo& getTechInfo(Technology t) {
    static const TechInfo infos[NUM_TECHNOLOGIES] = {
        // COLORIMETRIC
        {
            "Colorimetric", "Kolorimetrisch",
            LV_SYMBOL_EYE_OPEN,
            "Gold nanoparticle lateral flow immunoassay",
            "Gold-Nanopartikel Lateral-Flow-Immunoassay",
            0x22C55E,  // green
            180, false
        },
        // IMMUNOFLUORESCENCE
        {
            "Immunofluorescence", "Immunfluoreszenz",
            LV_SYMBOL_CHARGE,
            "Fluorescent label detection with UV excitation",
            "Fluoreszenzmarkierung mit UV-Anregung",
            0x8B5CF6,  // purple
            240, true
        },
        // DRY_CHEMISTRY
        {
            "Dry Chemistry", "Trockenchemie",
            LV_SYMBOL_LIST,
            "Multi-pad reflectance photometry",
            "Multi-Pad Reflexionsphotometrie",
            0xF59E0B,  // amber
            120, false
        },
        // MICROFLUIDICS
        {
            "Microfluidics", "Mikrofluidik",
            LV_SYMBOL_SHUFFLE,
            "Lab-on-chip capillary analysis",
            "Lab-on-Chip Kapillaranalyse",
            0x06B6D4,  // cyan
            300, false
        },
    };
    return infos[static_cast<uint8_t>(t)];
}

// ── Built-in Demo Assays ──────────────────────────────────────────────
// These represent real-world test panels the Igloo Pro supports

inline const AssayDefinition* getDemoAssays(size_t& count) {
    static const AssayDefinition assays[] = {
        // === COLORIMETRIC (Lateral Flow) ===
        {"CRP-COL", "CRP (hs)", "CRP (hs)",
         Technology::COLORIMETRIC, "mg/L",
         0.0f, 5.0f, 5.0f, 0.5f, 1.0f,
         180, 10, 2, true},

        {"PROCAL-COL", "Procalcitonin", "Procalcitonin",
         Technology::COLORIMETRIC, "ng/mL",
         0.0f, 0.5f, 0.5f, 0.02f, 0.05f,
         180, 10, 2, true},

        {"TSH-COL", "TSH", "TSH",
         Technology::COLORIMETRIC, "mIU/L",
         0.27f, 4.2f, 10.0f, 0.1f, 0.2f,
         180, 10, 2, true},

        {"DIMER-COL", "D-Dimer", "D-Dimer",
         Technology::COLORIMETRIC, "mg/L FEU",
         0.0f, 0.5f, 0.5f, 0.1f, 0.2f,
         180, 10, 2, true},

        {"TROPO-COL", "Troponin I", "Troponin I",
         Technology::COLORIMETRIC, "ng/mL",
         0.0f, 0.04f, 0.04f, 0.006f, 0.01f,
         180, 10, 2, true},

        // === IMMUNOFLUORESCENCE ===
        {"VITD-FLU", "Vitamin D (25-OH)", "Vitamin D (25-OH)",
         Technology::IMMUNOFLUORESCENCE, "ng/mL",
         30.0f, 100.0f, 20.0f, 3.0f, 5.0f,
         240, 15, 2, true},

        {"FER-FLU", "Ferritin", "Ferritin",
         Technology::IMMUNOFLUORESCENCE, "ng/mL",
         12.0f, 300.0f, 12.0f, 2.0f, 5.0f,
         240, 15, 2, true},

        {"BHCG-FLU", "Beta-HCG", "Beta-HCG",
         Technology::IMMUNOFLUORESCENCE, "mIU/mL",
         0.0f, 5.0f, 5.0f, 1.0f, 2.0f,
         240, 15, 2, true},

        {"PSA-FLU", "PSA (total)", "PSA (gesamt)",
         Technology::IMMUNOFLUORESCENCE, "ng/mL",
         0.0f, 4.0f, 4.0f, 0.1f, 0.2f,
         240, 15, 2, true},

        // === DRY CHEMISTRY (Trockenchemie) ===
        {"LIPID-DRY", "Lipid Panel", "Lipid-Panel",
         Technology::DRY_CHEMISTRY, "mg/dL",
         0.0f, 200.0f, 200.0f, 10.0f, 20.0f,
         120, 5, 4, true},

        {"HBA1C-DRY", "HbA1c", "HbA1c",
         Technology::DRY_CHEMISTRY, "%",
         4.0f, 5.6f, 6.5f, 3.0f, 3.5f,
         120, 5, 1, true},

        {"GLUC-DRY", "Glucose", "Glukose",
         Technology::DRY_CHEMISTRY, "mg/dL",
         70.0f, 100.0f, 126.0f, 10.0f, 20.0f,
         60, 5, 1, true},

        {"CREA-DRY", "Creatinine", "Kreatinin",
         Technology::DRY_CHEMISTRY, "mg/dL",
         0.6f, 1.2f, 1.3f, 0.1f, 0.2f,
         120, 5, 1, true},

        // === MICROFLUIDICS ===
        {"CBC-MFL", "Blood Count (3-part)", "Blutbild (3-part)",
         Technology::MICROFLUIDICS, "cells/uL",
         4000.0f, 10000.0f, 11000.0f, 100.0f, 200.0f,
         300, 20, 3, true},

        {"COAG-MFL", "Coagulation (PT/INR)", "Gerinnung (PT/INR)",
         Technology::MICROFLUIDICS, "INR",
         0.8f, 1.2f, 1.5f, 0.1f, 0.2f,
         240, 15, 1, true},

        {"ELECT-MFL", "Electrolytes (Na/K/Cl)", "Elektrolyte (Na/K/Cl)",
         Technology::MICROFLUIDICS, "mmol/L",
         136.0f, 145.0f, 150.0f, 1.0f, 2.0f,
         180, 10, 3, true},
    };
    count = sizeof(assays) / sizeof(assays[0]);
    return assays;
}

} // namespace phoenix
