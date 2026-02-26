// ==========================================================================
// FILE: src/IPC/CommandProtocol.cpp
// Dispatches incoming UART commands from ESP32 #1 (UI MCU)
// ==========================================================================

#include "phoenix/IPC/UartBridge.h"
#include "phoenix/Analysis/MeasurementPipeline.h"
#include "phoenix/Services/CalibrationService.h"
#include "phoenix/Core/SafetyManager.h"
#include <esp_log.h>

namespace phoenix {

static const char* TAG = "CmdProto";

class CommandDispatcher {
public:
    CommandDispatcher(
        UartBridge*          uart,
        MeasurementPipeline* pipeline,
        CalibrationService*  cal,
        SafetyManager*       safety)
        : uart_(uart), pipeline_(pipeline), cal_(cal), safety_(safety) {}

    // Process one incoming command (call from main loop)
    Result<void> processOne(uint32_t timeout_ms = 100) {
        if (!uart_ || !uart_->hasData()) return Ok();

        uint8_t payload[256] = {};
        size_t  len = sizeof(payload);
        auto cmd_res = uart_->receive(payload, &len, timeout_ms);

        if (cmd_res.is_err()) {
            // Timeout is normal during idle
            if (cmd_res.error().category == ErrorCategory::TIMEOUT) return Ok();
            return Err(cmd_res.error());
        }

        IpcCommand cmd = cmd_res.value();
        ESP_LOGD(TAG, "Received cmd: 0x%02X (%u bytes)",
                 static_cast<int>(cmd), len);

        switch (cmd) {
        case IpcCommand::PING:
            return uart_->send(IpcCommand::PONG);

        case IpcCommand::CMD_START_MEASUREMENT:
            return handleStartMeasurement(payload, len);

        case IpcCommand::CMD_CANCEL_MEASUREMENT:
            if (pipeline_) pipeline_->cancel();
            return uart_->send(IpcCommand::ACK);

        case IpcCommand::CMD_START_CALIBRATION:
            return handleStartCalibration(payload, len);

        case IpcCommand::CMD_ADD_CAL_POINT:
            return handleAddCalPoint(payload, len);

        case IpcCommand::CMD_FINISH_CALIBRATION:
            return handleFinishCalibration();

        case IpcCommand::CMD_RUN_DIAGNOSTICS:
            return handleDiagnostics();

        case IpcCommand::CMD_GET_STATUS:
            return handleGetStatus();

        case IpcCommand::CMD_SHUTDOWN:
            ESP_LOGI(TAG, "Shutdown requested by UI MCU");
            safety_->emergencyShutdown("UI requested shutdown");
            return Ok();  // Never reached

        default:
            ESP_LOGW(TAG, "Unknown command: 0x%02X", static_cast<int>(cmd));
            return uart_->send(IpcCommand::NACK);
        }
    }

private:
    UartBridge*          uart_     = nullptr;
    MeasurementPipeline* pipeline_ = nullptr;
    CalibrationService*  cal_      = nullptr;
    SafetyManager*       safety_   = nullptr;

    Result<void> handleStartMeasurement(const uint8_t* payload, size_t len) {
        if (!pipeline_) {
            return uart_->sendError(ErrorCategory::NOT_INITIALIZED, "No pipeline");
        }

        uart_->send(IpcCommand::ACK);

        // Parse config from payload (simplified: use defaults)
        PipelineConfig config;
        config.auto_roi       = true;
        config.num_captures   = 3;
        config.led_intensity  = 200;

        // Set progress callback to forward via UART
        pipeline_->setProgressCallback(
            [](const PipelineProgress& p, void* ctx) {
                auto* u = static_cast<UartBridge*>(ctx);
                u->sendProgress(p.percentage, p.message.c_str());
            }, uart_);

        // Run measurement (blocking — runs in this task)
        auto result = pipeline_->runMeasurement(config);

        if (result.is_ok()) {
            uart_->sendResult(result.value());
        } else {
            uart_->sendError(result.error().category, result.error().message);
        }

        return Ok();
    }

    Result<void> handleStartCalibration(const uint8_t* payload, size_t len) {
        if (!cal_) return uart_->sendError(ErrorCategory::NOT_INITIALIZED, "No cal svc");

        // Analyte name from payload
        char analyte[32] = {};
        size_t copy = (len < 31) ? len : 31;
        memcpy(analyte, payload, copy);

        auto res = cal_->startCalibrationWorkflow(analyte);
        if (res.is_ok()) {
            return uart_->send(IpcCommand::ACK);
        }
        return uart_->sendError(res.error().category, res.error().message);
    }

    Result<void> handleAddCalPoint(const uint8_t* payload, size_t len) {
        if (!cal_ || len < 8) {
            return uart_->send(IpcCommand::NACK);
        }

        float conc, signal;
        memcpy(&conc,   payload,     sizeof(float));
        memcpy(&signal, payload + 4, sizeof(float));

        auto res = cal_->addCalibrationPoint(conc, signal);
        return res.is_ok() ? uart_->send(IpcCommand::ACK)
                           : uart_->sendError(res.error().category, res.error().message);
    }

    Result<void> handleFinishCalibration() {
        if (!cal_) return uart_->send(IpcCommand::NACK);

        auto fit = cal_->fitCurve();
        if (fit.is_err()) {
            return uart_->sendError(fit.error().category, fit.error().message);
        }

        auto val = cal_->validateCalibration();
        if (val.is_err()) {
            return uart_->sendError(val.error().category, val.error().message);
        }

        auto save = cal_->saveCalibration("active");
        if (save.is_err()) {
            return uart_->sendError(save.error().category, save.error().message);
        }

        // Send calibration status to UI
        struct __attribute__((packed)) {
            float r_squared;
            float ec50;
            uint8_t num_points;
        } status = {
            fit.value().r_squared,
            fit.value().C,
            cal_->getPointCount()
        };

        return uart_->send(IpcCommand::CALIBRATION_STATUS, &status, sizeof(status));
    }

    Result<void> handleDiagnostics() {
        if (!safety_) return uart_->send(IpcCommand::NACK);

        auto report = safety_->performPowerOnSelfTest();
        if (report.is_ok()) {
            return uart_->send(IpcCommand::DIAGNOSTICS_REPORT,
                               &report.value(), sizeof(DiagnosticsReport));
        }
        return uart_->sendError(report.error().category, report.error().message);
    }

    Result<void> handleGetStatus() {
        struct __attribute__((packed)) {
            uint8_t pipeline_state;
            float   heap_free_kb;
            uint32_t uptime_s;
        } status = {};

        if (pipeline_) {
            status.pipeline_state = static_cast<uint8_t>(pipeline_->getState());
        }
        status.heap_free_kb = static_cast<float>(esp_get_free_heap_size()) / 1024.0f;
        status.uptime_s = static_cast<uint32_t>(esp_timer_get_time() / 1000000ULL);

        return uart_->send(IpcCommand::STATUS_REPORT, &status, sizeof(status));
    }
};

} // namespace phoenix
