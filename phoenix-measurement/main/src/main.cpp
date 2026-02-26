// ==========================================================================
// Phoenix v108.0 "Chimera" — ESP32 #2 Measurement MCU Entry Point
// IEC 62304 Class C · ESP-IDF · Igloo Pro Hardware
// ==========================================================================
#include "phoenix/Core/Result.h"
#include "phoenix/Core/ServiceLocator.h"
#include "phoenix/Core/SafetyManager.h"
#include "phoenix/HAL/CameraController_OV2686.h"
#include "phoenix/HAL/Interfaces.h"
#include "phoenix/Analysis/Interfaces.h"
#include "phoenix/Analysis/MeasurementPipeline.h"
#include "phoenix/Services/CalibrationService.h"
#include "phoenix/Services/BenchmarkValidator.h"
#include "phoenix/IPC/UartBridge.h"
#include <esp_log.h>
#include <esp_system.h>
#include <nvs_flash.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

using namespace phoenix;
static const char* TAG = "MAIN";

// Factory functions (defined in respective .cpp files)
namespace phoenix {
    IProfileExtractor*  getDefaultProfileExtractor();
    IBaselineEstimator* getDefaultBaselineEstimator();
    IPeakFinder*        getDefaultPeakFinder();
    ILEDController*     getIglooLEDController();
}

// ─── Globals ──────────────────────────────────────────────────────────
static SafetyManager            g_safety;
static CameraController_OV2686  g_camera;
static CalibrationService       g_cal;
static BenchmarkValidator       g_bench;
static UartBridge               g_uart;
static MeasurementPipeline*     g_pipeline = nullptr;

// ─── Register clinical benchmarks ─────────────────────────────────────
static void init_benchmarks() {
    auto reg = [&](const char* name, float lo, float hi,
                   const char* unit, const char* lo_i, const char* hi_i) {
        BenchmarkData b = {};
        b.analyte = name; b.method = "LFA";
        b.reference.low = lo; b.reference.high = hi;
        b.reference.unit = unit;
        b.reference.interpretation_low  = lo_i;
        b.reference.interpretation_mid  = "NORMAL";
        b.reference.interpretation_high = hi_i;
        g_bench.registerBenchmark(b);
    };
    reg("CRP",  0.0f,  5.0f, "mg/L",  "NORMAL",    "ELEVATED");
    reg("TSH",  0.4f,  4.0f, "mIU/L", "LOW",       "HIGH");
    reg("VitD", 30.0f, 100.0f,"ng/mL","DEFICIENT",  "EXCESS");
    reg("cTnI", 0.0f,  0.04f,"ng/mL", "NEGATIVE",   "POSITIVE");
    reg("PCT",  0.0f,  0.5f, "ng/mL", "NORMAL",     "ELEVATED");
    reg("HbA1c",4.0f,  5.6f, "%",     "LOW",        "DIABETIC");
    ESP_LOGI(TAG, "Benchmarks: %u registered", g_bench.getAvailableCount());
}

// ─── Handle one UART command ──────────────────────────────────────────
static void handle_command(IpcCommand cmd, uint8_t* payload, size_t len) {
    switch (cmd) {
    case IpcCommand::PING:
        g_uart.send(IpcCommand::PONG);
        break;

    case IpcCommand::CMD_START_MEASUREMENT: {
        g_uart.send(IpcCommand::ACK);
        PipelineConfig cfg;
        cfg.auto_roi = true; cfg.num_captures = 3; cfg.led_intensity = 200;
        g_pipeline->setProgressCallback(
            [](const PipelineProgress& p, void* ctx) {
                static_cast<UartBridge*>(ctx)->sendProgress(
                    p.percentage, p.message.c_str());
            }, &g_uart);
        auto res = g_pipeline->runMeasurement(cfg);
        if (res.is_ok()) g_uart.sendResult(res.value());
        else g_uart.sendError(res.error().category, res.error().message);
        break;
    }
    case IpcCommand::CMD_CANCEL_MEASUREMENT:
        g_pipeline->cancel();
        g_uart.send(IpcCommand::ACK);
        break;

    case IpcCommand::CMD_RUN_DIAGNOSTICS: {
        auto d = g_safety.performPowerOnSelfTest();
        if (d.is_ok())
            g_uart.send(IpcCommand::DIAGNOSTICS_REPORT,
                        &d.value(), sizeof(DiagnosticsReport));
        break;
    }
    case IpcCommand::CMD_START_CALIBRATION: {
        char analyte[32] = {};
        memcpy(analyte, payload, len < 31 ? len : 31);
        g_cal.startCalibrationWorkflow(analyte);
        g_uart.send(IpcCommand::ACK);
        break;
    }
    case IpcCommand::CMD_ADD_CAL_POINT: {
        if (len >= 8) {
            float conc, sig;
            memcpy(&conc, payload, 4);
            memcpy(&sig, payload + 4, 4);
            g_cal.addCalibrationPoint(conc, sig);
            g_uart.send(IpcCommand::ACK);
        }
        break;
    }
    case IpcCommand::CMD_FINISH_CALIBRATION: {
        auto fit = g_cal.fitCurve();
        if (fit.is_ok()) {
            g_cal.validateCalibration();
            g_cal.saveCalibration("active");
        }
        g_uart.send(IpcCommand::ACK);
        break;
    }
    default:
        ESP_LOGW(TAG, "Unknown cmd: 0x%02X", static_cast<int>(cmd));
        g_uart.send(IpcCommand::NACK);
    }
}

// ─── Main Loop Task ───────────────────────────────────────────────────
static void main_task(void* /*arg*/) {
    // ── 1. NVS ────────────────────────────────────────────────────
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES ||
        err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }

    // ── 2. Safety POST ────────────────────────────────────────────
    auto post = g_safety.performPowerOnSelfTest();
    if (post.is_err()) {
        ESP_LOGE(TAG, "POST FAILED: %s", post.error().message);
        // Continue with degraded mode — report to UI MCU
    }

    // ── 3. UART IPC ───────────────────────────────────────────────
    auto uart_res = g_uart.initialize();
    if (uart_res.is_err()) {
        ESP_LOGE(TAG, "UART init failed: %s", uart_res.error().message);
    }

    // ── 4. Camera ─────────────────────────────────────────────────
    CameraConfig cam_cfg;
    cam_cfg.width    = IMAGE_WIDTH;
    cam_cfg.height   = IMAGE_HEIGHT;
    cam_cfg.exposure = 128;
    cam_cfg.gain     = 64;
    auto cam_res = g_camera.initialize(cam_cfg);
    if (cam_res.is_err()) {
        ESP_LOGE(TAG, "Camera init failed: %s", cam_res.error().message);
        g_uart.sendError(ErrorCategory::HARDWARE_FAILURE, "Camera init failed");
    }

    // ── 5. LED ────────────────────────────────────────────────────
    ILEDController* led = getIglooLEDController();
    led->initialize();

    // ── 6. Benchmarks ─────────────────────────────────────────────
    init_benchmarks();

    // ── 7. Try loading saved calibration ──────────────────────────
    g_cal.loadCalibration("active");

    // ── 8. Assemble Pipeline ──────────────────────────────────────
    static MeasurementPipeline pipeline(
        &g_camera,
        led,
        getDefaultProfileExtractor(),
        getDefaultBaselineEstimator(),
        getDefaultPeakFinder(),
        &g_cal,
        &g_bench);
    g_pipeline = &pipeline;

    // ── 9. Register services ──────────────────────────────────────
    auto& svc = ServiceLocator::getInstance();
    svc.registerService<ICameraController>("Camera",  &g_camera);
    svc.registerService<ILEDController>("LED",        led);
    svc.registerService<CalibrationService>("Cal",    &g_cal);
    svc.registerService<BenchmarkValidator>("Bench",  &g_bench);
    svc.registerService<SafetyManager>("Safety",      &g_safety);
    svc.registerService<UartBridge>("UART",            &g_uart);

    // ── 10. Signal ready ──────────────────────────────────────────
    ESP_LOGI(TAG, "═══════════════════════════════════════════");
    ESP_LOGI(TAG, " Phoenix v108.0 Measurement MCU READY");
    ESP_LOGI(TAG, " Heap: %.1f kB free",
             static_cast<double>(esp_get_free_heap_size()) / 1024.0);
    ESP_LOGI(TAG, " Services: %u registered", svc.count());
    ESP_LOGI(TAG, "═══════════════════════════════════════════");

    // Status LED: green = ready
    LEDConfig led_ready;
    led_ready.mode = LEDMode::STATUS_RGB;
    led_ready.r = 0; led_ready.g = 30; led_ready.b = 0;
    led->setMode(led_ready);

    // ── Main Loop ─────────────────────────────────────────────────
    g_safety.initWatchdog(10000);  // 10s watchdog

    while (true) {
        g_safety.feedWatchdog();

        // Process UART commands from UI MCU
        if (g_uart.hasData()) {
            uint8_t payload[256] = {};
            size_t plen = sizeof(payload);
            auto cmd = g_uart.receive(payload, &plen, 100);
            if (cmd.is_ok()) {
                handle_command(cmd.value(), payload, plen);
            }
        }

        // Periodic safety checks (every 5s)
        static uint32_t last_check = 0;
        uint32_t now = static_cast<uint32_t>(
            esp_timer_get_time() / 1000000ULL);
        if (now - last_check >= 5) {
            g_safety.checkMemoryIntegrity();
            g_safety.monitorTemperature();
            g_safety.checkVoltages();
            last_check = now;
        }

        vTaskDelay(pdMS_TO_TICKS(10));  // 100 Hz main loop
    }
}

// ─── ESP-IDF Entry Point ──────────────────────────────────────────────
extern "C" void app_main() {
    ESP_LOGI(TAG, "Phoenix v108.0 Chimera — Measurement MCU booting...");

    // Main task needs large stack for measurement pipeline
    xTaskCreatePinnedToCore(
        main_task,
        "phoenix_main",
        16384,       // 16 kB stack
        nullptr,
        5,           // Priority 5
        nullptr,
        1            // Run on Core 1 (Core 0 for WiFi/BT on UI MCU)
    );
}
