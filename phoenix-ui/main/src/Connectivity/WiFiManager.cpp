// ==========================================================================
// FILE: src/Connectivity/WiFiManager.cpp
// WiFi Station mode — connect, reconnect, scan
// ==========================================================================
#include "phoenix/Core/Result.h"
#include "phoenix/Core/FixedString.h"
#include <esp_log.h>
#include <esp_wifi.h>
#include <esp_event.h>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <cstring>

namespace phoenix {
static const char* TAG = "WiFi";
static EventGroupHandle_t s_wifi_events = nullptr;
static constexpr int CONNECTED_BIT = BIT0;
static constexpr int FAIL_BIT      = BIT1;
static int s_retry_count = 0;

static void wifi_event_handler(void*, esp_event_base_t base,
                                int32_t id, void*) {
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_count < 5) {
            esp_wifi_connect();
            s_retry_count++;
            ESP_LOGI(TAG, "Reconnecting (%d/5)...", s_retry_count);
        } else {
            xEventGroupSetBits(s_wifi_events, FAIL_BIT);
        }
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        s_retry_count = 0;
        auto* ev = static_cast<ip_event_got_ip_t*>(nullptr); // Placeholder
        ESP_LOGI(TAG, "Connected");
        xEventGroupSetBits(s_wifi_events, CONNECTED_BIT);
    }
}

class WiFiManager {
public:
    Result<void> initialize() {
        s_wifi_events = xEventGroupCreate();
        ESP_ERROR_CHECK(esp_netif_init());
        ESP_ERROR_CHECK(esp_event_loop_create_default());
        esp_netif_create_default_wifi_sta();

        wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
        ESP_ERROR_CHECK(esp_wifi_init(&cfg));

        esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                   &wifi_event_handler, nullptr);
        esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                   &wifi_event_handler, nullptr);

        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
        initialized_ = true;
        ESP_LOGI(TAG, "WiFi initialized");
        return Ok();
    }

    Result<void> connect(const char* ssid, const char* password,
                         uint32_t timeout_ms = 10000) {
        if (!initialized_) return Err(ErrorCategory::NOT_INITIALIZED, "WiFi not init");

        wifi_config_t wifi_cfg = {};
        strncpy(reinterpret_cast<char*>(wifi_cfg.sta.ssid),
                ssid, sizeof(wifi_cfg.sta.ssid) - 1);
        strncpy(reinterpret_cast<char*>(wifi_cfg.sta.password),
                password, sizeof(wifi_cfg.sta.password) - 1);
        wifi_cfg.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg));
        ESP_ERROR_CHECK(esp_wifi_start());

        s_retry_count = 0;
        esp_wifi_connect();

        EventBits_t bits = xEventGroupWaitBits(
            s_wifi_events, CONNECTED_BIT | FAIL_BIT,
            pdTRUE, pdFALSE, pdMS_TO_TICKS(timeout_ms));

        if (bits & CONNECTED_BIT) {
            connected_ = true;
            ESP_LOGI(TAG, "Connected to %s", ssid);
            return Ok();
        }
        return Err(ErrorCategory::COMMUNICATION_ERROR, "WiFi connect failed");
    }

    void disconnect() {
        esp_wifi_disconnect();
        connected_ = false;
    }

    bool isConnected() const { return connected_; }

private:
    bool initialized_ = false;
    bool connected_    = false;
};

static WiFiManager s_wifi;
WiFiManager* getWiFiManager() { return &s_wifi; }

} // namespace phoenix
