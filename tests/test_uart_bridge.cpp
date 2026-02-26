// ==========================================================================
// Test: UART Bridge Protocol — CRC16, Frame Encoding/Decoding
// Verifies: IEC 62304 REQ-IPC-001 — Reliable inter-processor communication
//
// Tests the pure protocol logic (CRC, framing) without UART hardware.
// CRC16 and frame build/parse are reimplemented here to match the
// production UartBridge::crc16() / sendFrame() / receiveFrame().
// ==========================================================================
#include "test_framework.h"
#include "phoenix/IPC/UartBridge.h"
#include <cstring>

using namespace phoenix;

// ── Standalone CRC16/CCITT (matches UartBridge::crc16) ───────────────────

static uint16_t crc16_ccitt(const uint8_t* data, size_t len) {
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < len; ++i) {
        crc ^= static_cast<uint16_t>(data[i]) << 8;
        for (int j = 0; j < 8; ++j) {
            if (crc & 0x8000) crc = (crc << 1) ^ 0x1021;
            else crc <<= 1;
        }
    }
    return crc;
}

// Build a frame in memory (matches UartBridge frame layout)
static size_t buildFrame(uint8_t* frame, uint8_t cmd,
                          const void* payload, size_t len) {
    frame[0] = static_cast<uint8_t>(FRAME_SYNC >> 8);
    frame[1] = static_cast<uint8_t>(FRAME_SYNC & 0xFF);
    frame[2] = static_cast<uint8_t>(len >> 8);
    frame[3] = static_cast<uint8_t>(len & 0xFF);
    frame[4] = cmd;
    if (payload && len > 0) memcpy(&frame[5], payload, len);
    uint16_t crc = crc16_ccitt(&frame[4], len + 1);
    frame[5 + len] = static_cast<uint8_t>(crc >> 8);
    frame[6 + len] = static_cast<uint8_t>(crc & 0xFF);
    return FRAME_OVERHEAD + len;
}

// Parse a frame from buffer
static bool parseFrame(const uint8_t* frame, size_t frame_len,
                        uint8_t* cmd_out, uint8_t* payload_out,
                        size_t* payload_len_out) {
    if (frame_len < FRAME_OVERHEAD) return false;
    uint16_t sync = (static_cast<uint16_t>(frame[0]) << 8) | frame[1];
    if (sync != FRAME_SYNC) return false;
    uint16_t plen = (static_cast<uint16_t>(frame[2]) << 8) | frame[3];
    if (frame_len < FRAME_OVERHEAD + plen) return false;
    uint16_t received_crc = (static_cast<uint16_t>(frame[5 + plen]) << 8) | frame[6 + plen];
    uint16_t computed_crc = crc16_ccitt(&frame[4], plen + 1);
    if (received_crc != computed_crc) return false;
    *cmd_out = frame[4];
    if (payload_out && plen > 0) memcpy(payload_out, &frame[5], plen);
    *payload_len_out = plen;
    return true;
}

// ── CRC16 tests ──────────────────────────────────────────────────────────

TEST_SUITE(crc16_known_vector_123456789) {
    const uint8_t data[] = "123456789";
    uint16_t crc = crc16_ccitt(data, 9);
    ASSERT_EQ(crc, 0x29B1);
}

TEST_SUITE(crc16_single_byte) {
    const uint8_t data[] = {0xF0};  // PING
    uint16_t crc = crc16_ccitt(data, 1);
    ASSERT_TRUE(crc != 0);
    // CRC of same data must be deterministic
    ASSERT_EQ(crc, crc16_ccitt(data, 1));
}

TEST_SUITE(crc16_empty) {
    uint16_t crc = crc16_ccitt(nullptr, 0);
    ASSERT_EQ(crc, 0xFFFF);  // Initial value, no data processed
}

TEST_SUITE(crc16_all_zeros) {
    uint8_t data[16] = {};
    uint16_t crc = crc16_ccitt(data, 16);
    ASSERT_TRUE(crc != 0);  // Non-trivial
}

TEST_SUITE(crc16_different_data) {
    uint8_t a[] = {0x01, 0x02, 0x03};
    uint8_t b[] = {0x01, 0x02, 0x04};  // One bit different
    ASSERT_TRUE(crc16_ccitt(a, 3) != crc16_ccitt(b, 3));
}

// ── Frame encoding ───────────────────────────────────────────────────────

TEST_SUITE(frame_ping_no_payload) {
    uint8_t frame[64];
    size_t len = buildFrame(frame, static_cast<uint8_t>(IpcCommand::PING), nullptr, 0);

    ASSERT_EQ(len, FRAME_OVERHEAD);
    ASSERT_EQ(frame[0], 0xAA);
    ASSERT_EQ(frame[1], 0x55);
    ASSERT_EQ(frame[2], 0x00);  // Length high
    ASSERT_EQ(frame[3], 0x00);  // Length low
    ASSERT_EQ(frame[4], 0xF0);  // PING
}

TEST_SUITE(frame_with_float_payload) {
    float value = 42.5f;
    uint8_t frame[64];
    size_t len = buildFrame(frame, static_cast<uint8_t>(IpcCommand::MEASUREMENT_RESULT),
                             &value, sizeof(value));

    ASSERT_EQ(len, FRAME_OVERHEAD + sizeof(float));
}

TEST_SUITE(frame_max_payload) {
    // Test with payload near MAX_PAYLOAD
    uint8_t payload[256];
    memset(payload, 0xAB, sizeof(payload));

    uint8_t frame[512];
    size_t len = buildFrame(frame, 0x10, payload, sizeof(payload));
    ASSERT_EQ(len, FRAME_OVERHEAD + sizeof(payload));
}

// ── Frame decoding ───────────────────────────────────────────────────────

TEST_SUITE(frame_roundtrip_ping) {
    uint8_t frame[64];
    size_t len = buildFrame(frame, static_cast<uint8_t>(IpcCommand::PING), nullptr, 0);

    uint8_t cmd;
    uint8_t payload[64];
    size_t plen;
    bool ok = parseFrame(frame, len, &cmd, payload, &plen);

    ASSERT_TRUE(ok);
    ASSERT_EQ(cmd, 0xF0);
    ASSERT_EQ(plen, 0u);
}

TEST_SUITE(frame_roundtrip_float) {
    float original = 42.5f;
    uint8_t frame[64];
    size_t len = buildFrame(frame, static_cast<uint8_t>(IpcCommand::MEASUREMENT_RESULT),
                             &original, sizeof(original));

    uint8_t cmd;
    uint8_t payload[64];
    size_t plen;
    bool ok = parseFrame(frame, len, &cmd, payload, &plen);

    ASSERT_TRUE(ok);
    ASSERT_EQ(plen, sizeof(float));

    float recovered;
    memcpy(&recovered, payload, sizeof(float));
    ASSERT_FLOAT_EQ(recovered, 42.5f, 0.001f);
}

TEST_SUITE(frame_roundtrip_struct) {
    struct __attribute__((packed)) {
        uint8_t pct;
        char msg[32];
    } progress;
    progress.pct = 75;
    strncpy(progress.msg, "Processing image...", sizeof(progress.msg));

    uint8_t frame[128];
    size_t len = buildFrame(frame, static_cast<uint8_t>(IpcCommand::MEASUREMENT_PROGRESS),
                             &progress, sizeof(progress));

    uint8_t cmd;
    uint8_t payload[128];
    size_t plen;
    bool ok = parseFrame(frame, len, &cmd, payload, &plen);

    ASSERT_TRUE(ok);
    ASSERT_EQ(payload[0], 75);
    ASSERT_TRUE(strncmp(reinterpret_cast<char*>(&payload[1]),
                        "Processing image...", 19) == 0);
}

// ── Corruption detection ─────────────────────────────────────────────────

TEST_SUITE(frame_corrupted_crc_rejected) {
    float value = 1.0f;
    uint8_t frame[64];
    size_t len = buildFrame(frame, 0x12, &value, sizeof(value));

    // Corrupt last byte (CRC)
    frame[len - 1] ^= 0xFF;

    uint8_t cmd;
    uint8_t payload[64];
    size_t plen;
    bool ok = parseFrame(frame, len, &cmd, payload, &plen);
    ASSERT_FALSE(ok);
}

TEST_SUITE(frame_corrupted_sync_rejected) {
    uint8_t frame[64];
    size_t len = buildFrame(frame, 0xF0, nullptr, 0);

    // Corrupt SYNC word
    frame[0] = 0xBB;

    uint8_t cmd;
    uint8_t payload[64];
    size_t plen;
    bool ok = parseFrame(frame, len, &cmd, payload, &plen);
    ASSERT_FALSE(ok);
}

TEST_SUITE(frame_truncated_rejected) {
    uint8_t frame[64];
    buildFrame(frame, 0xF0, nullptr, 0);

    // Too short
    uint8_t cmd;
    uint8_t payload[64];
    size_t plen;
    bool ok = parseFrame(frame, 3, &cmd, payload, &plen);  // Less than FRAME_OVERHEAD
    ASSERT_FALSE(ok);
}

TEST_SUITE(frame_corrupted_payload_rejected) {
    float value = 3.14f;
    uint8_t frame[64];
    size_t len = buildFrame(frame, 0x12, &value, sizeof(value));

    // Corrupt payload byte (not CRC, not sync)
    frame[5] ^= 0x01;

    uint8_t cmd;
    uint8_t payload[64];
    size_t plen;
    bool ok = parseFrame(frame, len, &cmd, payload, &plen);
    ASSERT_FALSE(ok);  // CRC mismatch
}

// ── Protocol constants ───────────────────────────────────────────────────

TEST_SUITE(protocol_constants) {
    ASSERT_EQ(FRAME_SYNC, 0xAA55);
    ASSERT_EQ(FRAME_OVERHEAD, 7u);
    ASSERT_TRUE(MAX_PAYLOAD >= 256);
}

TEST_SUITE(ipc_command_values) {
    ASSERT_EQ(static_cast<uint8_t>(IpcCommand::PING), 0xF0);
    ASSERT_EQ(static_cast<uint8_t>(IpcCommand::PONG), 0xF1);
    ASSERT_EQ(static_cast<uint8_t>(IpcCommand::ACK), 0x01);
    ASSERT_EQ(static_cast<uint8_t>(IpcCommand::NACK), 0x02);
    ASSERT_EQ(static_cast<uint8_t>(IpcCommand::CMD_START_MEASUREMENT), 0x80);
}
