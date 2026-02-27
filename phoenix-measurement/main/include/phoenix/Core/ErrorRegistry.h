// ==========================================================================
// FILE: include/phoenix/Core/ErrorRegistry.h
// Phoenix v108.0 — Persistent Error Registry for IEC 62304 Class C
// REQ-ERR-001: System SHALL record and persist all device errors
//
// Heap-free design: static array in .bss, NVS persistence
// Errors are deduplicated by code — repeated occurrences increment counter.
// ==========================================================================
#pragma once

#include "phoenix/Core/Result.h"
#include "phoenix/Core/FixedString.h"
#include <cstdint>
#include <cstring>

#ifndef PHOENIX_HOST_TEST
  #include <nvs.h>
  #include <nvs_flash.h>
  #include <esp_timer.h>
#endif

namespace phoenix {

// ── Error Severity (ISO 14971 alignment) ────────────────────────────────

enum class ErrorSeverity : uint8_t {
    LOW      = 0,    // Informational, no patient risk
    MEDIUM   = 1,    // Degraded performance, acceptable risk
    HIGH     = 2,    // May affect measurement accuracy
    CRITICAL = 3,    // Patient safety risk — requires immediate action
};

// ── Predefined Error Code Ranges ────────────────────────────────────────
//
// 0x0100-0x01FF  HARDWARE     Camera-Init, LED-Failure, Temperature
// 0x0200-0x02FF  CALIBRATION  Fit diverged, R² too low, Expired
// 0x0300-0x03FF  MEASUREMENT  Invalid profile, Peak not found, QC Fail
// 0x0400-0x04FF  COMMUNICATION UART CRC Error, Frame Timeout, WiFi
// 0x0500-0x05FF  SAFETY       POST Fail, Watchdog, Memory Corruption

namespace ErrorCode {
    // ── Hardware (0x01xx) ────────────────────────────────────────
    static constexpr uint16_t HW_CAMERA_INIT_FAIL     = 0x0100;
    static constexpr uint16_t HW_CAMERA_CAPTURE_FAIL   = 0x0101;
    static constexpr uint16_t HW_LED_FAILURE           = 0x0110;
    static constexpr uint16_t HW_LED_OVERCURRENT       = 0x0111;
    static constexpr uint16_t HW_TEMP_SENSOR_FAIL      = 0x0120;
    static constexpr uint16_t HW_TEMP_OVER_LIMIT       = 0x0121;
    static constexpr uint16_t HW_ADC_FAILURE           = 0x0130;
    static constexpr uint16_t HW_FLASH_FAILURE         = 0x0140;

    // ── Calibration (0x02xx) ────────────────────────────────────
    static constexpr uint16_t CAL_FIT_DIVERGED         = 0x0200;
    static constexpr uint16_t CAL_R2_TOO_LOW           = 0x0201;
    static constexpr uint16_t CAL_RESIDUALS_HIGH       = 0x0202;
    static constexpr uint16_t CAL_EXPIRED              = 0x0210;
    static constexpr uint16_t CAL_NOT_FOUND            = 0x0211;
    static constexpr uint16_t CAL_NVS_SAVE_FAIL        = 0x0220;
    static constexpr uint16_t CAL_NVS_LOAD_FAIL        = 0x0221;
    static constexpr uint16_t CAL_INSUFFICIENT_POINTS  = 0x0230;

    // ── Measurement (0x03xx) ────────────────────────────────────
    static constexpr uint16_t MEAS_PROFILE_INVALID     = 0x0300;
    static constexpr uint16_t MEAS_PEAK_NOT_FOUND      = 0x0301;
    static constexpr uint16_t MEAS_CONTROL_LINE_WEAK   = 0x0302;
    static constexpr uint16_t MEAS_QC_FAIL             = 0x0310;
    static constexpr uint16_t MEAS_OUT_OF_RANGE        = 0x0311;
    static constexpr uint16_t MEAS_IMAGE_CORRUPT       = 0x0320;
    static constexpr uint16_t MEAS_ROI_INVALID         = 0x0321;

    // ── Communication (0x04xx) ──────────────────────────────────
    static constexpr uint16_t COMM_UART_CRC_ERROR      = 0x0400;
    static constexpr uint16_t COMM_UART_FRAME_TIMEOUT  = 0x0401;
    static constexpr uint16_t COMM_UART_SYNC_LOST      = 0x0402;
    static constexpr uint16_t COMM_UART_OVERFLOW       = 0x0403;
    static constexpr uint16_t COMM_WIFI_DISCONNECT     = 0x0410;
    static constexpr uint16_t COMM_BLE_FAILURE         = 0x0420;
    static constexpr uint16_t COMM_IPC_NACK            = 0x0430;

    // ── Safety (0x05xx) ─────────────────────────────────────────
    static constexpr uint16_t SAFE_POST_FAIL           = 0x0500;
    static constexpr uint16_t SAFE_WATCHDOG_TIMEOUT    = 0x0501;
    static constexpr uint16_t SAFE_MEMORY_CORRUPTION   = 0x0502;
    static constexpr uint16_t SAFE_HEAP_LOW            = 0x0503;
    static constexpr uint16_t SAFE_STACK_OVERFLOW      = 0x0504;
    static constexpr uint16_t SAFE_VOLTAGE_LOW         = 0x0510;
    static constexpr uint16_t SAFE_OVER_TEMPERATURE    = 0x0511;
    static constexpr uint16_t SAFE_EMERGENCY_SHUTDOWN  = 0x05FF;
} // namespace ErrorCode

// ── Error Entry ─────────────────────────────────────────────────────────

struct ErrorEntry {
    uint16_t        code           = 0;
    ErrorSeverity   severity       = ErrorSeverity::LOW;
    ErrorCategory   category       = ErrorCategory::NONE;
    FixedString<64> message;
    FixedString<64> recovery_hint;
    uint32_t        first_seen_ms  = 0;     // Timestamp of first occurrence
    uint32_t        last_seen_ms   = 0;     // Timestamp of most recent
    uint16_t        occurrence_count = 0;

    bool active() const { return code != 0; }
};

// ── Error Registry Singleton ────────────────────────────────────────────

class ErrorRegistry {
public:
    static constexpr int MAX_ERRORS  = 50;
    static constexpr uint32_t NVS_MAGIC = 0x45525231; // "ERR1"

    static ErrorRegistry& instance() {
        static ErrorRegistry reg;
        return reg;
    }

    // Register an error (new or increment existing)
    void registerError(uint16_t code, ErrorSeverity severity,
                       ErrorCategory category,
                       const char* message,
                       const char* recovery_hint = nullptr) {
        uint32_t now = currentTimeMs();

        // Deduplicate: find existing entry with same code
        for (int i = 0; i < count_; i++) {
            if (entries_[i].code == code) {
                entries_[i].occurrence_count++;
                entries_[i].last_seen_ms = now;
                // Update severity if escalated
                if (severity > entries_[i].severity) {
                    entries_[i].severity = severity;
                }
                return;
            }
        }

        // New error
        if (count_ < MAX_ERRORS) {
            auto& e = entries_[count_];
            e.code = code;
            e.severity = severity;
            e.category = category;
            e.message = message;
            if (recovery_hint) e.recovery_hint = recovery_hint;
            e.first_seen_ms = now;
            e.last_seen_ms = now;
            e.occurrence_count = 1;
            count_++;
        } else {
            // Buffer full — overwrite oldest LOW-severity entry
            int oldest_low = -1;
            uint32_t oldest_time = UINT32_MAX;
            for (int i = 0; i < MAX_ERRORS; i++) {
                if (entries_[i].severity == ErrorSeverity::LOW &&
                    entries_[i].last_seen_ms < oldest_time) {
                    oldest_low = i;
                    oldest_time = entries_[i].last_seen_ms;
                }
            }
            if (oldest_low >= 0) {
                auto& e = entries_[oldest_low];
                e.code = code;
                e.severity = severity;
                e.category = category;
                e.message = message;
                if (recovery_hint) e.recovery_hint = recovery_hint;
                else e.recovery_hint.clear();
                e.first_seen_ms = now;
                e.last_seen_ms = now;
                e.occurrence_count = 1;
            }
            // If no LOW entries to evict, the error is lost (CRITICAL/HIGH preserved)
        }
    }

    // ── Accessors ───────────────────────────────────────────────────

    int getErrorCount() const { return count_; }

    const ErrorEntry* getErrors() const { return entries_; }

    const ErrorEntry* findByCode(uint16_t code) const {
        for (int i = 0; i < count_; i++) {
            if (entries_[i].code == code) return &entries_[i];
        }
        return nullptr;
    }

    // Count errors at or above a severity level
    int countBySeverity(ErrorSeverity min_severity) const {
        int n = 0;
        for (int i = 0; i < count_; i++) {
            if (entries_[i].severity >= min_severity) n++;
        }
        return n;
    }

    // Count errors in a category
    int countByCategory(ErrorCategory category) const {
        int n = 0;
        for (int i = 0; i < count_; i++) {
            if (entries_[i].category == category) n++;
        }
        return n;
    }

    bool hasCriticalErrors() const {
        return countBySeverity(ErrorSeverity::CRITICAL) > 0;
    }

    void clearErrors() {
        for (int i = 0; i < MAX_ERRORS; i++) {
            entries_[i] = {};
        }
        count_ = 0;
    }

    // ── NVS Persistence ─────────────────────────────────────────────

    Result<void> persistToNvs() {
#ifndef PHOENIX_HOST_TEST
        nvs_handle_t handle;
        esp_err_t err = nvs_open("err_reg", NVS_READWRITE, &handle);
        if (err != ESP_OK) {
            return Err(ErrorCategory::HARDWARE_FAILURE, "NVS open failed");
        }

        // Write magic + count header
        err = nvs_set_u32(handle, "err_magic", NVS_MAGIC);
        if (err != ESP_OK) { nvs_close(handle); return Err(ErrorCategory::HARDWARE_FAILURE, "NVS write magic"); }

        err = nvs_set_u32(handle, "err_count", static_cast<uint32_t>(count_));
        if (err != ESP_OK) { nvs_close(handle); return Err(ErrorCategory::HARDWARE_FAILURE, "NVS write count"); }

        // Write entries as blob
        err = nvs_set_blob(handle, "err_data", entries_, sizeof(ErrorEntry) * count_);
        if (err != ESP_OK) { nvs_close(handle); return Err(ErrorCategory::HARDWARE_FAILURE, "NVS write errors"); }

        err = nvs_commit(handle);
        nvs_close(handle);
        if (err != ESP_OK) return Err(ErrorCategory::HARDWARE_FAILURE, "NVS commit");
#endif
        return Ok();
    }

    Result<void> loadFromNvs() {
#ifndef PHOENIX_HOST_TEST
        nvs_handle_t handle;
        esp_err_t err = nvs_open("err_reg", NVS_READONLY, &handle);
        if (err != ESP_OK) {
            return Err(ErrorCategory::NOT_FOUND, "NVS open failed");
        }

        uint32_t magic = 0;
        err = nvs_get_u32(handle, "err_magic", &magic);
        if (err != ESP_OK || magic != NVS_MAGIC) {
            nvs_close(handle);
            return Err(ErrorCategory::NOT_FOUND, "No error registry in NVS");
        }

        uint32_t stored_count = 0;
        err = nvs_get_u32(handle, "err_count", &stored_count);
        if (err != ESP_OK || stored_count > MAX_ERRORS) {
            nvs_close(handle);
            return Err(ErrorCategory::INVALID_PARAMETER, "Invalid error count");
        }

        size_t blob_size = sizeof(ErrorEntry) * stored_count;
        err = nvs_get_blob(handle, "err_data", entries_, &blob_size);
        nvs_close(handle);

        if (err != ESP_OK) {
            return Err(ErrorCategory::HARDWARE_FAILURE, "NVS read errors");
        }

        count_ = static_cast<int>(stored_count);
#endif
        return Ok();
    }

private:
    ErrorRegistry() = default;
    ErrorRegistry(const ErrorRegistry&) = delete;
    ErrorRegistry& operator=(const ErrorRegistry&) = delete;

    ErrorEntry entries_[MAX_ERRORS] = {};
    int        count_ = 0;

    static uint32_t currentTimeMs() {
#ifndef PHOENIX_HOST_TEST
        return static_cast<uint32_t>(esp_timer_get_time() / 1000ULL);
#else
        return 0;
#endif
    }
};

// ── Severity name utility ───────────────────────────────────────────────

inline const char* severityName(ErrorSeverity sev) {
    switch (sev) {
        case ErrorSeverity::LOW:      return "LOW";
        case ErrorSeverity::MEDIUM:   return "MEDIUM";
        case ErrorSeverity::HIGH:     return "HIGH";
        case ErrorSeverity::CRITICAL: return "CRITICAL";
        default:                      return "???";
    }
}

} // namespace phoenix
