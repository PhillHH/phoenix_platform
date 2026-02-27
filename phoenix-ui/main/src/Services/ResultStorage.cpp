// ==========================================================================
// FILE: src/Services/ResultStorage.cpp
// Phoenix v108.0 — Persistent measurement result storage
// NVS ring buffer with patient linkage, LIMS export tracking, search
// IEC 62304: Full measurement traceability (REQ-TRACE-001)
// ==========================================================================
#include "phoenix/Core/Result.h"
#include "phoenix/Core/FixedString.h"
#include "phoenix/Core/MeasurementTypes.h"
#include <esp_log.h>
#include <nvs_flash.h>
#include <nvs.h>
#include <cstring>
#include <cmath>

namespace phoenix {

static const char* TAG = "ResultStore";

static constexpr size_t MAX_STORED_RESULTS = 500;
static constexpr char   NVS_NS[]           = "results";

struct __attribute__((packed)) StoredResult {
    static constexpr uint16_t MAGIC   = 0x5052;
    static constexpr uint16_t VERSION = 2;

    uint16_t magic           = MAGIC;
    uint16_t version         = VERSION;
    uint32_t record_id       = 0;
    uint32_t timestamp       = 0;
    uint32_t patient_id      = 0;
    float    concentration   = 0.0f;
    float    signal          = 0.0f;
    float    control_signal  = 0.0f;
    float    tc_ratio        = 0.0f;
    float    confidence      = 0.0f;
    float    ref_low         = 0.0f;
    float    ref_high        = 0.0f;
    uint8_t  qc_flags        = 0;
    char     analyte[32]     = {};
    char     unit[16]        = {};
    char     interpretation[32] = {};
    char     operator_id[16] = {};
    char     lot_number[24]  = {};
    uint8_t  exported        = 0;
    uint8_t  _pad[3]         = {};

    bool isValid() const { return magic == MAGIC && version == VERSION && record_id > 0; }
};

struct __attribute__((packed)) StorageIndex {
    uint32_t write_idx    = 0;
    uint32_t total_stored = 0;
    uint32_t next_id      = 1;
};

class ResultStorage {
public:
    Result<void> initialize() {
        nvs_handle_t h;
        esp_err_t err = nvs_open(NVS_NS, NVS_READWRITE, &h);
        if (err != ESP_OK) return Err(ErrorCategory::HARDWARE_FAILURE, "NVS open");

        size_t sz = sizeof(StorageIndex);
        if (nvs_get_blob(h, "_idx", &idx_, &sz) != ESP_OK) {
            idx_ = {};
            nvs_set_blob(h, "_idx", &idx_, sizeof(idx_));
            nvs_commit(h);
        }
        nvs_close(h);
        ready_ = true;
        ESP_LOGI(TAG, "Ready: %lu records, next_id=%lu", idx_.total_stored, idx_.next_id);
        return Ok();
    }

    Result<uint32_t> store(const MeasurementResult& r, uint32_t patient = 0,
                           const char* op = nullptr, const char* lot = nullptr) {
        if (!ready_) return Err<uint32_t>(ErrorCategory::NOT_INITIALIZED, "Not init");

        StoredResult rec = {};
        rec.record_id = idx_.next_id;
        rec.timestamp = r.timestamp;
        rec.patient_id = patient;
        rec.concentration = r.concentration_ng_ml;
        rec.signal = r.signal_intensity;
        rec.control_signal = r.control_line_signal;
        rec.tc_ratio = r.test_control_ratio;
        rec.confidence = r.confidence;
        rec.ref_low = r.reference_low;
        rec.ref_high = r.reference_high;
        rec.qc_flags = static_cast<uint8_t>(r.qc_flags);
        strncpy(rec.analyte, r.analyte_name.c_str(), 31);
        strncpy(rec.unit, r.unit.c_str(), 15);
        strncpy(rec.interpretation, r.interpretation.c_str(), 31);
        if (op) strncpy(rec.operator_id, op, 15);
        if (lot) strncpy(rec.lot_number, lot, 23);

        nvs_handle_t h;
        if (nvs_open(NVS_NS, NVS_READWRITE, &h) != ESP_OK)
            return Err<uint32_t>(ErrorCategory::HARDWARE_FAILURE, "NVS");

        char key[12];
        snprintf(key, sizeof(key), "r%04lu", idx_.write_idx % MAX_STORED_RESULTS);
        nvs_set_blob(h, key, &rec, sizeof(rec));
        idx_.write_idx++;
        idx_.total_stored++;
        idx_.next_id++;
        nvs_set_blob(h, "_idx", &idx_, sizeof(idx_));
        nvs_commit(h);
        nvs_close(h);

        ESP_LOGI(TAG, "#%lu: %s %.2f %s", rec.record_id, rec.analyte,
                 static_cast<double>(rec.concentration), rec.interpretation);
        return Ok(rec.record_id);
    }

    Result<StoredResult> getById(uint32_t id) {
        return scanFor([id](const StoredResult& r) { return r.record_id == id; });
    }

    Result<uint32_t> getRecent(StoredResult* out, uint32_t max_n) {
        if (!ready_ || !out) return Err<uint32_t>(ErrorCategory::INVALID_PARAMETER, "Bad");
        nvs_handle_t h;
        if (nvs_open(NVS_NS, NVS_READONLY, &h) != ESP_OK) return Ok(uint32_t(0));

        uint32_t n = 0;
        for (uint32_t i = idx_.write_idx; i > 0 && n < max_n; --i) {
            char key[12];
            snprintf(key, sizeof(key), "r%04lu", (i - 1) % MAX_STORED_RESULTS);
            StoredResult rec;
            size_t sz = sizeof(rec);
            if (nvs_get_blob(h, key, &rec, &sz) == ESP_OK && rec.isValid())
                out[n++] = rec;
        }
        nvs_close(h);
        return Ok(n);
    }

    Result<uint32_t> getByPatient(uint32_t pid, StoredResult* out, uint32_t max_n) {
        if (!ready_) return Err<uint32_t>(ErrorCategory::NOT_INITIALIZED, "N");
        nvs_handle_t h;
        if (nvs_open(NVS_NS, NVS_READONLY, &h) != ESP_OK) return Ok(uint32_t(0));

        uint32_t n = 0, start = ringStart();
        for (uint32_t i = idx_.write_idx; i > start && n < max_n; --i) {
            char key[12];
            snprintf(key, sizeof(key), "r%04lu", (i - 1) % MAX_STORED_RESULTS);
            StoredResult rec;
            size_t sz = sizeof(rec);
            if (nvs_get_blob(h, key, &rec, &sz) == ESP_OK && rec.isValid())
                if (rec.patient_id == pid) out[n++] = rec;
        }
        nvs_close(h);
        return Ok(n);
    }

    Result<void> markExported(uint32_t id) {
        return mutateRecord(id, [](StoredResult& r) { r.exported = 1; });
    }

    uint32_t getPendingExportCount() {
        if (!ready_) return 0;
        nvs_handle_t h;
        if (nvs_open(NVS_NS, NVS_READONLY, &h) != ESP_OK) return 0;
        uint32_t pending = 0, start = ringStart();
        for (uint32_t i = start; i < idx_.write_idx; ++i) {
            char key[12];
            snprintf(key, sizeof(key), "r%04lu", i % MAX_STORED_RESULTS);
            StoredResult rec;
            size_t sz = sizeof(rec);
            if (nvs_get_blob(h, key, &rec, &sz) == ESP_OK && rec.isValid() && !rec.exported)
                pending++;
        }
        nvs_close(h);
        return pending;
    }

    Result<void> eraseAll() {
        nvs_handle_t h;
        if (nvs_open(NVS_NS, NVS_READWRITE, &h) != ESP_OK)
            return Err(ErrorCategory::HARDWARE_FAILURE, "NVS");
        nvs_erase_all(h);
        idx_ = {};
        nvs_set_blob(h, "_idx", &idx_, sizeof(idx_));
        nvs_commit(h);
        nvs_close(h);
        ESP_LOGW(TAG, "All results erased");
        return Ok();
    }

    uint32_t getTotalCount()  const { return idx_.total_stored; }
    uint32_t getStoredCount() const {
        return idx_.write_idx > MAX_STORED_RESULTS ? MAX_STORED_RESULTS : idx_.write_idx;
    }

private:
    StorageIndex idx_   = {};
    bool         ready_ = false;

    uint32_t ringStart() const {
        return (idx_.write_idx > static_cast<uint32_t>(MAX_STORED_RESULTS))
            ? static_cast<uint32_t>(idx_.write_idx - MAX_STORED_RESULTS) : 0u;
    }

    template <typename Pred>
    Result<StoredResult> scanFor(Pred pred) {
        if (!ready_) return Err<StoredResult>(ErrorCategory::NOT_INITIALIZED, "N");
        nvs_handle_t h;
        if (nvs_open(NVS_NS, NVS_READONLY, &h) != ESP_OK)
            return Err<StoredResult>(ErrorCategory::NOT_FOUND, "NVS");
        uint32_t start = ringStart();
        for (uint32_t i = start; i < idx_.write_idx; ++i) {
            char key[12];
            snprintf(key, sizeof(key), "r%04lu", i % MAX_STORED_RESULTS);
            StoredResult rec;
            size_t sz = sizeof(rec);
            if (nvs_get_blob(h, key, &rec, &sz) == ESP_OK && rec.isValid() && pred(rec)) {
                nvs_close(h);
                return Ok(rec);
            }
        }
        nvs_close(h);
        return Err<StoredResult>(ErrorCategory::NOT_FOUND, "Not found");
    }

    Result<void> mutateRecord(uint32_t id, void(*fn)(StoredResult&)) {
        nvs_handle_t h;
        if (nvs_open(NVS_NS, NVS_READWRITE, &h) != ESP_OK)
            return Err(ErrorCategory::HARDWARE_FAILURE, "NVS");
        uint32_t start = ringStart();
        for (uint32_t i = start; i < idx_.write_idx; ++i) {
            char key[12];
            snprintf(key, sizeof(key), "r%04lu", i % MAX_STORED_RESULTS);
            StoredResult rec;
            size_t sz = sizeof(rec);
            if (nvs_get_blob(h, key, &rec, &sz) == ESP_OK && rec.record_id == id) {
                fn(rec);
                nvs_set_blob(h, key, &rec, sizeof(rec));
                nvs_commit(h);
                nvs_close(h);
                return Ok();
            }
        }
        nvs_close(h);
        return Err(ErrorCategory::NOT_FOUND, "Record not found");
    }
};

static ResultStorage s_storage;
ResultStorage* getResultStorage() { return &s_storage; }

} // namespace phoenix
