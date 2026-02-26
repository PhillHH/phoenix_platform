#pragma once
#include <cstdint>
#include <cstddef>
#include <cstring>
#include "nvs_flash.h"

typedef uint32_t nvs_handle_t;
#define NVS_READWRITE 1
#define NVS_READONLY  0

// Simple in-memory NVS stub for host tests
#include <map>
#include <string>
#include <vector>

namespace nvs_stub {
inline std::map<std::string, std::vector<uint8_t>>& store() {
    static std::map<std::string, std::vector<uint8_t>> s;
    return s;
}
inline void reset() { store().clear(); }
}

static inline esp_err_t nvs_open(const char*, int, nvs_handle_t* h) {
    *h = 1;
    return ESP_OK;
}

static inline esp_err_t nvs_set_blob(nvs_handle_t, const char* key,
                                      const void* data, size_t len) {
    auto& s = nvs_stub::store();
    s[key] = std::vector<uint8_t>(static_cast<const uint8_t*>(data),
                                  static_cast<const uint8_t*>(data) + len);
    return ESP_OK;
}

static inline esp_err_t nvs_get_blob(nvs_handle_t, const char* key,
                                      void* out, size_t* len) {
    auto& s = nvs_stub::store();
    auto it = s.find(key);
    if (it == s.end()) return -1;
    size_t available = it->second.size();
    if (*len < available) return -1;
    memcpy(out, it->second.data(), available);
    *len = available;
    return ESP_OK;
}

static inline esp_err_t nvs_set_u32(nvs_handle_t, const char* key, uint32_t val) {
    auto& s = nvs_stub::store();
    auto* p = reinterpret_cast<const uint8_t*>(&val);
    s[key] = std::vector<uint8_t>(p, p + sizeof(val));
    return ESP_OK;
}

static inline esp_err_t nvs_get_u32(nvs_handle_t, const char* key, uint32_t* out) {
    auto& s = nvs_stub::store();
    auto it = s.find(key);
    if (it == s.end()) return -1;
    memcpy(out, it->second.data(), sizeof(uint32_t));
    return ESP_OK;
}

static inline esp_err_t nvs_commit(nvs_handle_t) { return ESP_OK; }
static inline void nvs_close(nvs_handle_t) {}
