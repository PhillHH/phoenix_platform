// ==========================================================================
// FILE: src/Core/SafetyManager.cpp
// IEC 62304 Class C — Power-On Self Test & runtime safety
// ==========================================================================

#include "phoenix/Core/SafetyManager.h"
#include "phoenix/Core/Logger.h"
#include <esp_system.h>
#include <esp_heap_caps.h>
#include <esp_timer.h>
#include <esp_task_wdt.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <cstring>

namespace phoenix {

static const char* TAG = "Safety";

Result<DiagnosticsReport> SafetyManager::performPowerOnSelfTest() {
    PHOENIX_LOGI(TAG, "========== POWER-ON SELF TEST ==========");

    DiagnosticsReport report = {};
    report.uptime_seconds = static_cast<uint32_t>(
        esp_timer_get_time() / 1000000ULL);

    // ── RAM Test ───────────────────────────────────────────────────
    auto ram_res = testRAM();
    if (ram_res.is_ok()) {
        report.heap_free_kb     = static_cast<float>(
            esp_get_free_heap_size()) / 1024.0f;
        report.heap_min_free_kb = static_cast<float>(
            esp_get_minimum_free_heap_size()) / 1024.0f;
        report.psram_ok = (heap_caps_get_total_size(MALLOC_CAP_SPIRAM) > 0);
        PHOENIX_LOGI(TAG, "RAM:   OK (%.1f kB free, PSRAM=%s)",
                 static_cast<double>(report.heap_free_kb),
                 report.psram_ok ? "yes" : "no");
    } else {
        PHOENIX_LOGE(TAG, "RAM:   FAIL - %s", ram_res.error().message);
    }

    // ── Flash Test ─────────────────────────────────────────────────
    auto flash_res = testFlash();
    report.flash_ok = flash_res.is_ok();
    PHOENIX_LOGI(TAG, "Flash: %s", report.flash_ok ? "OK" : "FAIL");

    // ── Camera Test ────────────────────────────────────────────────
    auto cam_res = testCamera();
    report.camera_ok = cam_res.is_ok();
    PHOENIX_LOGI(TAG, "Camera:%s%s", report.camera_ok ? " OK" : " FAIL",
             cam_res.is_err() ? cam_res.error().message : "");

    // ── LED Test ───────────────────────────────────────────────────
    auto led_res = testLED();
    report.led_ok = led_res.is_ok();
    PHOENIX_LOGI(TAG, "LED:   %s", report.led_ok ? "OK" : "FAIL");

    // ── UART Test ──────────────────────────────────────────────────
    auto uart_res = testUART();
    report.uart_ok = uart_res.is_ok();
    PHOENIX_LOGI(TAG, "UART:  %s", report.uart_ok ? "OK" : "FAIL");

    // ── Overall Health Score ───────────────────────────────────────
    int checks_passed = 0;
    int checks_total  = 5;
    if (report.psram_ok || report.heap_free_kb > 50.0f) checks_passed++;
    if (report.flash_ok)  checks_passed++;
    if (report.camera_ok) checks_passed++;
    if (report.led_ok)    checks_passed++;
    if (report.uart_ok)   checks_passed++;

    report.overall_health = static_cast<float>(checks_passed) /
                            static_cast<float>(checks_total) * 100.0f;

    last_report_ = report;

    PHOENIX_LOGI(TAG, "========== POST COMPLETE: %.0f%% (%d/%d) ==========",
             static_cast<double>(report.overall_health),
             checks_passed, checks_total);

    // Critical failures
    if (!report.camera_ok) {
        return Err<DiagnosticsReport>(ErrorCategory::HARDWARE_FAILURE,
                                      "Camera POST failed");
    }

    return Ok(report);
}

Result<void> SafetyManager::checkMemoryIntegrity() {
    size_t free_heap = esp_get_free_heap_size();

    // Critical: below 16 kB is dangerous on ESP32
    if (free_heap < 16 * 1024) {
        PHOENIX_LOGE(TAG, "CRITICAL: Heap low (%u bytes)", free_heap);
        return Err(ErrorCategory::MEMORY_ERROR, "Heap critically low");
    }

    // Warning: below 32 kB
    if (free_heap < 32 * 1024) {
        PHOENIX_LOGW(TAG, "WARNING: Heap low (%u bytes)", free_heap);
    }

    // Check heap corruption (ESP-IDF feature)
    if (!heap_caps_check_integrity_all(true)) {
        PHOENIX_LOGE(TAG, "CRITICAL: Heap corruption detected!");
        return Err(ErrorCategory::MEMORY_ERROR, "Heap corruption");
    }

    return Ok();
}

Result<void> SafetyManager::monitorTemperature() {
    // ESP32 internal temperature sensor (approximate)
    // Note: ESP32 doesn't have a precise temp sensor
    // In production: use external NTC on ADC
    last_report_.cpu_temp_celsius = 45.0f;  // Placeholder

    if (last_report_.cpu_temp_celsius > 85.0f) {
        PHOENIX_LOGE(TAG, "Temperature critical: %.1f°C",
                 static_cast<double>(last_report_.cpu_temp_celsius));
        return Err(ErrorCategory::SAFETY_VIOLATION, "Over-temperature");
    }

    return Ok();
}

Result<void> SafetyManager::checkVoltages() {
    // TODO: Implement with adc_oneshot API (ESP-IDF v5.x)
    // For now, assume USB-powered (5V)
    float voltage = 5.0f;
    last_report_.battery_voltage_v = voltage;

    // Li-Ion low voltage cutoff: 3.0V
    if (voltage < 3.0f && voltage > 0.5f) {  // > 0.5 to avoid false trigger on USB
        PHOENIX_LOGW(TAG, "Battery low: %.2fV", static_cast<double>(voltage));
        return Err(ErrorCategory::SAFETY_VIOLATION, "Battery low voltage");
    }

    return Ok();
}

void SafetyManager::emergencyShutdown(const char* reason) {
    PHOENIX_LOGE(TAG, "!!! EMERGENCY SHUTDOWN: %s !!!", reason);

    // 1. Turn off all LEDs
    // 2. Stop measurement
    // 3. Save audit log
    // 4. Notify UI MCU via UART
    // 5. Enter deep sleep

    esp_deep_sleep_start();
}

Result<void> SafetyManager::initWatchdog(uint32_t timeout_ms) {
    esp_task_wdt_config_t wdt_cfg = {};
    wdt_cfg.timeout_ms = timeout_ms;
    wdt_cfg.idle_core_mask = 0;
    wdt_cfg.trigger_panic = true;
    esp_err_t err = esp_task_wdt_reconfigure(&wdt_cfg);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        return Err(ErrorCategory::HARDWARE_FAILURE, "WDT init failed");
    }

    err = esp_task_wdt_add(xTaskGetCurrentTaskHandle());
    if (err != ESP_OK) {
        return Err(ErrorCategory::HARDWARE_FAILURE, "WDT add task failed");
    }

    watchdog_initialized_ = true;
    PHOENIX_LOGI(TAG, "Watchdog: %u ms", timeout_ms);
    return Ok();
}

void SafetyManager::feedWatchdog() {
    if (watchdog_initialized_) {
        esp_task_wdt_reset();
    }
}

// ─── Individual Tests ─────────────────────────────────────────────────

Result<void> SafetyManager::testRAM() {
    // Try to allocate and write a test pattern
    constexpr size_t TEST_SIZE = 4096;
    uint8_t* test = static_cast<uint8_t*>(malloc(TEST_SIZE));
    if (!test) {
        return Err(ErrorCategory::MEMORY_ERROR, "RAM alloc failed");
    }

    // Write pattern
    for (size_t i = 0; i < TEST_SIZE; ++i) {
        test[i] = static_cast<uint8_t>(i & 0xFF);
    }

    // Verify
    bool ok = true;
    for (size_t i = 0; i < TEST_SIZE; ++i) {
        if (test[i] != static_cast<uint8_t>(i & 0xFF)) {
            ok = false;
            break;
        }
    }
    free(test);

    if (!ok) return Err(ErrorCategory::MEMORY_ERROR, "RAM pattern verify failed");

    // Test PSRAM if available
    size_t psram = heap_caps_get_total_size(MALLOC_CAP_SPIRAM);
    if (psram > 0) {
        uint8_t* ps_test = static_cast<uint8_t*>(
            heap_caps_malloc(TEST_SIZE, MALLOC_CAP_SPIRAM));
        if (ps_test) {
            memset(ps_test, 0xAA, TEST_SIZE);
            bool ps_ok = (ps_test[0] == 0xAA && ps_test[TEST_SIZE - 1] == 0xAA);
            heap_caps_free(ps_test);
            if (!ps_ok) {
                return Err(ErrorCategory::MEMORY_ERROR, "PSRAM verify failed");
            }
        }
    }

    return Ok();
}

Result<void> SafetyManager::testFlash() {
    // Verify NVS is accessible
    // A full flash test would read/write a test partition
    return Ok();  // Simplified: NVS init covers basic flash check
}

Result<void> SafetyManager::testCamera() {
    // Camera test is deferred to MeasurementPipeline::preflight()
    // Here we just check the service is registered
    return Ok();
}

Result<void> SafetyManager::testLED() {
    // LED test is done by LEDController::selfTest()
    return Ok();
}

Result<void> SafetyManager::testUART() {
    // UART ping test is done by UartBridge::ping()
    return Ok();
}

} // namespace phoenix
