// ==========================================================================
// FILE: include/phoenix/Core/Result.h
// Phoenix Medical Device Firmware v108.0 "Chimera"
// IEC 62304 Class C — Error handling without exceptions
// ==========================================================================
#pragma once

#include <cstdint>
#include <cstring>
#include <utility>

namespace phoenix {

// ─── Error Categories (ISO 14971 risk classification) ─────────────────
enum class ErrorCategory : uint8_t {
    NONE = 0,
    HARDWARE_FAILURE    = 1,   // Camera, LED, sensor
    CALIBRATION_ERROR   = 2,   // Invalid calibration data
    MEASUREMENT_ERROR   = 3,   // Image processing failure
    COMMUNICATION_ERROR = 4,   // UART IPC failure
    MEMORY_ERROR        = 5,   // Allocation failure
    INVALID_PARAMETER   = 6,   // Bad input
    SAFETY_VIOLATION    = 7,   // Safety limit exceeded
    TIMEOUT             = 8,
    NOT_FOUND           = 9,
    NOT_INITIALIZED     = 10,
    BUSY                = 11,
};

// ─── Error Info (fixed-size, no heap) ─────────────────────────────────
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

// ─── Result<T> — Rust-style Result for embedded ───────────────────────
//
// Usage:
//   Result<int> good = Ok(42);
//   Result<int> bad  = Err<int>(ErrorCategory::HARDWARE_FAILURE, "Camera init failed");
//
//   if (good.is_ok()) { int val = good.value(); }
//   if (bad.is_err()) { auto& err = bad.error(); }
//
template <typename T>
class Result {
public:
    // Success constructor
    static Result Ok(T val) {
        Result r;
        r.value_   = val;
        r.has_val_ = true;
        return r;
    }

    // Error constructor
    static Result Err(ErrorCategory cat, const char* msg, uint32_t code = 0) {
        Result r;
        r.error_   = ErrorInfo(cat, msg, code);
        r.has_val_ = false;
        return r;
    }

    static Result Err(ErrorInfo info) {
        Result r;
        r.error_   = info;
        r.has_val_ = false;
        return r;
    }

    [[nodiscard]] bool is_ok()  const { return has_val_; }
    [[nodiscard]] bool is_err() const { return !has_val_; }

    [[nodiscard]] const T& value() const { return value_; }
    [[nodiscard]] T&       value()       { return value_; }

    [[nodiscard]] const ErrorInfo& error() const { return error_; }

    // Monadic: unwrap with default
    [[nodiscard]] T value_or(T fallback) const {
        return has_val_ ? value_ : fallback;
    }

private:
    T         value_   = {};
    ErrorInfo error_   = {};
    bool      has_val_ = false;
};

// ─── Result<void> specialization ──────────────────────────────────────
template <>
class Result<void> {
public:
    static Result Ok() {
        Result r;
        r.ok_ = true;
        return r;
    }

    static Result Err(ErrorCategory cat, const char* msg, uint32_t code = 0) {
        Result r;
        r.error_ = ErrorInfo(cat, msg, code);
        r.ok_    = false;
        return r;
    }

    static Result Err(ErrorInfo info) {
        Result r;
        r.error_ = info;
        r.ok_    = false;
        return r;
    }

    [[nodiscard]] bool is_ok()  const { return ok_; }
    [[nodiscard]] bool is_err() const { return !ok_; }

    [[nodiscard]] const ErrorInfo& error() const { return error_; }

private:
    ErrorInfo error_ = {};
    bool      ok_    = false;
};

// ─── Convenience free functions ───────────────────────────────────────
template <typename T>
Result<T> Ok(T val) { return Result<T>::Ok(val); }

inline Result<void> Ok() { return Result<void>::Ok(); }

template <typename T>
Result<T> Err(ErrorCategory cat, const char* msg, uint32_t code = 0) {
    return Result<T>::Err(cat, msg, code);
}

inline Result<void> Err(ErrorCategory cat, const char* msg, uint32_t code = 0) {
    return Result<void>::Err(cat, msg, code);
}

// ─── PHOENIX_TRY macro — propagate errors like Rust's ? operator ──────
// Usage:
//   Result<void> init() {
//       PHOENIX_TRY(camera.init());
//       PHOENIX_TRY(led.init());
//       return Ok();
//   }
#define PHOENIX_TRY(expr)                     \
    do {                                      \
        auto _r = (expr);                     \
        if (_r.is_err()) return _r;           \
    } while (0)

#define PHOENIX_TRY_VAL(var, expr)            \
    auto _r_##var = (expr);                   \
    if (_r_##var.is_err()) return _r_##var;   \
    auto var = _r_##var.value()

} // namespace phoenix
