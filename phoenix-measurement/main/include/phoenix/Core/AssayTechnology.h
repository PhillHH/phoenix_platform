// ==========================================================================
// FILE: include/phoenix/Core/AssayTechnology.h
// Phoenix v108.0 — Assay technology classification
// ==========================================================================
#pragma once

#include <cstdint>

namespace phoenix {

enum class Technology : uint8_t {
    COLORIMETRIC        = 0,  // Gold nanoparticle LFT
    IMMUNOFLUORESCENCE  = 1,  // UV excitation, fluorescent emission
    DRY_CHEMISTRY       = 2,  // Reflectance photometry on reagent pads
    MICROFLUIDICS       = 3,  // Lab-on-chip capillary analysis
};

} // namespace phoenix
