// ==========================================================================
// FILE: src/HAL/CameraController_OV2686.cpp
// OV2686 DVP Camera — Igloo Pro Hardware Implementation
// Supertek SHWX01 module, DVP 8-bit, 1600x1200 native
// ==========================================================================

#include "phoenix/HAL/CameraController_OV2686.h"
#include <esp_log.h>
#include <esp_camera.h>
#include <esp_timer.h>
#include <esp_heap_caps.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <cstring>

namespace phoenix {

static const char* TAG = "OV2686";

static constexpr uint16_t CHIP_ID_OV2686 = 0x2686;

CameraController_OV2686::CameraController_OV2686(const OV2686Pins& pins)
    : pins_(pins) {}

CameraController_OV2686::~CameraController_OV2686() {
    deinitialize();
}

Result<void> CameraController_OV2686::initialize(const CameraConfig& cfg) {
    if (initialized_) return Ok();

    config_ = cfg;

    // ── ESP32 camera driver configuration ──────────────────────────
    camera_config_t cam_cfg = {};

    // DVP data bus
    cam_cfg.pin_d0       = pins_.d0;
    cam_cfg.pin_d1       = pins_.d1;
    cam_cfg.pin_d2       = pins_.d2;
    cam_cfg.pin_d3       = pins_.d3;
    cam_cfg.pin_d4       = pins_.d4;
    cam_cfg.pin_d5       = pins_.d5;
    cam_cfg.pin_d6       = pins_.d6;
    cam_cfg.pin_d7       = pins_.d7;

    // Control signals
    cam_cfg.pin_xclk     = pins_.xclk;
    cam_cfg.pin_pclk     = pins_.pclk;
    cam_cfg.pin_vsync    = pins_.vsync;
    cam_cfg.pin_href     = pins_.href;
    cam_cfg.pin_sccb_sda = pins_.sda;
    cam_cfg.pin_sccb_scl = pins_.scl;
    cam_cfg.pin_reset    = pins_.reset;
    cam_cfg.pin_pwdn     = pins_.pwdn;

    cam_cfg.xclk_freq_hz = 20000000;       // 20 MHz master clock
    cam_cfg.ledc_timer   = LEDC_TIMER_0;
    cam_cfg.ledc_channel = LEDC_CHANNEL_0;

    // Grayscale 640x480 for LFA strip analysis
    cam_cfg.pixel_format = PIXFORMAT_GRAYSCALE;
    cam_cfg.frame_size   = FRAMESIZE_VGA;
    cam_cfg.fb_count     = 2;               // Double-buffer
    cam_cfg.fb_location  = CAMERA_FB_IN_PSRAM;
    cam_cfg.grab_mode    = CAMERA_GRAB_LATEST;

    esp_err_t err = esp_camera_init(&cam_cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_camera_init failed: 0x%X", err);
        return Err(ErrorCategory::HARDWARE_FAILURE,
                   "Camera init failed", static_cast<uint32_t>(err));
    }

    // ── Verify chip responds ───────────────────────────────────────
    auto id_res = readChipID();
    if (id_res.is_err()) {
        esp_camera_deinit();
        return Err(ErrorCategory::HARDWARE_FAILURE, "Chip ID read failed");
    }
    ESP_LOGI(TAG, "Detected sensor: 0x%04X", id_res.value());

    // ── Apply initial exposure/gain ────────────────────────────────
    setExposure(cfg.exposure);
    setGain(cfg.gain);

    // ── Allocate persistent frame buffer in PSRAM ──────────────────
    frame_buffer_ = static_cast<uint8_t*>(
        heap_caps_malloc(IMAGE_SIZE, MALLOC_CAP_SPIRAM));
    if (!frame_buffer_) {
        esp_camera_deinit();
        return Err(ErrorCategory::MEMORY_ERROR, "PSRAM frame buf alloc failed");
    }

    initialized_ = true;
    ESP_LOGI(TAG, "Initialized: %ux%u, exp=%u, gain=%u",
             cfg.width, cfg.height, cfg.exposure, cfg.gain);
    return Ok();
}

Result<ImageBuffer> CameraController_OV2686::captureImage() {
    if (!initialized_) {
        return Err<ImageBuffer>(ErrorCategory::NOT_INITIALIZED, "Camera not init");
    }

    camera_fb_t* fb = esp_camera_fb_get();
    if (!fb) {
        return Err<ImageBuffer>(ErrorCategory::HARDWARE_FAILURE, "Capture failed");
    }

    // Copy into our persistent buffer (fb returned to driver pool)
    size_t copy_size = (fb->len < IMAGE_SIZE) ? fb->len : IMAGE_SIZE;
    memcpy(frame_buffer_, fb->buf, copy_size);

    ImageBuffer img;
    img.data      = frame_buffer_;
    img.width     = static_cast<uint16_t>(fb->width);
    img.height    = static_cast<uint16_t>(fb->height);
    img.size      = copy_size;
    img.timestamp = static_cast<uint32_t>(esp_timer_get_time() / 1000);
    img.exposure  = config_.exposure;
    img.gain      = config_.gain;

    esp_camera_fb_return(fb);

    ESP_LOGD(TAG, "Captured %ux%u (%u B) t=%u",
             img.width, img.height,
             static_cast<unsigned>(img.size), img.timestamp);
    return Ok(img);
}

Result<void> CameraController_OV2686::setExposure(uint8_t value) {
    sensor_t* s = esp_camera_sensor_get();
    if (!s) return Err(ErrorCategory::HARDWARE_FAILURE, "No sensor handle");

    s->set_exposure_ctrl(s, 0);                              // Manual
    s->set_aec_value(s, static_cast<int>(value) * 4);       // Map 0-255 → 0-1020
    config_.exposure = value;
    return Ok();
}

Result<void> CameraController_OV2686::setGain(uint8_t value) {
    sensor_t* s = esp_camera_sensor_get();
    if (!s) return Err(ErrorCategory::HARDWARE_FAILURE, "No sensor handle");

    s->set_gain_ctrl(s, 0);                                  // Manual
    s->set_agc_gain(s, static_cast<int>(value));
    config_.gain = value;
    return Ok();
}

Result<void> CameraController_OV2686::selfTest() {
    if (!initialized_) {
        return Err(ErrorCategory::NOT_INITIALIZED, "Camera not init");
    }

    // 1. Chip ID check
    auto id = readChipID();
    if (id.is_err()) {
        return Err(ErrorCategory::HARDWARE_FAILURE, "Chip ID read failed");
    }

    // 2. Test capture
    camera_fb_t* fb = esp_camera_fb_get();
    if (!fb) {
        return Err(ErrorCategory::HARDWARE_FAILURE, "Test capture failed");
    }

    // 3. Sanity check: frame not all-black or all-white
    uint32_t sum = 0;
    size_t n = fb->len > 1000 ? 1000 : fb->len;
    for (size_t i = 0; i < n; ++i) {
        sum += fb->buf[i];
    }
    esp_camera_fb_return(fb);

    float mean = static_cast<float>(sum) / static_cast<float>(n);
    if (mean < 5.0f) {
        return Err(ErrorCategory::HARDWARE_FAILURE, "Frame all black");
    }
    if (mean > 250.0f) {
        return Err(ErrorCategory::HARDWARE_FAILURE, "Frame saturated (all white)");
    }

    ESP_LOGI(TAG, "Self-test OK (mean=%.1f)", static_cast<double>(mean));
    return Ok();
}

void CameraController_OV2686::deinitialize() {
    if (initialized_) {
        esp_camera_deinit();
        if (frame_buffer_) {
            heap_caps_free(frame_buffer_);
            frame_buffer_ = nullptr;
        }
        initialized_ = false;
        ESP_LOGI(TAG, "Deinitialized");
    }
}

Result<uint16_t> CameraController_OV2686::readChipID() {
    sensor_t* s = esp_camera_sensor_get();
    if (!s) return Err<uint16_t>(ErrorCategory::HARDWARE_FAILURE, "No sensor");
    return Ok(static_cast<uint16_t>(s->id.PID));
}

Result<void> CameraController_OV2686::setTestPattern(bool enable) {
    sensor_t* s = esp_camera_sensor_get();
    if (!s) return Err(ErrorCategory::HARDWARE_FAILURE, "No sensor");
    s->set_colorbar(s, enable ? 1 : 0);
    ESP_LOGI(TAG, "Test pattern %s", enable ? "ON" : "OFF");
    return Ok();
}

} // namespace phoenix
