// ==========================================================================
// FILE: include/phoenix/HAL/CameraController_OV2686.h
// OV2686 2MP DVP Camera — Igloo Pro Hardware
// Supertek SHWX01 module, DVP 8-bit, 1600x1200 native
// ==========================================================================
#pragma once

#include "phoenix/HAL/Interfaces.h"

namespace phoenix {

// Pin mapping for Igloo Pro PCB (DXR-EG-000-MAIN-PCB)
struct OV2686Pins {
    // DVP data bus D0-D7
    int d0 = 5,  d1 = 18, d2 = 19, d3 = 21;
    int d4 = 36, d5 = 39, d6 = 34, d7 = 35;
    // Control
    int xclk   = 0;    // Master clock output
    int pclk   = 22;   // Pixel clock input
    int vsync  = 25;   // Frame sync
    int href   = 23;   // Line valid
    int sda    = 26;   // I2C data (SCCB)
    int scl    = 27;   // I2C clock (SCCB)
    int reset  = 15;   // Active low
    int pwdn   = -1;   // Not connected on Igloo Pro
};

class CameraController_OV2686 : public ICameraController {
public:
    explicit CameraController_OV2686(const OV2686Pins& pins = {});
    ~CameraController_OV2686() override;

    Result<void>        initialize(const CameraConfig& cfg) override;
    Result<ImageBuffer> captureImage() override;
    Result<void>        setExposure(uint8_t value) override;
    Result<void>        setGain(uint8_t value) override;
    Result<void>        selfTest() override;
    void                deinitialize() override;

    // OV2686-specific
    Result<uint16_t> readChipID();
    Result<void>     setTestPattern(bool enable);

private:
    OV2686Pins  pins_;
    CameraConfig config_;
    bool         initialized_ = false;
    uint8_t*     frame_buffer_ = nullptr;  // PSRAM-allocated

    Result<void> sccbWrite(uint16_t reg, uint8_t value);
    Result<uint8_t> sccbRead(uint16_t reg);
    Result<void> loadDefaultRegs();
    Result<void> setResolution(uint16_t w, uint16_t h);
};

} // namespace phoenix
