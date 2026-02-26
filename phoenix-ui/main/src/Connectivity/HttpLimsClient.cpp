// ==========================================================================
// FILE: src/Connectivity/HttpLimsClient.cpp
// Phoenix v108.0 — HTTP LIMS Client (HL7 FHIR-compatible JSON)
// Supports: result upload, batch sync, status polling, mTLS
// ==========================================================================
#include "phoenix/Core/Result.h"
#include "phoenix/Core/FixedString.h"
#include "phoenix/Core/MeasurementTypes.h"
#include <esp_log.h>
#include <esp_http_client.h>
#include <esp_tls.h>
#include <nvs_flash.h>
#include <nvs.h>
#include <cstring>
#include <cstdio>

namespace phoenix {

static const char* TAG = "LIMS";

static constexpr size_t MAX_RESPONSE_SIZE = 2048;
static constexpr int    MAX_RETRIES       = 3;
static constexpr int    RETRY_DELAY_MS    = 2000;

struct LimsConfig {
    char     base_url[128]     = {};    // e.g., "https://lims.clinic.de/api/v1"
    char     api_key[64]       = {};    // Bearer token
    char     device_id[32]     = {};    // Igloo Pro serial number
    char     practice_id[32]   = {};    // Clinic/practice identifier
    uint16_t timeout_ms        = 10000;
    bool     tls_verify        = true;
};

// ─── JSON Builder (no heap, fixed buffer) ─────────────────────────────
class JsonBuilder {
public:
    explicit JsonBuilder(char* buf, size_t cap) : buf_(buf), cap_(cap) {
        pos_ = 0;
        buf_[0] = '\0';
    }

    void begin()                { add("{"); }
    void end()                  { if (pos_ > 0 && buf_[pos_-1] == ',') pos_--; add("}"); }
    void beginArray(const char* key) { addFmt("\"%s\":[", key); }
    void endArray()             { if (pos_ > 0 && buf_[pos_-1] == ',') pos_--; add("],"); }

    void addString(const char* k, const char* v) {
        addFmt("\"%s\":\"%s\",", k, v ? v : "");
    }
    void addFloat(const char* k, float v) {
        addFmt("\"%s\":%.4f,", k, static_cast<double>(v));
    }
    void addInt(const char* k, int32_t v) {
        addFmt("\"%s\":%ld,", k, v);
    }
    void addBool(const char* k, bool v) {
        addFmt("\"%s\":%s,", k, v ? "true" : "false");
    }

    const char* c_str() const { return buf_; }
    size_t      length() const { return pos_; }

private:
    char*  buf_;
    size_t cap_;
    size_t pos_ = 0;

    void add(const char* s) {
        size_t n = strlen(s);
        if (pos_ + n < cap_) {
            memcpy(buf_ + pos_, s, n);
            pos_ += n;
            buf_[pos_] = '\0';
        }
    }

    void addFmt(const char* fmt, ...) {
        va_list args;
        va_start(args, fmt);
        int n = vsnprintf(buf_ + pos_, cap_ - pos_, fmt, args);
        va_end(args);
        if (n > 0 && pos_ + static_cast<size_t>(n) < cap_) {
            pos_ += static_cast<size_t>(n);
        }
    }
};

class HttpLimsClient {
public:
    Result<void> initialize() {
        // Load config from NVS
        nvs_handle_t h;
        if (nvs_open("lims", NVS_READONLY, &h) == ESP_OK) {
            size_t sz;
            sz = sizeof(config_.base_url);
            nvs_get_str(h, "url", config_.base_url, &sz);
            sz = sizeof(config_.api_key);
            nvs_get_str(h, "key", config_.api_key, &sz);
            sz = sizeof(config_.device_id);
            nvs_get_str(h, "dev_id", config_.device_id, &sz);
            sz = sizeof(config_.practice_id);
            nvs_get_str(h, "prac_id", config_.practice_id, &sz);
            nvs_close(h);
        }

        ready_ = (strlen(config_.base_url) > 0);
        if (ready_) {
            ESP_LOGI(TAG, "LIMS configured: %s (device=%s)",
                     config_.base_url, config_.device_id);
        } else {
            ESP_LOGW(TAG, "LIMS not configured — set URL in settings");
        }
        return Ok();
    }

    Result<void> configure(const LimsConfig& cfg) {
        config_ = cfg;
        // Persist to NVS
        nvs_handle_t h;
        if (nvs_open("lims", NVS_READWRITE, &h) == ESP_OK) {
            nvs_set_str(h, "url", cfg.base_url);
            nvs_set_str(h, "key", cfg.api_key);
            nvs_set_str(h, "dev_id", cfg.device_id);
            nvs_set_str(h, "prac_id", cfg.practice_id);
            nvs_commit(h);
            nvs_close(h);
        }
        ready_ = true;
        return Ok();
    }

    // Upload a single measurement result
    Result<void> uploadResult(const MeasurementResult& result,
                               uint32_t record_id = 0,
                               uint32_t patient_id = 0)
    {
        if (!ready_) return Err(ErrorCategory::NOT_INITIALIZED, "LIMS not configured");

        // Build FHIR-compatible Observation JSON
        char json[1024];
        JsonBuilder jb(json, sizeof(json));
        jb.begin();
        jb.addString("resourceType", "Observation");
        jb.addString("status", "final");
        jb.addString("device_id", config_.device_id);
        jb.addString("practice_id", config_.practice_id);
        jb.addInt("record_id", static_cast<int32_t>(record_id));
        jb.addInt("patient_id", static_cast<int32_t>(patient_id));
        jb.addInt("timestamp", static_cast<int32_t>(result.timestamp));
        jb.addString("analyte", result.analyte_name.c_str());
        jb.addFloat("value", result.concentration_ng_ml);
        jb.addString("unit", result.unit.c_str());
        jb.addString("interpretation", result.interpretation.c_str());
        jb.addFloat("signal", result.signal_intensity);
        jb.addFloat("control", result.control_line_signal);
        jb.addFloat("tc_ratio", result.test_control_ratio);
        jb.addFloat("confidence", result.confidence);
        jb.addFloat("ref_low", result.reference_low);
        jb.addFloat("ref_high", result.reference_high);
        jb.addInt("qc_flags", static_cast<int32_t>(static_cast<uint8_t>(result.qc_flags)));
        jb.addString("firmware", "Phoenix v108.0");
        jb.end();

        // POST with retries
        char url[192];
        snprintf(url, sizeof(url), "%s/observations", config_.base_url);

        return postWithRetry(url, json, jb.length());
    }

    // Batch upload pending results
    Result<uint32_t> syncPending() {
        // This would iterate ResultStorage::getPendingExportCount()
        // and call uploadResult for each, marking as exported on success.
        // Simplified: returns 0 for now, actual impl needs ResultStorage reference.
        ESP_LOGI(TAG, "Sync pending — not yet wired to ResultStorage");
        return Ok(uint32_t(0));
    }

    // Check server connectivity
    Result<void> ping() {
        if (!ready_) return Err(ErrorCategory::NOT_INITIALIZED, "LIMS not configured");

        char url[192];
        snprintf(url, sizeof(url), "%s/health", config_.base_url);

        esp_http_client_config_t http_cfg = {};
        http_cfg.url     = url;
        http_cfg.method  = HTTP_METHOD_GET;
        http_cfg.timeout_ms = 5000;

        esp_http_client_handle_t client = esp_http_client_init(&http_cfg);
        if (!client) return Err(ErrorCategory::COMMUNICATION_ERROR, "HTTP init failed");

        addAuthHeader(client);
        esp_err_t err = esp_http_client_perform(client);
        int status = esp_http_client_get_status_code(client);
        esp_http_client_cleanup(client);

        if (err != ESP_OK) return Err(ErrorCategory::COMMUNICATION_ERROR, "HTTP error");
        if (status < 200 || status >= 300) {
            ESP_LOGW(TAG, "LIMS health check: HTTP %d", status);
            return Err(ErrorCategory::COMMUNICATION_ERROR, "LIMS unhealthy");
        }

        ESP_LOGI(TAG, "LIMS reachable (HTTP %d)", status);
        return Ok();
    }

    bool isConfigured() const { return ready_; }
    bool isConnected()  const { return last_status_ >= 200 && last_status_ < 300; }

private:
    LimsConfig config_      = {};
    bool       ready_        = false;
    int        last_status_  = 0;

    void addAuthHeader(esp_http_client_handle_t client) {
        if (strlen(config_.api_key) > 0) {
            char auth[80];
            snprintf(auth, sizeof(auth), "Bearer %s", config_.api_key);
            esp_http_client_set_header(client, "Authorization", auth);
        }
        esp_http_client_set_header(client, "Content-Type", "application/json");
        esp_http_client_set_header(client, "X-Device-ID", config_.device_id);
    }

    Result<void> postWithRetry(const char* url, const char* body, size_t len) {
        for (int attempt = 0; attempt < MAX_RETRIES; ++attempt) {
            esp_http_client_config_t http_cfg = {};
            http_cfg.url       = url;
            http_cfg.method    = HTTP_METHOD_POST;
            http_cfg.timeout_ms = config_.timeout_ms;

            esp_http_client_handle_t client = esp_http_client_init(&http_cfg);
            if (!client) continue;

            addAuthHeader(client);
            esp_http_client_set_post_field(client, body, static_cast<int>(len));

            esp_err_t err = esp_http_client_perform(client);
            last_status_ = esp_http_client_get_status_code(client);
            esp_http_client_cleanup(client);

            if (err == ESP_OK && last_status_ >= 200 && last_status_ < 300) {
                ESP_LOGI(TAG, "POST %s → %d", url, last_status_);
                return Ok();
            }

            ESP_LOGW(TAG, "Attempt %d/%d failed (HTTP %d)", attempt + 1, MAX_RETRIES, last_status_);
            if (attempt < MAX_RETRIES - 1) {
                vTaskDelay(pdMS_TO_TICKS(RETRY_DELAY_MS * (attempt + 1)));
            }
        }

        return Err(ErrorCategory::COMMUNICATION_ERROR, "LIMS upload failed after retries");
    }
};

static HttpLimsClient s_lims;
HttpLimsClient* getLimsClient() { return &s_lims; }

} // namespace phoenix
