// ==========================================================================
// FILE: src/IPC/MeasurementProxy.cpp
// ==========================================================================
#include "phoenix/IPC/MeasurementProxy.h"
#include <esp_log.h>
#include <cstring>

namespace phoenix {
static const char* TAG = "MeasProxy";

MeasurementProxy::MeasurementProxy(UartBridge* uart) : uart_(uart) {}

Result<void> MeasurementProxy::startMeasurement() {
    return uart_->send(IpcCommand::CMD_START_MEASUREMENT);
}
Result<void> MeasurementProxy::cancelMeasurement() {
    return uart_->send(IpcCommand::CMD_CANCEL_MEASUREMENT);
}
Result<void> MeasurementProxy::startCalibration(const char* analyte) {
    return uart_->send(IpcCommand::CMD_START_CALIBRATION,
                       analyte, strlen(analyte));
}
Result<void> MeasurementProxy::addCalibrationPoint(float conc, float sig) {
    struct __attribute__((packed)) { float c; float s; } p = {conc, sig};
    return uart_->send(IpcCommand::CMD_ADD_CAL_POINT, &p, sizeof(p));
}
Result<void> MeasurementProxy::finishCalibration() {
    return uart_->send(IpcCommand::CMD_FINISH_CALIBRATION);
}
Result<void> MeasurementProxy::requestDiagnostics() {
    return uart_->send(IpcCommand::CMD_RUN_DIAGNOSTICS);
}
Result<void> MeasurementProxy::requestStatus() {
    return uart_->send(IpcCommand::CMD_GET_STATUS);
}
Result<void> MeasurementProxy::ping() {
    auto res = uart_->ping(500);
    connected_ = res.is_ok();
    return res;
}

void MeasurementProxy::poll(uint32_t timeout_ms) {
    if (!uart_ || !uart_->hasData()) return;

    uint8_t payload[512];
    size_t len = sizeof(payload);
    auto cmd = uart_->receive(payload, &len, timeout_ms);
    if (cmd.is_ok()) {
        handleIncoming(cmd.value(), payload, len);
    }
}

void MeasurementProxy::handleIncoming(
    IpcCommand cmd, const uint8_t* data, size_t len)
{
    switch (cmd) {
    case IpcCommand::MEASUREMENT_RESULT:
        if (len >= sizeof(MeasurementResult)) {
            memcpy(&last_result_, data, sizeof(MeasurementResult));
            if (callbacks_.on_result)
                callbacks_.on_result(last_result_, callbacks_.ctx);
            ESP_LOGI(TAG, "Result: %.2f %s",
                     static_cast<double>(last_result_.concentration_ng_ml),
                     last_result_.interpretation.c_str());
        }
        break;

    case IpcCommand::MEASUREMENT_PROGRESS:
        if (len >= 2 && callbacks_.on_progress) {
            uint8_t pct = data[0];
            const char* msg = reinterpret_cast<const char*>(&data[1]);
            callbacks_.on_progress(pct, msg, callbacks_.ctx);
        }
        break;

    case IpcCommand::ERROR_REPORT:
        if (len >= 2 && callbacks_.on_error) {
            auto cat = static_cast<ErrorCategory>(data[0]);
            const char* msg = reinterpret_cast<const char*>(&data[1]);
            callbacks_.on_error(cat, msg, callbacks_.ctx);
        }
        break;

    case IpcCommand::PONG:
        connected_ = true;
        break;

    case IpcCommand::DIAGNOSTICS_REPORT:
        ESP_LOGI(TAG, "Diagnostics received (%u bytes)", len);
        break;

    default:
        ESP_LOGD(TAG, "Unhandled IPC: 0x%02X", static_cast<int>(cmd));
    }
}

} // namespace phoenix
