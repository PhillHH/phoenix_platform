// ==========================================================================
// FILE: src/Services/PatientManager.cpp
// Phoenix v108.0 — Patient management (NVS-backed)
// GDPR-compliant: minimal PII, anonymization support
// IEC 62304: Patient-result linkage (REQ-PAT-001)
// ==========================================================================
#include "phoenix/Core/Result.h"
#include "phoenix/Core/FixedString.h"
#include <esp_log.h>
#include <nvs_flash.h>
#include <nvs.h>
#include <cstring>

namespace phoenix {

static const char* TAG = "Patient";
static constexpr size_t   MAX_PATIENTS  = 200;
static constexpr char     NVS_NS[]      = "patients";

struct __attribute__((packed)) PatientRecord {
    static constexpr uint16_t MAGIC   = 0x5041; // "PA"
    static constexpr uint16_t VERSION = 1;

    uint16_t magic       = MAGIC;
    uint16_t version     = VERSION;
    uint32_t patient_id  = 0;
    uint32_t created_at  = 0;
    uint32_t last_test   = 0;
    uint16_t test_count  = 0;
    uint8_t  age         = 0;
    uint8_t  sex         = 0;       // 0=unknown, 1=male, 2=female
    char     display_id[24] = {};   // External ID (e.g., practice number)
    char     initials[8]    = {};   // Max 3 chars + null
    char     notes[64]      = {};
    uint8_t  active      = 1;       // Soft-delete support
    uint8_t  _pad[3]     = {};

    bool isValid() const { return magic == MAGIC && version == VERSION && patient_id > 0; }
};

struct __attribute__((packed)) PatientIndex {
    uint32_t count   = 0;
    uint32_t next_id = 1;
};

class PatientManager {
public:
    Result<void> initialize() {
        nvs_handle_t h;
        if (nvs_open(NVS_NS, NVS_READWRITE, &h) != ESP_OK)
            return Err(ErrorCategory::HARDWARE_FAILURE, "Patient NVS failed");

        size_t sz = sizeof(PatientIndex);
        if (nvs_get_blob(h, "_pidx", &idx_, &sz) != ESP_OK) {
            idx_ = {};
            nvs_set_blob(h, "_pidx", &idx_, sizeof(idx_));
            nvs_commit(h);
        }
        nvs_close(h);
        ready_ = true;
        ESP_LOGI(TAG, "Loaded: %lu patients", idx_.count);
        return Ok();
    }

    Result<uint32_t> createPatient(const char* display_id,
                                    const char* initials = nullptr,
                                    uint8_t age = 0, uint8_t sex = 0)
    {
        if (!ready_) return Err<uint32_t>(ErrorCategory::NOT_INITIALIZED, "N");
        if (idx_.count >= MAX_PATIENTS)
            return Err<uint32_t>(ErrorCategory::MEMORY_ERROR, "Patient table full");

        PatientRecord rec = {};
        rec.patient_id = idx_.next_id;
        rec.created_at = 0; // Caller should set from RTC
        rec.age = age;
        rec.sex = sex;
        if (display_id) strncpy(rec.display_id, display_id, 23);
        if (initials) strncpy(rec.initials, initials, 7);

        nvs_handle_t h;
        if (nvs_open(NVS_NS, NVS_READWRITE, &h) != ESP_OK)
            return Err<uint32_t>(ErrorCategory::HARDWARE_FAILURE, "NVS");

        char key[12];
        snprintf(key, sizeof(key), "p%05lu", rec.patient_id);
        nvs_set_blob(h, key, &rec, sizeof(rec));
        idx_.count++;
        idx_.next_id++;
        nvs_set_blob(h, "_pidx", &idx_, sizeof(idx_));
        nvs_commit(h);
        nvs_close(h);

        ESP_LOGI(TAG, "Created #%lu: %s (%s)", rec.patient_id, rec.display_id, rec.initials);
        return Ok(rec.patient_id);
    }

    Result<PatientRecord> getPatient(uint32_t id) {
        if (!ready_) return Err<PatientRecord>(ErrorCategory::NOT_INITIALIZED, "N");
        nvs_handle_t h;
        if (nvs_open(NVS_NS, NVS_READONLY, &h) != ESP_OK)
            return Err<PatientRecord>(ErrorCategory::NOT_FOUND, "NVS");

        char key[12];
        snprintf(key, sizeof(key), "p%05lu", id);
        PatientRecord rec;
        size_t sz = sizeof(rec);
        esp_err_t err = nvs_get_blob(h, key, &rec, &sz);
        nvs_close(h);

        if (err != ESP_OK || !rec.isValid() || !rec.active)
            return Err<PatientRecord>(ErrorCategory::NOT_FOUND, "Patient not found");
        return Ok(rec);
    }

    Result<PatientRecord> findByDisplayId(const char* display_id) {
        if (!ready_) return Err<PatientRecord>(ErrorCategory::NOT_INITIALIZED, "N");
        nvs_handle_t h;
        if (nvs_open(NVS_NS, NVS_READONLY, &h) != ESP_OK)
            return Err<PatientRecord>(ErrorCategory::NOT_FOUND, "NVS");

        for (uint32_t id = 1; id < idx_.next_id; ++id) {
            char key[12];
            snprintf(key, sizeof(key), "p%05lu", id);
            PatientRecord rec;
            size_t sz = sizeof(rec);
            if (nvs_get_blob(h, key, &rec, &sz) == ESP_OK && rec.isValid() && rec.active) {
                if (strcmp(rec.display_id, display_id) == 0) {
                    nvs_close(h);
                    return Ok(rec);
                }
            }
        }
        nvs_close(h);
        return Err<PatientRecord>(ErrorCategory::NOT_FOUND, "Not found");
    }

    Result<uint32_t> listActive(PatientRecord* out, uint32_t max_n) {
        if (!ready_ || !out) return Err<uint32_t>(ErrorCategory::INVALID_PARAMETER, "Bad");
        nvs_handle_t h;
        if (nvs_open(NVS_NS, NVS_READONLY, &h) != ESP_OK) return Ok(uint32_t(0));

        uint32_t n = 0;
        for (uint32_t id = idx_.next_id; id > 0 && n < max_n; --id) {
            char key[12];
            snprintf(key, sizeof(key), "p%05lu", id - 1 + 1);
            PatientRecord rec;
            size_t sz = sizeof(rec);
            if (nvs_get_blob(h, key, &rec, &sz) == ESP_OK && rec.isValid() && rec.active)
                out[n++] = rec;
        }
        nvs_close(h);
        return Ok(n);
    }

    Result<void> incrementTestCount(uint32_t patient_id, uint32_t timestamp) {
        nvs_handle_t h;
        if (nvs_open(NVS_NS, NVS_READWRITE, &h) != ESP_OK)
            return Err(ErrorCategory::HARDWARE_FAILURE, "NVS");

        char key[12];
        snprintf(key, sizeof(key), "p%05lu", patient_id);
        PatientRecord rec;
        size_t sz = sizeof(rec);
        if (nvs_get_blob(h, key, &rec, &sz) == ESP_OK && rec.isValid()) {
            rec.test_count++;
            rec.last_test = timestamp;
            nvs_set_blob(h, key, &rec, sizeof(rec));
            nvs_commit(h);
        }
        nvs_close(h);
        return Ok();
    }

    // GDPR: soft-delete patient (preserves result linkage IDs)
    Result<void> deactivatePatient(uint32_t patient_id) {
        nvs_handle_t h;
        if (nvs_open(NVS_NS, NVS_READWRITE, &h) != ESP_OK)
            return Err(ErrorCategory::HARDWARE_FAILURE, "NVS");

        char key[12];
        snprintf(key, sizeof(key), "p%05lu", patient_id);
        PatientRecord rec;
        size_t sz = sizeof(rec);
        if (nvs_get_blob(h, key, &rec, &sz) == ESP_OK && rec.isValid()) {
            rec.active = 0;
            memset(rec.initials, 0, sizeof(rec.initials));
            memset(rec.notes, 0, sizeof(rec.notes));
            nvs_set_blob(h, key, &rec, sizeof(rec));
            nvs_commit(h);
            ESP_LOGI(TAG, "Patient #%lu deactivated (GDPR)", patient_id);
        }
        nvs_close(h);
        return Ok();
    }

    uint32_t getActiveCount() const { return idx_.count; }

private:
    PatientIndex idx_   = {};
    bool         ready_ = false;
};

static PatientManager s_patients;
PatientManager* getPatientManager() { return &s_patients; }

} // namespace phoenix
