// ==========================================================================
// FILE: include/phoenix/HAL/Interfaces.h
// Hardware Abstraction Layer — interfaces for all hardware
// Allows mock injection for unit testing (IEC 62304 requirement)
// ==========================================================================
#pragma once

#include "phoenix/Core/Result.h"
#include <cstdint>
#include <cstddef>

namespace phoenix {

// ─── Image buffer (fixed, no heap) ────────────────────────────────────
// OV2686: 1600x1200 max, but we capture at 640x480 for LFA analysis
// DVP 8-bit grayscale = 640*480 = 307,200 bytes
// With PSRAM this fits comfortably
static constexpr uint16_t IMAGE_WIDTH  = 640;
static constexpr uint16_t IMAGE_HEIGHT = 480;
static constexpr size_t   IMAGE_SIZE   = IMAGE_WIDTH * IMAGE_HEIGHT;

struct ImageBuffer {
    uint8_t* data       = nullptr;   // Points to PSRAM or DMA buffer
    uint16_t width      = 0;
    uint16_t height     = 0;
    size_t   size       = 0;
    uint32_t timestamp  = 0;         // ms since boot
    uint8_t  exposure   = 0;
    uint8_t  gain       = 0;
};

// ─── Camera Controller Interface ──────────────────────────────────────
struct CameraConfig {
    uint16_t width       = IMAGE_WIDTH;
    uint16_t height      = IMAGE_HEIGHT;
    uint8_t  exposure    = 128;      // 0-255
    uint8_t  gain        = 64;       // 0-255
    bool     auto_expose = false;
};

class ICameraController {
public:
    virtual ~ICameraController() = default;

    virtual Result<void>        initialize(const CameraConfig& cfg) = 0;
    virtual Result<ImageBuffer> captureImage()                       = 0;
    virtual Result<void>        setExposure(uint8_t value)          = 0;
    virtual Result<void>        setGain(uint8_t value)              = 0;
    virtual Result<void>        selfTest()                           = 0;
    virtual void                deinitialize()                       = 0;
};

// ─── LED Controller Interface ─────────────────────────────────────────
enum class LEDMode : uint8_t {
    OFF         = 0,
    WHITE       = 1,   // Illumination for LFA imaging
    UV_365NM    = 2,   // TRF excitation
    STATUS_RGB  = 3,   // Status ring
};

struct LEDConfig {
    LEDMode  mode       = LEDMode::OFF;
    uint8_t  intensity  = 255;    // 0-255 PWM duty
    uint8_t  r = 0, g = 0, b = 0; // For STATUS_RGB mode
};

class ILEDController {
public:
    virtual ~ILEDController() = default;

    virtual Result<void> initialize()                  = 0;
    virtual Result<void> setMode(const LEDConfig& cfg) = 0;
    virtual Result<void> selfTest()                    = 0;
    virtual void         off()                         = 0;
};

} // namespace phoenix
