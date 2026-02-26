#pragma once
#include <cstdint>
typedef int esp_err_t;
#define ESP_OK 0
#define ESP_ERR_NVS_NO_FREE_PAGES  0x1100
#define ESP_ERR_NVS_NEW_VERSION_FOUND 0x1101
static inline esp_err_t nvs_flash_init() { return ESP_OK; }
static inline esp_err_t nvs_flash_erase() { return ESP_OK; }
