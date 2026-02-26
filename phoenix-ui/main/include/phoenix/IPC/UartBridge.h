// ==========================================================================
// FILE: include/phoenix/IPC/UartBridge.h
// Inter-Processor Communication via UART
// ESP32 #2 (Measurement) ↔ ESP32 #1 (UI/Connectivity)
//
// Protocol: Length-prefixed binary frames with CRC16
// Frame: [SYNC(2)] [LEN(2)] [CMD(1)] [PAYLOAD(0..1024)] [CRC16(2)]
// ==========================================================================
#pragma once

#include "phoenix/Core/Result.h"
#include "phoenix/Core/FixedString.h"
#include "phoenix/Core/MeasurementTypes.h"
#include <cstdint>
#include <cstddef>

namespace phoenix {

// ─── UART Configuration ───────────────────────────────────────────────
struct UartConfig {
    int      uart_num     = 1;         // UART1 (UART0 reserved for debug)
    int      tx_pin       = 43;
    int      rx_pin       = 44;
    uint32_t baud_rate    = 921600;    // Fast for image transfer
    uint16_t rx_buf_size  = 2048;
    uint16_t tx_buf_size  = 2048;
};

// ─── Command IDs ──────────────────────────────────────────────────────
enum class IpcCommand : uint8_t {
    // ─ Measurement MCU → UI MCU
    ACK                     = 0x01,
    NACK                    = 0x02,
    STATUS_REPORT           = 0x10,
    MEASUREMENT_PROGRESS    = 0x11,
    MEASUREMENT_RESULT      = 0x12,
    DIAGNOSTICS_REPORT      = 0x13,
    IMAGE_THUMBNAIL         = 0x14,  // Downscaled preview
    ERROR_REPORT            = 0x15,
    CALIBRATION_STATUS      = 0x16,

    // ─ UI MCU → Measurement MCU
    CMD_START_MEASUREMENT   = 0x80,
    CMD_CANCEL_MEASUREMENT  = 0x81,
    CMD_START_CALIBRATION   = 0x82,
    CMD_ADD_CAL_POINT       = 0x83,
    CMD_FINISH_CALIBRATION  = 0x84,
    CMD_SET_EXPOSURE        = 0x85,
    CMD_SET_LED             = 0x86,
    CMD_CAPTURE_PREVIEW     = 0x87,
    CMD_RUN_DIAGNOSTICS     = 0x88,
    CMD_GET_STATUS          = 0x89,
    CMD_SHUTDOWN            = 0x8A,

    // ─ Bidirectional
    PING                    = 0xF0,
    PONG                    = 0xF1,
};

// ─── Frame Header ─────────────────────────────────────────────────────
static constexpr uint16_t FRAME_SYNC = 0xAA55;
static constexpr size_t   MAX_PAYLOAD = 1024;
static constexpr size_t   FRAME_OVERHEAD = 7;  // SYNC(2) + LEN(2) + CMD(1) + CRC(2)

struct __attribute__((packed)) FrameHeader {
    uint16_t sync    = FRAME_SYNC;
    uint16_t length  = 0;     // Payload length (excl. header + CRC)
    uint8_t  command = 0;
};

// ─── UART Bridge ──────────────────────────────────────────────────────
class UartBridge {
public:
    Result<void> initialize(const UartConfig& config = {});
    void         deinitialize();

    // Send a command with optional payload
    Result<void> send(IpcCommand cmd, const void* payload = nullptr, size_t len = 0);

    // Send typed data
    Result<void> sendResult(const MeasurementResult& result);
    Result<void> sendProgress(uint8_t percentage, const char* message);
    Result<void> sendError(ErrorCategory cat, const char* message);

    // Receive (blocking with timeout)
    Result<IpcCommand> receive(void* payload_out, size_t* len_out, uint32_t timeout_ms = 1000);

    // Non-blocking check
    bool hasData() const;

    // Ping/pong for connection test
    Result<void> ping(uint32_t timeout_ms = 500);

    // Stats
    uint32_t getTxCount() const { return tx_count_; }
    uint32_t getRxCount() const { return rx_count_; }
    uint32_t getErrorCount() const { return err_count_; }

private:
    UartConfig config_       = {};
    bool       initialized_  = false;
    uint32_t   tx_count_     = 0;
    uint32_t   rx_count_     = 0;
    uint32_t   err_count_    = 0;

    // CRC-16/CCITT
    static uint16_t crc16(const uint8_t* data, size_t len);

    // Frame assembly/parsing
    Result<void> sendFrame(uint8_t cmd, const void* payload, size_t len);
    Result<size_t> receiveFrame(uint8_t* cmd_out, void* payload_out, size_t max_len, uint32_t timeout_ms);
};

} // namespace phoenix
