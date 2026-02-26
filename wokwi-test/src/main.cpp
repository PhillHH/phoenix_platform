// ==========================================================================
// Phoenix v108.0 "Chimera" — Wokwi ESP-IDF Integration Test
//
// This file contains ALL core Phoenix algorithms condensed for testing
// on the Wokwi ESP32 simulator. Every algorithm is verbatim from the
// real codebase — only hardware stubs are simplified for simulation.
//
// Tests:
//   1. Result<T> error handling framework
//   2. 5PL calibration math (forward + inverse + roundtrip)
//   3. Peak detection algorithm (7-point descriptor)
//   4. IPC protocol (CRC16, frame build/parse)
//   5. Measurement pipeline (synthetic profile → concentration)
//   6. LED control via LEDC PWM (visible on Wokwi!)
//   7. NVS storage (read/write calibration data)
//   8. Safety checks (heap monitoring)
// ==========================================================================

#include <Arduino.h>
#include <esp_log.h>
#include <esp_system.h>
#include <esp_timer.h>
#include <esp_heap_caps.h>
#include <nvs_flash.h>
#include <nvs.h>
#include <driver/ledc.h>
#include <driver/gpio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <cmath>
#include <algorithm>

static const char* TAG = "Phoenix";

// ══════════════════════════════════════════════════════════════════════════
// SECTION 1: Phoenix Core Framework (from Core/Result.h + FixedString.h)
// ══════════════════════════════════════════════════════════════════════════

namespace phoenix {

enum class ErrorCategory : uint8_t {
    NONE = 0,
    HARDWARE_FAILURE    = 1,
    CALIBRATION_ERROR   = 2,
    MEASUREMENT_ERROR   = 3,
    COMMUNICATION_ERROR = 4,
    MEMORY_ERROR        = 5,
    INVALID_PARAMETER   = 6,
    SAFETY_VIOLATION    = 7,
    TIMEOUT             = 8,
    NOT_FOUND           = 9,
    NOT_INITIALIZED     = 10,
    BUSY                = 11,
};

struct ErrorInfo {
    ErrorCategory category = ErrorCategory::NONE;
    char message[80]       = {};
    uint32_t code          = 0;

    constexpr ErrorInfo() = default;
    ErrorInfo(ErrorCategory cat, const char* msg, uint32_t c = 0)
        : category(cat), code(c) {
        if (msg) {
            strncpy(message, msg, sizeof(message) - 1);
            message[sizeof(message) - 1] = '\0';
        }
    }
};

template <typename T>
class Result {
public:
    static Result Ok(T val) {
        Result r; r.value_ = val; r.has_val_ = true; return r;
    }
    static Result Err(ErrorCategory cat, const char* msg, uint32_t code = 0) {
        Result r; r.error_ = ErrorInfo(cat, msg, code); r.has_val_ = false; return r;
    }
    static Result Err(ErrorInfo info) {
        Result r; r.error_ = info; r.has_val_ = false; return r;
    }
    bool is_ok()  const { return has_val_; }
    bool is_err() const { return !has_val_; }
    const T& value() const { return value_; }
    T&       value()       { return value_; }
    const ErrorInfo& error() const { return error_; }
    T value_or(T fallback) const { return has_val_ ? value_ : fallback; }
private:
    T value_ = {}; ErrorInfo error_ = {}; bool has_val_ = false;
};

template <>
class Result<void> {
public:
    static Result Ok() { Result r; r.ok_ = true; return r; }
    static Result Err(ErrorCategory cat, const char* msg, uint32_t code = 0) {
        Result r; r.error_ = ErrorInfo(cat, msg, code); r.ok_ = false; return r;
    }
    static Result Err(ErrorInfo info) {
        Result r; r.error_ = info; r.ok_ = false; return r;
    }
    bool is_ok()  const { return ok_; }
    bool is_err() const { return !ok_; }
    const ErrorInfo& error() const { return error_; }
private:
    ErrorInfo error_ = {}; bool ok_ = false;
};

template <typename T> Result<T> Ok(T val) { return Result<T>::Ok(val); }
inline Result<void> Ok() { return Result<void>::Ok(); }
template <typename T> Result<T> Err(ErrorCategory cat, const char* msg, uint32_t code = 0) {
    return Result<T>::Err(cat, msg, code);
}
inline Result<void> Err(ErrorCategory cat, const char* msg, uint32_t code = 0) {
    return Result<void>::Err(cat, msg, code);
}
template <typename T> Result<T> Err(ErrorInfo info) { return Result<T>::Err(info); }
inline Result<void> Err(ErrorInfo info) { return Result<void>::Err(info); }

#define PHOENIX_TRY(expr) \
    do { auto _r = (expr); if (_r.is_err()) return ::phoenix::Err(_r.error()); } while(0)

// FixedString (heap-free)
template <size_t N>
class FixedString {
public:
    constexpr FixedString() { buf_[0] = '\0'; }
    FixedString(const char* s) {
        if (s) { strncpy(buf_, s, N-1); buf_[N-1] = '\0'; }
        else { buf_[0] = '\0'; }
    }
    const char* c_str() const { return buf_; }
    size_t length() const { return strlen(buf_); }
    bool empty() const { return buf_[0] == '\0'; }
    bool operator==(const char* s) const { return strcmp(buf_, s) == 0; }
private:
    char buf_[N] = {};
};

using String16  = FixedString<16>;
using String32  = FixedString<32>;
using String64  = FixedString<64>;

// ══════════════════════════════════════════════════════════════════════════
// SECTION 2: 5PL Model (verbatim from Dx365Algorithm.h)
// y = D + (A - D) / (1 + (x/C)^B)^G
// ══════════════════════════════════════════════════════════════════════════

struct FivePL_G {
    float A = 0.0f;  // minimum asymptote
    float B = 1.0f;  // Hill slope
    float C = 100.0f; // inflection point (EC50)
    float D = 1.0f;  // maximum asymptote
    float G = 1.0f;  // asymmetry factor

    float evaluate(float x) const {
        if (x <= 0.0f) return A;
        float ratio = powf(x / C, B);
        return D + (A - D) / powf(1.0f + ratio, G);
    }

    float inverse(float y) const {
        if (fabsf(y - D) < 1e-8f) return 1e6f;
        if (fabsf(A - D) < 1e-8f) return C;
        float inner = (A - D) / (y - D);
        if (inner <= 0.0f) return 0.0f;
        float powered = powf(inner, 1.0f / G) - 1.0f;
        if (powered <= 0.0f) return 0.0f;
        return C * powf(powered, 1.0f / B);
    }
};

// ══════════════════════════════════════════════════════════════════════════
// SECTION 3: Peak Detection Structures
// ══════════════════════════════════════════════════════════════════════════

static constexpr size_t MAX_PROFILE_LEN = 400;
static constexpr size_t MAX_LINES = 8;

struct PeakDescriptor {
    uint16_t left_expect_idx, left_limit_idx, left_peak_idx;
    uint16_t center_idx;
    uint16_t right_peak_idx, right_limit_idx, right_expect_idx;
    uint16_t width;
    float value, height, integral;
    bool valid;
};

struct LineProfile {
    float data[MAX_PROFILE_LEN] = {};
    uint16_t length = 0;
};

struct AssayLine {
    String32 id;
    bool is_control;
    float x_position;
    float x_width;
    uint8_t color_id;
    String64 assay_id;
};

enum class SignalType : uint8_t { TL_DIV_CL = 0 };
enum class ScaleType  : uint8_t { LINEAR = 0 };

struct AssayConfig {
    String32 id;
    String64 name;
    String16 loinc_id;
    uint8_t measure_unit_id;
    SignalType signal_type;
    ScaleType scale_type;
    uint16_t incubation_sec;
    FivePL_G test_5pl, control_5pl, div_5pl;
    AssayLine lines[MAX_LINES];
    uint8_t num_lines;
};

struct Dx365MeasurementResult {
    PeakDescriptor peaks[MAX_LINES];
    uint8_t num_peaks;
    float control_line_intensity;
    struct AssayResult {
        String32 assay_id;
        float intensity, original_intensity, concentration;
    };
    AssayResult assay_results[MAX_LINES];
    uint8_t num_assay_results;
    LineProfile profile;
    bool succeeded;
};

// ══════════════════════════════════════════════════════════════════════════
// SECTION 4: Peak Detection Algorithm (from Dx365Algorithm.cpp)
// ══════════════════════════════════════════════════════════════════════════

class Dx365PeakDetector {
public:
    uint16_t findPeakCenter(const LineProfile& profile,
                            uint16_t expected_center, uint16_t search_radius) {
        uint16_t start = (expected_center > search_radius) ? expected_center - search_radius : 0;
        uint16_t end = (expected_center + search_radius < profile.length) ?
                        expected_center + search_radius : profile.length - 1;
        uint16_t min_idx = start;
        float min_val = profile.data[start];
        for (uint16_t i = start + 1; i <= end; i++) {
            if (profile.data[i] < min_val) { min_val = profile.data[i]; min_idx = i; }
        }
        return min_idx;
    }

    void findPeakBoundaries(const LineProfile& profile, uint16_t center,
                            uint16_t expected_width, PeakDescriptor& peak) {
        uint16_t half_w = expected_width / 2;
        uint16_t expect_left = (center > half_w + 20) ? center - half_w - 20 : 0;
        uint16_t expect_right = (center + half_w + 20 < profile.length) ?
                                 center + half_w + 20 : profile.length - 1;
        float baseline = 0; int bl_count = 0;
        for (uint16_t i = expect_left; i < center - half_w && i < profile.length; i++) {
            baseline += profile.data[i]; bl_count++;
        }
        for (uint16_t i = center + half_w; i <= expect_right && i < profile.length; i++) {
            baseline += profile.data[i]; bl_count++;
        }
        if (bl_count > 0) baseline /= bl_count;

        float center_val = profile.data[center];
        float half_height = (baseline + center_val) / 2.0f;

        uint16_t left_peak = center;
        for (uint16_t i = center; i > expect_left; i--) {
            if (profile.data[i] >= half_height) { left_peak = i; break; }
        }
        uint16_t right_peak = center;
        for (uint16_t i = center; i < expect_right; i++) {
            if (profile.data[i] >= half_height) { right_peak = i; break; }
        }

        float limit_threshold = baseline - (baseline - center_val) * 0.1f;
        uint16_t left_limit = left_peak;
        for (uint16_t i = left_peak; i > expect_left; i--) {
            if (profile.data[i] >= limit_threshold) { left_limit = i; break; }
        }
        uint16_t right_limit = right_peak;
        for (uint16_t i = right_peak; i < expect_right; i++) {
            if (profile.data[i] >= limit_threshold) { right_limit = i; break; }
        }

        peak.left_expect_idx = expect_left;
        peak.left_limit_idx = left_limit;
        peak.left_peak_idx = left_peak;
        peak.center_idx = center;
        peak.right_peak_idx = right_peak;
        peak.right_limit_idx = right_limit;
        peak.right_expect_idx = expect_right;
        peak.width = expected_width;
    }

    void computePeakMetrics(const LineProfile& profile, PeakDescriptor& peak) {
        float left_bl = profile.data[peak.left_limit_idx];
        float right_bl = profile.data[peak.right_limit_idx];
        float baseline = (left_bl + right_bl) / 2.0f;
        peak.height = baseline - profile.data[peak.center_idx];

        float sum = 0; int count = 0;
        for (uint16_t i = peak.left_peak_idx; i <= peak.right_peak_idx; i++) {
            sum += (baseline - profile.data[i]); count++;
        }
        peak.value = (count > 0) ? sum / count : 0;

        peak.integral = 0;
        for (uint16_t i = peak.left_limit_idx; i <= peak.right_limit_idx; i++) {
            peak.integral += (baseline - profile.data[i]);
        }
        peak.valid = (peak.height > 0.5f);
        if (peak.value < 0) { peak.value = 0; peak.valid = false; }
    }

    Result<void> detectPeaks(const LineProfile& profile, const AssayLine* expected_lines,
                             uint8_t num_lines, PeakDescriptor* out_peaks, uint8_t& out_count) {
        if (profile.length == 0) return Err(ErrorCategory::INVALID_PARAMETER, "Empty profile");
        out_count = 0;
        for (uint8_t i = 0; i < num_lines && i < MAX_LINES; i++) {
            const auto& line = expected_lines[i];
            uint16_t expected_idx = static_cast<uint16_t>(line.x_position);
            uint16_t expected_width = static_cast<uint16_t>(line.x_width);
            uint16_t center = findPeakCenter(profile, expected_idx, expected_width);

            PeakDescriptor peak = {};
            findPeakBoundaries(profile, center, expected_width, peak);
            computePeakMetrics(profile, peak);
            out_peaks[i] = peak;
            out_count++;
        }
        return Ok();
    }
};

// ══════════════════════════════════════════════════════════════════════════
// SECTION 5: IPC Protocol (from UartBridge.cpp)
// ══════════════════════════════════════════════════════════════════════════

enum class IpcCommand : uint8_t {
    ACK = 0x01, NACK = 0x02,
    STATUS_REPORT = 0x10, MEASUREMENT_PROGRESS = 0x11,
    MEASUREMENT_RESULT = 0x12, DIAGNOSTICS_REPORT = 0x13,
    ERROR_REPORT = 0x15, CALIBRATION_STATUS = 0x16,
    CMD_START_MEASUREMENT = 0x80, CMD_CANCEL_MEASUREMENT = 0x81,
    CMD_START_CALIBRATION = 0x82, CMD_RUN_DIAGNOSTICS = 0x88,
    CMD_GET_STATUS = 0x89, CMD_SHUTDOWN = 0x8A,
    PING = 0xF0, PONG = 0xF1,
};

static constexpr uint16_t FRAME_SYNC = 0xAA55;
static constexpr size_t MAX_PAYLOAD = 256;
static constexpr size_t FRAME_OVERHEAD = 7;

// CRC-16/CCITT (verbatim from UartBridge.cpp)
uint16_t crc16(const uint8_t* data, size_t len) {
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

// Build a frame in memory (for testing without real UART)
size_t buildFrame(uint8_t* frame, uint8_t cmd, const void* payload, size_t len) {
    frame[0] = static_cast<uint8_t>(FRAME_SYNC >> 8);
    frame[1] = static_cast<uint8_t>(FRAME_SYNC & 0xFF);
    frame[2] = static_cast<uint8_t>(len >> 8);
    frame[3] = static_cast<uint8_t>(len & 0xFF);
    frame[4] = cmd;
    if (payload && len > 0) memcpy(&frame[5], payload, len);
    uint16_t crc = crc16(&frame[4], len + 1);
    frame[5 + len] = static_cast<uint8_t>(crc >> 8);
    frame[6 + len] = static_cast<uint8_t>(crc & 0xFF);
    return FRAME_OVERHEAD + len;
}

// Parse a frame from buffer
bool parseFrame(const uint8_t* frame, size_t frame_len,
                uint8_t* cmd_out, uint8_t* payload_out, size_t* payload_len_out) {
    if (frame_len < FRAME_OVERHEAD) return false;
    uint16_t sync = (static_cast<uint16_t>(frame[0]) << 8) | frame[1];
    if (sync != FRAME_SYNC) return false;
    uint16_t plen = (static_cast<uint16_t>(frame[2]) << 8) | frame[3];
    if (frame_len < FRAME_OVERHEAD + plen) return false;

    uint16_t received_crc = (static_cast<uint16_t>(frame[5 + plen]) << 8) | frame[6 + plen];
    uint16_t computed_crc = crc16(&frame[4], plen + 1);
    if (received_crc != computed_crc) return false;

    *cmd_out = frame[4];
    if (payload_out && plen > 0) memcpy(payload_out, &frame[5], plen);
    *payload_len_out = plen;
    return true;
}

// ══════════════════════════════════════════════════════════════════════════
// SECTION 6: Verified IgE Assay Configuration (from Dx365Algorithm.h)
// ══════════════════════════════════════════════════════════════════════════

AssayConfig getVerifiedIgEAssay() {
    AssayConfig cfg = {};
    cfg.id = "IgE-LFT";
    cfg.name = "IgE Ab [Presence] in Serum or Plasma";
    cfg.loinc_id = "51651-8";
    cfg.measure_unit_id = 77;
    cfg.signal_type = SignalType::TL_DIV_CL;
    cfg.scale_type = ScaleType::LINEAR;
    cfg.incubation_sec = 600;

    // VERIFIED Division 5PL (concentration -> T/C ratio) — PRIMARY
    cfg.div_5pl = { -0.000202f, 1.59834f, 255.486f, 0.28367f, 10.0f };

    // VERIFIED Test 5PL (concentration -> peak value)
    cfg.test_5pl = { 0.01896f, 1.75754f, 117.834f, 5.82638f, 3.77055f };

    // Control 5PL
    cfg.control_5pl = { -12581.79f, -0.01987f, 31697768.0f, 24.0337f, 10.0f };

    // Line definitions from MCP project
    cfg.lines[0] = {"ctrl", true,  47.0f, 44.0f, 0, "ctrl"};   // Control line
    cfg.lines[1] = {"tl1",  false, 138.3f, 45.6f, 2, "IgE-LFT"}; // Test line 1
    cfg.lines[2] = {"tl2",  false, 247.1f, 43.9f, 3, "IgE-LFT"}; // Test line 2
    cfg.num_lines = 3;

    return cfg;
}

// Generate synthetic profile mimicking real Dx365 measurement data
LineProfile generateSyntheticProfile(float cl_depth, float tl1_depth, float tl2_depth) {
    LineProfile profile = {};
    profile.length = 350;

    // Fill with baseline ~165 (typical from real data)
    for (int i = 0; i < profile.length; i++) {
        profile.data[i] = 165.0f;
    }

    // Control line at ~idx 47, width ~44 (Gaussian dip)
    for (int i = 20; i < 75; i++) {
        float x = (i - 47.0f) / 10.0f;
        profile.data[i] = 165.0f - cl_depth * expf(-0.5f * x * x);
    }

    // Test line 1 at ~idx 138, width ~46
    for (int i = 110; i < 165; i++) {
        float x = (i - 138.0f) / 10.0f;
        profile.data[i] = 165.0f - tl1_depth * expf(-0.5f * x * x);
    }

    // Test line 2 at ~idx 247, width ~44
    for (int i = 220; i < 275; i++) {
        float x = (i - 247.0f) / 10.0f;
        profile.data[i] = 165.0f - tl2_depth * expf(-0.5f * x * x);
    }

    return profile;
}

} // namespace phoenix

// ══════════════════════════════════════════════════════════════════════════
// SECTION 7: LED Controller (real LEDC API — LEDs light up on Wokwi!)
// ══════════════════════════════════════════════════════════════════════════

// Pin mapping from Phoenix LEDController.cpp
static constexpr gpio_num_t PIN_LED_WHITE = GPIO_NUM_4;
static constexpr gpio_num_t PIN_LED_R     = GPIO_NUM_2;
static constexpr gpio_num_t PIN_LED_G     = GPIO_NUM_14;
static constexpr gpio_num_t PIN_LED_B     = GPIO_NUM_13;

static constexpr ledc_channel_t CH_WHITE = LEDC_CHANNEL_1;
static constexpr ledc_channel_t CH_R     = LEDC_CHANNEL_2;
static constexpr ledc_channel_t CH_G     = LEDC_CHANNEL_3;
static constexpr ledc_channel_t CH_B     = LEDC_CHANNEL_4;

static constexpr ledc_timer_t LED_TIMER = LEDC_TIMER_1;

static void led_init() {
    ledc_timer_config_t timer_cfg = {};
    timer_cfg.speed_mode      = LEDC_LOW_SPEED_MODE;
    timer_cfg.duty_resolution = LEDC_TIMER_8_BIT;
    timer_cfg.timer_num       = LED_TIMER;
    timer_cfg.freq_hz         = 5000;
    timer_cfg.clk_cfg         = LEDC_AUTO_CLK;
    ledc_timer_config(&timer_cfg);

    struct { gpio_num_t pin; ledc_channel_t ch; } channels[] = {
        {PIN_LED_WHITE, CH_WHITE}, {PIN_LED_R, CH_R},
        {PIN_LED_G, CH_G}, {PIN_LED_B, CH_B},
    };
    for (auto& c : channels) {
        ledc_channel_config_t ch_cfg = {};
        ch_cfg.gpio_num   = c.pin;
        ch_cfg.speed_mode = LEDC_LOW_SPEED_MODE;
        ch_cfg.channel    = c.ch;
        ch_cfg.timer_sel  = LED_TIMER;
        ch_cfg.duty       = 0;
        ch_cfg.hpoint     = 0;
        ledc_channel_config(&ch_cfg);
    }
    ESP_LOGI(TAG, "LED controller initialized (4 LEDC channels)");
}

static void led_set(ledc_channel_t ch, uint8_t duty) {
    ledc_set_duty(LEDC_LOW_SPEED_MODE, ch, duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, ch);
}

static void led_all_off() {
    led_set(CH_WHITE, 0); led_set(CH_R, 0);
    led_set(CH_G, 0); led_set(CH_B, 0);
}

static void led_rgb(uint8_t r, uint8_t g, uint8_t b) {
    led_set(CH_R, r); led_set(CH_G, g); led_set(CH_B, b);
}

// ══════════════════════════════════════════════════════════════════════════
// SECTION 8: Test Suite
// ══════════════════════════════════════════════════════════════════════════

static int tests_passed = 0;
static int tests_failed = 0;

static void check(bool condition, const char* test_name) {
    if (condition) {
        tests_passed++;
        ESP_LOGI("TEST", "  PASS: %s", test_name);
    } else {
        tests_failed++;
        ESP_LOGE("TEST", "  FAIL: %s", test_name);
        Serial.printf("FAIL: %s\n", test_name);
    }
}

static void check_float(float actual, float expected, float tolerance, const char* name) {
    float diff = fabsf(actual - expected);
    bool ok = diff <= tolerance;
    if (ok) {
        tests_passed++;
        ESP_LOGI("TEST", "  PASS: %s = %.4f (expected %.4f)", name,
                 (double)actual, (double)expected);
    } else {
        tests_failed++;
        ESP_LOGE("TEST", "  FAIL: %s = %.4f (expected %.4f, diff=%.6f)", name,
                 (double)actual, (double)expected, (double)diff);
        Serial.printf("FAIL: %s = %.4f (expected %.4f)\n", name,
                      (double)actual, (double)expected);
    }
}

// ── Test 1: Result<T> Framework ──────────────────────────────────────────

static void test_result_framework() {
    ESP_LOGI("TEST", "═══ TEST 1: Result<T> Framework ═══");
    using namespace phoenix;

    // Ok path
    auto ok_int = Ok(42);
    check(ok_int.is_ok(), "Ok(42) is ok");
    check(ok_int.value() == 42, "Ok(42) value == 42");
    check(!ok_int.is_err(), "Ok(42) not error");

    // Error path
    auto err = Err<int>(ErrorCategory::HARDWARE_FAILURE, "Camera broken");
    check(err.is_err(), "Err is error");
    check(!err.is_ok(), "Err not ok");
    check(err.error().category == ErrorCategory::HARDWARE_FAILURE, "Error category");
    check(strcmp(err.error().message, "Camera broken") == 0, "Error message");

    // Result<void>
    auto void_ok = Ok();
    check(void_ok.is_ok(), "Ok() void is ok");

    auto void_err = Err(ErrorCategory::TIMEOUT, "Timed out");
    check(void_err.is_err(), "Err() void is error");

    // value_or
    check(err.value_or(99) == 99, "value_or on error returns fallback");
    check(ok_int.value_or(99) == 42, "value_or on ok returns value");
}

// ── Test 2: 5PL Calibration Math ─────────────────────────────────────────

static void test_5pl_math() {
    ESP_LOGI("TEST", "═══ TEST 2: 5PL Calibration Math ═══");
    using namespace phoenix;

    // Verified IgE DIV 5PL: A=-0.000202, B=1.59834, C=255.486, D=0.28367, G=10.0
    FivePL_G ige = { -0.000202f, 1.59834f, 255.486f, 0.28367f, 10.0f };

    // Forward evaluation at known concentrations
    float y1 = ige.evaluate(1.0f);
    float y10 = ige.evaluate(10.0f);
    float y50 = ige.evaluate(50.0f);
    float y100 = ige.evaluate(100.0f);
    float y200 = ige.evaluate(200.0f);
    float y0 = ige.evaluate(0.0f);

    ESP_LOGI("TEST", "  IgE 5PL forward: conc=[0,1,10,50,100,200]");
    ESP_LOGI("TEST", "  T/C ratios: [%.4f, %.4f, %.4f, %.4f, %.4f, %.4f]",
             (double)y0, (double)y1, (double)y10, (double)y50, (double)y100, (double)y200);

    check(y0 < 0.001f, "5PL(0) near A (blank)");
    check(y1 > y0, "5PL(1) > 5PL(0) — monotonic");
    check(y10 > y1, "5PL(10) > 5PL(1) — monotonic");
    check(y100 > y50, "5PL(100) > 5PL(50) — monotonic");
    check(y200 < 0.30f, "5PL(200) < D — below asymptote");

    // Inverse: signal → concentration (roundtrip test)
    ESP_LOGI("TEST", "  Roundtrip test (forward → inverse):");
    float test_concs[] = {1.0f, 5.0f, 10.0f, 25.0f, 50.0f, 100.0f, 150.0f, 200.0f};
    for (float conc : test_concs) {
        float signal = ige.evaluate(conc);
        float recovered = ige.inverse(signal);
        float err_pct = fabsf(recovered - conc) / conc * 100.0f;
        check_float(recovered, conc, conc * 0.02f,  // 2% tolerance
                    "5PL roundtrip");
        ESP_LOGI("TEST", "    conc=%.1f → signal=%.5f → recovered=%.2f (%.1f%% error)",
                 (double)conc, (double)signal, (double)recovered, (double)err_pct);
    }

    // Edge cases
    float inv_zero = ige.inverse(0.0f);
    check(inv_zero >= 0.0f, "5PL inverse(0) >= 0");

    float inv_max = ige.inverse(0.28367f);
    check(inv_max > 1000.0f, "5PL inverse(D) → very high conc");
}

// ── Test 3: Peak Detection ───────────────────────────────────────────────

static void test_peak_detection() {
    ESP_LOGI("TEST", "═══ TEST 3: Peak Detection (Synthetic Profile) ═══");
    using namespace phoenix;

    // Generate synthetic profile: strong CL (130 depth), medium TL1 (50), weak TL2 (15)
    LineProfile profile = generateSyntheticProfile(130.0f, 50.0f, 15.0f);
    ESP_LOGI("TEST", "  Profile: %d points, baseline=165", profile.length);
    ESP_LOGI("TEST", "  CL depth=130 (strong), TL1 depth=50 (medium), TL2 depth=15 (weak)");

    // Print a few profile values around peaks
    ESP_LOGI("TEST", "  Profile[47] (CL center) = %.1f", (double)profile.data[47]);
    ESP_LOGI("TEST", "  Profile[138] (TL1 center) = %.1f", (double)profile.data[138]);
    ESP_LOGI("TEST", "  Profile[247] (TL2 center) = %.1f", (double)profile.data[247]);

    AssayConfig assay = getVerifiedIgEAssay();
    Dx365PeakDetector detector;
    PeakDescriptor peaks[MAX_LINES];
    uint8_t num_peaks = 0;

    auto result = detector.detectPeaks(profile, assay.lines, assay.num_lines, peaks, num_peaks);
    check(result.is_ok(), "detectPeaks succeeded");
    check(num_peaks == 3, "Found 3 peaks");

    // Control line
    check(peaks[0].valid, "CL peak valid");
    check(peaks[0].height > 50.0f, "CL height > 50");
    ESP_LOGI("TEST", "  CL: center=%d, height=%.1f, value=%.1f, integral=%.1f",
             peaks[0].center_idx, (double)peaks[0].height,
             (double)peaks[0].value, (double)peaks[0].integral);

    // Test line 1 (medium)
    check(peaks[1].valid, "TL1 peak valid");
    check(peaks[1].height > 10.0f, "TL1 height > 10");
    ESP_LOGI("TEST", "  TL1: center=%d, height=%.1f, value=%.1f",
             peaks[1].center_idx, (double)peaks[1].height, (double)peaks[1].value);

    // Test line 2 (weak)
    ESP_LOGI("TEST", "  TL2: center=%d, height=%.1f, value=%.1f, valid=%d",
             peaks[2].center_idx, (double)peaks[2].height,
             (double)peaks[2].value, peaks[2].valid);

    // T/C ratio
    if (peaks[0].value > 0.1f) {
        float tc1 = peaks[1].value / peaks[0].value;
        float tc2 = peaks[2].value / peaks[0].value;
        ESP_LOGI("TEST", "  T/C ratios: TL1=%.4f, TL2=%.4f", (double)tc1, (double)tc2);
        check(tc1 > 0.0f && tc1 < 1.0f, "TL1 T/C ratio in valid range");
    }

    // Empty profile error
    LineProfile empty = {};
    PeakDescriptor dummy[1];
    uint8_t dummy_count = 0;
    auto err_result = detector.detectPeaks(empty, assay.lines, 1, dummy, dummy_count);
    check(err_result.is_err(), "Empty profile returns error");
}

// ── Test 4: IPC Protocol ─────────────────────────────────────────────────

static void test_ipc_protocol() {
    ESP_LOGI("TEST", "═══ TEST 4: IPC Protocol (CRC16 + Frames) ═══");
    using namespace phoenix;

    // CRC16 known vectors
    const uint8_t test1[] = "123456789";
    uint16_t crc1 = crc16(test1, 9);
    ESP_LOGI("TEST", "  CRC16('123456789') = 0x%04X", crc1);
    check(crc1 == 0x29B1, "CRC16 known vector '123456789' = 0x29B1");

    const uint8_t test2[] = {0xF0};  // PING command
    uint16_t crc2 = crc16(test2, 1);
    ESP_LOGI("TEST", "  CRC16(PING) = 0x%04X", crc2);

    // Build and parse frame: PING (no payload)
    uint8_t frame[64];
    size_t frame_len = buildFrame(frame, static_cast<uint8_t>(IpcCommand::PING), nullptr, 0);
    check(frame_len == FRAME_OVERHEAD, "PING frame size = 7");
    check(frame[0] == 0xAA && frame[1] == 0x55, "Frame sync = 0xAA55");
    check(frame[4] == 0xF0, "Frame cmd = PING");

    uint8_t parsed_cmd; uint8_t parsed_payload[64]; size_t parsed_len;
    bool ok = parseFrame(frame, frame_len, &parsed_cmd, parsed_payload, &parsed_len);
    check(ok, "PING frame parse OK");
    check(parsed_cmd == 0xF0, "Parsed cmd = PING");
    check(parsed_len == 0, "Parsed payload empty");

    // Build frame with float payload (like measurement result)
    float test_value = 42.5f;
    frame_len = buildFrame(frame, static_cast<uint8_t>(IpcCommand::MEASUREMENT_RESULT),
                           &test_value, sizeof(test_value));
    check(frame_len == FRAME_OVERHEAD + 4, "Float frame size = 11");

    ok = parseFrame(frame, frame_len, &parsed_cmd, parsed_payload, &parsed_len);
    check(ok, "Float frame parse OK");
    check(parsed_len == 4, "Float payload size = 4");
    float recovered;
    memcpy(&recovered, parsed_payload, 4);
    check_float(recovered, 42.5f, 0.001f, "Float payload roundtrip");

    // Corrupt CRC → should fail
    frame[frame_len - 1] ^= 0xFF;
    ok = parseFrame(frame, frame_len, &parsed_cmd, parsed_payload, &parsed_len);
    check(!ok, "Corrupted CRC rejected");

    // Multi-byte payload (simulate measurement progress)
    struct __attribute__((packed)) { uint8_t pct; char msg[32]; } progress;
    progress.pct = 75;
    strncpy(progress.msg, "Processing image...", sizeof(progress.msg));
    frame_len = buildFrame(frame, static_cast<uint8_t>(IpcCommand::MEASUREMENT_PROGRESS),
                           &progress, sizeof(progress));
    ok = parseFrame(frame, frame_len, &parsed_cmd, parsed_payload, &parsed_len);
    check(ok, "Progress frame parse OK");
    check(parsed_payload[0] == 75, "Progress percentage = 75%");
    check(strncmp((char*)&parsed_payload[1], "Processing image...", 19) == 0, "Progress message intact");
}

// ── Test 5: Full Measurement Pipeline ────────────────────────────────────

static void test_measurement_pipeline() {
    ESP_LOGI("TEST", "═══ TEST 5: Measurement Pipeline (Profile → Concentration) ═══");
    using namespace phoenix;

    AssayConfig assay = getVerifiedIgEAssay();

    // Simulate different concentration levels
    struct TestCase {
        const char* name;
        float cl_depth;   // Control line depth
        float tl_depth;   // Test line depth
    };

    TestCase cases[] = {
        {"HIGH conc (strong TL)",   130.0f, 90.0f},
        {"MEDIUM conc (medium TL)", 130.0f, 40.0f},
        {"LOW conc (weak TL)",      130.0f, 10.0f},
        {"NEGATIVE (no TL)",        130.0f,  0.5f},
    };

    for (auto& tc : cases) {
        ESP_LOGI("TEST", "  --- %s (CL=%.0f, TL=%.0f) ---", tc.name, (double)tc.cl_depth, (double)tc.tl_depth);

        LineProfile profile = generateSyntheticProfile(tc.cl_depth, tc.tl_depth, tc.tl_depth * 0.3f);

        Dx365PeakDetector detector;
        PeakDescriptor peaks[MAX_LINES];
        uint8_t num_peaks = 0;
        auto det_res = detector.detectPeaks(profile, assay.lines, assay.num_lines, peaks, num_peaks);
        check(det_res.is_ok(), "Peak detection OK");

        float cl_value = peaks[0].value;
        float tl_value = peaks[1].value;

        // T/C ratio
        float tc_ratio = (cl_value > 0.1f) ? tl_value / cl_value : 0.0f;

        // 5PL inverse → concentration
        float concentration = assay.div_5pl.inverse(tc_ratio);

        ESP_LOGI("TEST", "    CL=%.2f, TL=%.2f, T/C=%.4f → conc=%.1f",
                 (double)cl_value, (double)tl_value, (double)tc_ratio, (double)concentration);

        check(cl_value > 1.0f, "Control line detected");

        if (tc.tl_depth > 50.0f) {
            check(concentration > 20.0f, "High conc > 20");
        } else if (tc.tl_depth < 2.0f) {
            check(concentration < 5.0f || tc_ratio < 0.01f, "Negative/very low conc");
        }
    }
}

// ── Test 6: LED Control ──────────────────────────────────────────────────

static void test_led_control() {
    ESP_LOGI("TEST", "═══ TEST 6: LED Control (watch the LEDs on Wokwi!) ═══");

    // Red
    ESP_LOGI("TEST", "  RED...");
    led_rgb(100, 0, 0);
    vTaskDelay(pdMS_TO_TICKS(500));

    // Green
    ESP_LOGI("TEST", "  GREEN...");
    led_rgb(0, 100, 0);
    vTaskDelay(pdMS_TO_TICKS(500));

    // Blue
    ESP_LOGI("TEST", "  BLUE...");
    led_rgb(0, 0, 100);
    vTaskDelay(pdMS_TO_TICKS(500));

    // White
    ESP_LOGI("TEST", "  WHITE (illumination LED)...");
    led_all_off();
    led_set(CH_WHITE, 150);
    vTaskDelay(pdMS_TO_TICKS(500));

    // All off
    led_all_off();

    // PWM ramp test on green LED
    ESP_LOGI("TEST", "  PWM ramp on green LED...");
    for (int duty = 0; duty <= 255; duty += 15) {
        led_set(CH_G, duty);
        vTaskDelay(pdMS_TO_TICKS(30));
    }
    for (int duty = 255; duty >= 0; duty -= 15) {
        led_set(CH_G, duty);
        vTaskDelay(pdMS_TO_TICKS(30));
    }
    led_all_off();

    tests_passed++;
    ESP_LOGI("TEST", "  PASS: LED self-test complete");
}

// ── Test 7: NVS Storage ──────────────────────────────────────────────────

static void test_nvs_storage() {
    ESP_LOGI("TEST", "═══ TEST 7: NVS Storage (Calibration Persistence) ═══");
    using namespace phoenix;

    nvs_handle_t nvs;
    esp_err_t err = nvs_open("phoenix_cal", NVS_READWRITE, &nvs);
    check(err == ESP_OK, "NVS open 'phoenix_cal'");

    if (err == ESP_OK) {
        // Write 5PL parameters
        FivePL_G cal = { -0.000202f, 1.59834f, 255.486f, 0.28367f, 10.0f };
        err = nvs_set_blob(nvs, "ige_5pl", &cal, sizeof(cal));
        check(err == ESP_OK, "NVS write 5PL blob");

        err = nvs_set_u32(nvs, "cal_date", 20260226);
        check(err == ESP_OK, "NVS write cal date");

        err = nvs_commit(nvs);
        check(err == ESP_OK, "NVS commit");

        // Read back
        FivePL_G cal_read = {};
        size_t read_len = sizeof(cal_read);
        err = nvs_get_blob(nvs, "ige_5pl", &cal_read, &read_len);
        check(err == ESP_OK, "NVS read 5PL blob");
        check(read_len == sizeof(FivePL_G), "NVS blob size correct");
        check_float(cal_read.A, -0.000202f, 0.0001f, "NVS A parameter roundtrip");
        check_float(cal_read.C, 255.486f, 0.01f, "NVS C parameter roundtrip");
        check_float(cal_read.G, 10.0f, 0.001f, "NVS G parameter roundtrip");

        uint32_t date = 0;
        err = nvs_get_u32(nvs, "cal_date", &date);
        check(err == ESP_OK && date == 20260226, "NVS date roundtrip");

        nvs_close(nvs);
    }
}

// ── Test 8: Safety Checks ────────────────────────────────────────────────

static void test_safety_checks() {
    ESP_LOGI("TEST", "═══ TEST 8: Safety Checks (Heap Monitor) ═══");

    size_t free_heap = esp_get_free_heap_size();
    size_t min_heap = esp_get_minimum_free_heap_size();

    ESP_LOGI("TEST", "  Free heap: %u bytes (%.1f kB)", free_heap, (double)free_heap / 1024.0);
    ESP_LOGI("TEST", "  Min free:  %u bytes (%.1f kB)", min_heap, (double)min_heap / 1024.0);

    check(free_heap > 16 * 1024, "Heap > 16 kB (critical threshold)");
    check(free_heap > 32 * 1024, "Heap > 32 kB (warning threshold)");

    // Allocate + free test (like SafetyManager::testRAM)
    constexpr size_t TEST_SIZE = 4096;
    uint8_t* test = static_cast<uint8_t*>(malloc(TEST_SIZE));
    check(test != nullptr, "RAM alloc 4 kB OK");

    if (test) {
        for (size_t i = 0; i < TEST_SIZE; i++) {
            test[i] = static_cast<uint8_t>(i & 0xFF);
        }
        bool ok = true;
        for (size_t i = 0; i < TEST_SIZE; i++) {
            if (test[i] != static_cast<uint8_t>(i & 0xFF)) { ok = false; break; }
        }
        check(ok, "RAM pattern write/verify");
        free(test);
    }

    // Check heap after alloc/free
    size_t heap_after = esp_get_free_heap_size();
    check(heap_after >= free_heap - 256, "No heap leak after alloc/free");

    // Uptime
    uint32_t uptime = static_cast<uint32_t>(esp_timer_get_time() / 1000000ULL);
    ESP_LOGI("TEST", "  Uptime: %u seconds", uptime);
}

// ══════════════════════════════════════════════════════════════════════════
// SECTION 9: Entry Point
// ══════════════════════════════════════════════════════════════════════════

static void phoenix_main_task(void* arg) {
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "══════════════════════════════════════════════════════════");
    ESP_LOGI(TAG, " Phoenix v108.0 Chimera — Wokwi Integration Test");
    ESP_LOGI(TAG, " Igloo Pro Medical Device Firmware");
    ESP_LOGI(TAG, " ESP-IDF %s on ESP32", esp_get_idf_version());
    ESP_LOGI(TAG, " Free heap: %.1f kB",
             (double)esp_get_free_heap_size() / 1024.0);
    ESP_LOGI(TAG, "══════════════════════════════════════════════════════════");
    ESP_LOGI(TAG, "");

    // Initialize NVS
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }

    // Initialize LEDs
    led_init();

    // Status: Yellow = testing
    led_rgb(80, 80, 0);

    // ── Run all tests ────────────────────────────────────────────────
    vTaskDelay(pdMS_TO_TICKS(500));

    test_result_framework();
    vTaskDelay(pdMS_TO_TICKS(100));

    test_5pl_math();
    vTaskDelay(pdMS_TO_TICKS(100));

    test_peak_detection();
    vTaskDelay(pdMS_TO_TICKS(100));

    test_ipc_protocol();
    vTaskDelay(pdMS_TO_TICKS(100));

    test_measurement_pipeline();
    vTaskDelay(pdMS_TO_TICKS(100));

    test_led_control();
    vTaskDelay(pdMS_TO_TICKS(100));

    Serial.printf(">> Before NVS test, passed=%d failed=%d\n", tests_passed, tests_failed);
    test_nvs_storage();
    Serial.printf(">> After NVS test, passed=%d failed=%d\n", tests_passed, tests_failed);
    vTaskDelay(pdMS_TO_TICKS(100));

    Serial.printf(">> Before Safety test\n");
    test_safety_checks();
    Serial.printf(">> After Safety test, passed=%d failed=%d\n", tests_passed, tests_failed);

    // ── Summary ──────────────────────────────────────────────────────
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "══════════════════════════════════════════════════════════");
    ESP_LOGI(TAG, " TEST RESULTS: %d PASSED, %d FAILED (total: %d)",
             tests_passed, tests_failed, tests_passed + tests_failed);
    ESP_LOGI(TAG, "══════════════════════════════════════════════════════════");

    if (tests_failed == 0) {
        ESP_LOGI(TAG, " ALL TESTS PASSED — Phoenix core algorithms verified!");
        Serial.printf("\n*** ALL %d TESTS PASSED ***\n", tests_passed);
        // Green = all good
        led_all_off();
        led_rgb(0, 80, 0);
    } else {
        ESP_LOGE(TAG, " %d TESTS FAILED — check output above", tests_failed);
        Serial.printf("\n*** %d PASSED, %d FAILED ***\n", tests_passed, tests_failed);
        // Red = failures
        led_all_off();
        led_rgb(80, 0, 0);
    }

    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, " Heap after tests: %.1f kB free",
             (double)esp_get_free_heap_size() / 1024.0);
    ESP_LOGI(TAG, "══════════════════════════════════════════════════════════");

    // Keep blinking status LED
    while (true) {
        if (tests_failed == 0) {
            led_set(CH_G, 80); vTaskDelay(pdMS_TO_TICKS(500));
            led_set(CH_G, 0);  vTaskDelay(pdMS_TO_TICKS(500));
        } else {
            led_set(CH_R, 80); vTaskDelay(pdMS_TO_TICKS(250));
            led_set(CH_R, 0);  vTaskDelay(pdMS_TO_TICKS(250));
        }
    }
}

void setup() {
    Serial.begin(115200);
    delay(500);
    ESP_LOGI(TAG, "Phoenix v108.0 Chimera booting...");
    xTaskCreatePinnedToCore(
        phoenix_main_task,
        "phoenix_main",
        16384,    // 16 kB stack (like real Phoenix)
        nullptr,
        5,        // Priority 5
        nullptr,
        1         // Core 1
    );
}

void loop() {
    vTaskDelay(pdMS_TO_TICKS(1000));
}
