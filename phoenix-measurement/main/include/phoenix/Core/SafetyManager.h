// ==========================================================================
// FILE: include/phoenix/Core/SafetyManager.h
// IEC 62304 Class C — Safety monitoring and Power-On Self Test
// REQ-SAFE-001: System SHALL perform POST
// ==========================================================================
#pragma once

#include "phoenix/Core/Result.h"
#include <cstdint>

namespace phoenix {

struct DiagnosticsReport {
    float    heap_free_kb       = 0.0f;
    float    heap_min_free_kb   = 0.0f;
    float    cpu_temp_celsius   = 0.0f;
    float    battery_voltage_v  = 0.0f;
    bool     psram_ok           = false;
    bool     flash_ok           = false;
    bool     camera_ok          = false;
    bool     led_ok             = false;
    bool     uart_ok            = false;
    float    overall_health     = 0.0f;   // 0..100%
    uint32_t uptime_seconds     = 0;
};

class SafetyManager {
public:
    // Run at boot — checks RAM, Flash, sensors
    Result<DiagnosticsReport> performPowerOnSelfTest();

    // Runtime monitoring (called periodically from main loop)
    Result<void> checkMemoryIntegrity();
    Result<void> monitorTemperature();
    Result<void> checkVoltages();

    // Emergency: safe state, log event, notify UI MCU
    void emergencyShutdown(const char* reason);

    // Watchdog
    Result<void> initWatchdog(uint32_t timeout_ms);
    void feedWatchdog();

    // Diagnostics
    DiagnosticsReport getLastReport() const { return last_report_; }

private:
    DiagnosticsReport last_report_ = {};
    bool watchdog_initialized_ = false;

    Result<void> testRAM();
    Result<void> testFlash();
    Result<void> testCamera();
    Result<void> testLED();
    Result<void> testUART();
};

} // namespace phoenix
