#pragma once
#include <cstdint>
static inline uint32_t esp_get_free_heap_size() { return 256 * 1024; }
static inline uint32_t esp_get_minimum_free_heap_size() { return 128 * 1024; }
static inline const char* esp_get_idf_version() { return "host-stub"; }
