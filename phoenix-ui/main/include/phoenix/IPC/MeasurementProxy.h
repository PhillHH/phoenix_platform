// ==========================================================================
// FILE: include/phoenix/IPC/MeasurementProxy.h
// Proxy on UI MCU that sends commands to Measurement MCU via UART
// and receives results/progress asynchronously
// ==========================================================================
#pragma once
#include "phoenix/Core/Result.h"
#include "phoenix/Core/FixedString.h"
#include "phoenix/Core/MeasurementTypes.h"
#include "phoenix/IPC/UartBridge.h"
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

namespace phoenix {

using MeasurementCallback = void(*)(const MeasurementResult& result, void* ctx);
using ProgressCallback    = void(*)(uint8_t pct, const char* msg, void* ctx);
using ErrorCallback       = void(*)(ErrorCategory cat, const char* msg, void* ctx);

struct ProxyCallbacks {
    MeasurementCallback on_result   = nullptr;
    ProgressCallback    on_progress = nullptr;
    ErrorCallback       on_error    = nullptr;
    void*               ctx         = nullptr;
};

class MeasurementProxy {
public:
    explicit MeasurementProxy(UartBridge* uart);

    // Commands to Measurement MCU
    Result<void> startMeasurement();
    Result<void> cancelMeasurement();
    Result<void> startCalibration(const char* analyte);
    Result<void> addCalibrationPoint(float concentration, float signal);
    Result<void> finishCalibration();
    Result<void> requestDiagnostics();
    Result<void> requestStatus();
    Result<void> ping();

    // Set callbacks for async responses
    void setCallbacks(const ProxyCallbacks& cb) { callbacks_ = cb; }

    // Poll for incoming messages (call from UI task)
    void poll(uint32_t timeout_ms = 10);

    // Last received data
    const MeasurementResult* getLastResult() const { return &last_result_; }
    bool isConnected() const { return connected_; }

private:
    UartBridge*       uart_      = nullptr;
    ProxyCallbacks    callbacks_ = {};
    MeasurementResult last_result_ = {};
    bool              connected_ = false;

    void handleIncoming(IpcCommand cmd, const uint8_t* data, size_t len);
};

} // namespace phoenix
