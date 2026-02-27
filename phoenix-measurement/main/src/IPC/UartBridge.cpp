// ==========================================================================
// FILE: src/IPC/UartBridge.cpp
// UART IPC between ESP32 #2 (Measurement) ↔ ESP32 #1 (UI)
// Protocol: [0xAA55][LEN:2][CMD:1][PAYLOAD:0..1024][CRC16:2]
// ==========================================================================

#include "phoenix/IPC/UartBridge.h"
#include "phoenix/Core/Logger.h"
#include <driver/uart.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <cstring>

namespace phoenix {

static const char* TAG = "UART-IPC";

// ─── CRC-16/CCITT ─────────────────────────────────────────────────────
uint16_t UartBridge::crc16(const uint8_t* data, size_t len) {
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < len; ++i) {
        crc ^= static_cast<uint16_t>(data[i]) << 8;
        for (int j = 0; j < 8; ++j) {
            if (crc & 0x8000) {
                crc = (crc << 1) ^ 0x1021;
            } else {
                crc <<= 1;
            }
        }
    }
    return crc;
}

// ─── Initialize ───────────────────────────────────────────────────────
Result<void> UartBridge::initialize(const UartConfig& config) {
    if (initialized_) return Ok();

    config_ = config;

    uart_config_t uart_cfg = {};
    uart_cfg.baud_rate  = static_cast<int>(config.baud_rate);
    uart_cfg.data_bits  = UART_DATA_8_BITS;
    uart_cfg.parity     = UART_PARITY_DISABLE;
    uart_cfg.stop_bits  = UART_STOP_BITS_1;
    uart_cfg.flow_ctrl  = UART_HW_FLOWCTRL_DISABLE;
    uart_cfg.source_clk = UART_SCLK_DEFAULT;

    esp_err_t err = uart_param_config(
        static_cast<uart_port_t>(config.uart_num), &uart_cfg);
    if (err != ESP_OK) {
        return Err(ErrorCategory::COMMUNICATION_ERROR,
                   "UART config failed", static_cast<uint32_t>(err));
    }

    err = uart_set_pin(
        static_cast<uart_port_t>(config.uart_num),
        config.tx_pin, config.rx_pin,
        UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    if (err != ESP_OK) {
        return Err(ErrorCategory::COMMUNICATION_ERROR, "UART pin config failed");
    }

    err = uart_driver_install(
        static_cast<uart_port_t>(config.uart_num),
        config.rx_buf_size, config.tx_buf_size,
        0, nullptr, 0);
    if (err != ESP_OK) {
        return Err(ErrorCategory::COMMUNICATION_ERROR, "UART driver install failed");
    }

    initialized_ = true;
    PHOENIX_LOGI(TAG, "UART%d initialized: %u baud, TX=%d RX=%d",
             config.uart_num, config.baud_rate,
             config.tx_pin, config.rx_pin);
    return Ok();
}

void UartBridge::deinitialize() {
    if (initialized_) {
        uart_driver_delete(static_cast<uart_port_t>(config_.uart_num));
        initialized_ = false;
    }
}

// ─── Send Frame ───────────────────────────────────────────────────────
Result<void> UartBridge::sendFrame(
    uint8_t cmd, const void* payload, size_t len)
{
    if (!initialized_) {
        return Err(ErrorCategory::NOT_INITIALIZED, "UART not init");
    }
    if (len > MAX_PAYLOAD) {
        return Err(ErrorCategory::INVALID_PARAMETER, "Payload too large");
    }

    // Build frame: SYNC(2) + LEN(2) + CMD(1) + PAYLOAD(len) + CRC(2)
    size_t frame_size = FRAME_OVERHEAD + len;
    uint8_t frame[FRAME_OVERHEAD + MAX_PAYLOAD];

    // Header
    frame[0] = static_cast<uint8_t>(FRAME_SYNC >> 8);
    frame[1] = static_cast<uint8_t>(FRAME_SYNC & 0xFF);
    frame[2] = static_cast<uint8_t>(len >> 8);
    frame[3] = static_cast<uint8_t>(len & 0xFF);
    frame[4] = cmd;

    // Payload
    if (payload && len > 0) {
        memcpy(&frame[5], payload, len);
    }

    // CRC over CMD + PAYLOAD
    uint16_t crc = crc16(&frame[4], len + 1);
    frame[5 + len] = static_cast<uint8_t>(crc >> 8);
    frame[6 + len] = static_cast<uint8_t>(crc & 0xFF);

    int written = uart_write_bytes(
        static_cast<uart_port_t>(config_.uart_num),
        frame, frame_size);

    if (written < 0 || static_cast<size_t>(written) != frame_size) {
        err_count_++;
        return Err(ErrorCategory::COMMUNICATION_ERROR, "UART write failed");
    }

    tx_count_++;
    return Ok();
}

// ─── Receive Frame ────────────────────────────────────────────────────
Result<size_t> UartBridge::receiveFrame(
    uint8_t* cmd_out, void* payload_out, size_t max_len, uint32_t timeout_ms)
{
    if (!initialized_) {
        return Err<size_t>(ErrorCategory::NOT_INITIALIZED, "UART not init");
    }

    uart_port_t port = static_cast<uart_port_t>(config_.uart_num);
    TickType_t ticks  = pdMS_TO_TICKS(timeout_ms);

    // Read sync bytes
    uint8_t sync[2];
    int n = uart_read_bytes(port, sync, 2, ticks);
    if (n < 2) {
        return Err<size_t>(ErrorCategory::TIMEOUT, "Sync timeout");
    }

    uint16_t sync_word = (static_cast<uint16_t>(sync[0]) << 8) | sync[1];
    if (sync_word != FRAME_SYNC) {
        err_count_++;
        // Try to resync: skip bytes until we find sync
        for (int attempt = 0; attempt < 64; ++attempt) {
            sync[0] = sync[1];
            n = uart_read_bytes(port, &sync[1], 1, pdMS_TO_TICKS(10));
            if (n < 1) break;
            sync_word = (static_cast<uint16_t>(sync[0]) << 8) | sync[1];
            if (sync_word == FRAME_SYNC) break;
        }
        if (sync_word != FRAME_SYNC) {
            return Err<size_t>(ErrorCategory::COMMUNICATION_ERROR, "Sync lost");
        }
    }

    // Read length
    uint8_t len_buf[2];
    n = uart_read_bytes(port, len_buf, 2, ticks);
    if (n < 2) {
        return Err<size_t>(ErrorCategory::TIMEOUT, "Length timeout");
    }
    uint16_t payload_len = (static_cast<uint16_t>(len_buf[0]) << 8) | len_buf[1];
    if (payload_len > MAX_PAYLOAD) {
        err_count_++;
        return Err<size_t>(ErrorCategory::COMMUNICATION_ERROR, "Frame too large");
    }

    // Read CMD + payload + CRC
    size_t remaining = 1 + payload_len + 2;  // CMD + payload + CRC16
    uint8_t buf[1 + MAX_PAYLOAD + 2];
    n = uart_read_bytes(port, buf, remaining, ticks);
    if (n < static_cast<int>(remaining)) {
        return Err<size_t>(ErrorCategory::TIMEOUT, "Payload timeout");
    }

    // Verify CRC
    uint16_t received_crc =
        (static_cast<uint16_t>(buf[1 + payload_len]) << 8) |
        buf[1 + payload_len + 1];
    uint16_t computed_crc = crc16(buf, 1 + payload_len);

    if (received_crc != computed_crc) {
        err_count_++;
        PHOENIX_LOGW(TAG, "CRC mismatch: recv=0x%04X calc=0x%04X",
                 received_crc, computed_crc);
        return Err<size_t>(ErrorCategory::COMMUNICATION_ERROR, "CRC error");
    }

    // Extract command and payload
    *cmd_out = buf[0];
    size_t copy_len = (payload_len < max_len) ? payload_len : max_len;
    if (payload_out && copy_len > 0) {
        memcpy(payload_out, &buf[1], copy_len);
    }

    rx_count_++;
    return Ok(copy_len);
}

// ─── High-Level Send ──────────────────────────────────────────────────

Result<void> UartBridge::send(IpcCommand cmd, const void* payload, size_t len) {
    return sendFrame(static_cast<uint8_t>(cmd), payload, len);
}

Result<void> UartBridge::sendResult(const MeasurementResult& result) {
    return send(IpcCommand::MEASUREMENT_RESULT, &result, sizeof(result));
}

Result<void> UartBridge::sendProgress(uint8_t percentage, const char* message) {
    struct __attribute__((packed)) {
        uint8_t pct;
        char    msg[120];
    } payload = {};

    payload.pct = percentage;
    if (message) {
        strncpy(payload.msg, message, sizeof(payload.msg) - 1);
    }

    return send(IpcCommand::MEASUREMENT_PROGRESS, &payload,
                2 + strlen(payload.msg));
}

Result<void> UartBridge::sendError(ErrorCategory cat, const char* message) {
    struct __attribute__((packed)) {
        uint8_t category;
        char    msg[120];
    } payload = {};

    payload.category = static_cast<uint8_t>(cat);
    if (message) {
        strncpy(payload.msg, message, sizeof(payload.msg) - 1);
    }

    return send(IpcCommand::ERROR_REPORT, &payload,
                2 + strlen(payload.msg));
}

// ─── High-Level Receive ───────────────────────────────────────────────

Result<IpcCommand> UartBridge::receive(
    void* payload_out, size_t* len_out, uint32_t timeout_ms)
{
    uint8_t cmd;
    auto res = receiveFrame(&cmd, payload_out,
                            len_out ? *len_out : MAX_PAYLOAD, timeout_ms);
    if (res.is_err()) {
        return Err<IpcCommand>(res.error());
    }
    if (len_out) *len_out = res.value();
    return Ok(static_cast<IpcCommand>(cmd));
}

bool UartBridge::hasData() const {
    if (!initialized_) return false;
    size_t available = 0;
    uart_get_buffered_data_len(
        static_cast<uart_port_t>(config_.uart_num), &available);
    return available >= FRAME_OVERHEAD;
}

Result<void> UartBridge::ping(uint32_t timeout_ms) {
    PHOENIX_TRY(send(IpcCommand::PING));

    uint8_t cmd;
    uint8_t dummy[4];
    auto res = receiveFrame(&cmd, dummy, sizeof(dummy), timeout_ms);
    if (res.is_err()) {
        return Err(ErrorCategory::COMMUNICATION_ERROR, "Ping timeout");
    }
    if (static_cast<IpcCommand>(cmd) != IpcCommand::PONG) {
        return Err(ErrorCategory::COMMUNICATION_ERROR, "Expected PONG");
    }

    PHOENIX_LOGD(TAG, "Ping OK");
    return Ok();
}

} // namespace phoenix
