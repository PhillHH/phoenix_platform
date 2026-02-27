// ==========================================================================
// FILE: src/Core/AuditTrail.cpp
// IEC 62304 Audit Trail — logs all safety-relevant events to NVS
// ==========================================================================

#include "phoenix/Core/Result.h"
#include "phoenix/Core/FixedString.h"
#include <esp_log.h>
#include <esp_timer.h>
#include <nvs_flash.h>
#include <nvs.h>

namespace phoenix {

static const char* TAG = "Audit";

enum class AuditEvent : uint8_t {
    SYSTEM_BOOT        = 0x01,
    MEASUREMENT_START  = 0x10,
    MEASUREMENT_DONE   = 0x11,
    MEASUREMENT_FAIL   = 0x12,
    CALIBRATION_DONE   = 0x20,
    SAFETY_WARNING     = 0x30,
    SAFETY_EMERGENCY   = 0x31,
    ERROR              = 0x40,
};

struct __attribute__((packed)) AuditEntry {
    uint32_t   timestamp;
    AuditEvent event;
    uint8_t    severity;    // 0=info, 1=warning, 2=error, 3=critical
    char       message[64];
};

class AuditTrail {
public:
    Result<void> initialize() {
        ESP_LOGI(TAG, "Audit trail initialized");
        return Ok();
    }

    Result<void> log(AuditEvent event, uint8_t severity, const char* msg) {
        AuditEntry entry = {};
        entry.timestamp = static_cast<uint32_t>(
            esp_timer_get_time() / 1000000ULL);
        entry.event    = event;
        entry.severity = severity;
        if (msg) {
            strncpy(entry.message, msg, sizeof(entry.message) - 1);
        }

        // Store in ring buffer in NVS
        nvs_handle_t handle;
        esp_err_t err = nvs_open("audit", NVS_READWRITE, &handle);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "NVS open failed for audit");
            return Err(ErrorCategory::HARDWARE_FAILURE, "Audit NVS failed");
        }

        // Increment write index
        uint32_t idx = 0;
        nvs_get_u32(handle, "write_idx", &idx);

        char key[16];
        snprintf(key, sizeof(key), "e%05u", static_cast<unsigned>(idx % 1000));  // Ring buffer of 1000

        nvs_set_blob(handle, key, &entry, sizeof(entry));
        nvs_set_u32(handle, "write_idx", idx + 1);
        nvs_commit(handle);
        nvs_close(handle);

        static const char* const sev_str[] = {"INFO", "WARN", "ERROR", "CRIT"};
        (void)sev_str;
        ESP_LOGI(TAG, "[%s] %s",
                 sev_str[severity & 3],
                 msg ? msg : "(no message)");

        return Ok();
    }

    uint32_t getEntryCount() const { return entry_count_; }

private:
    uint32_t entry_count_ = 0;
};

// Global instance
static AuditTrail s_audit;

AuditTrail* getAuditTrail() { return &s_audit; }

} // namespace phoenix
