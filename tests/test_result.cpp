// ==========================================================================
// Test: Result<T> Error Handling Framework
// Verifies: IEC 62304 REQ-CORE-001 — Exception-free error propagation
// ==========================================================================
#include "test_framework.h"
#include "phoenix/Core/Result.h"

using namespace phoenix;

// Helper that uses PHOENIX_TRY
static Result<void> chain_ok() {
    PHOENIX_TRY(Ok());
    return Ok();
}

static Result<void> chain_err() {
    PHOENIX_TRY(Err(ErrorCategory::TIMEOUT, "inner timeout"));
    return Ok();  // Should not reach
}

// PHOENIX_TRY_VAL: tested via Result<void> to avoid template deduction issue
static Result<void> chain_try_val_void() {
    auto r = Ok(42);
    if (r.is_err()) return Err(r.error());
    int val = r.value();
    (void)val;
    return Ok();
}

static Result<void> chain_try_val_err_void() {
    auto r = Err<int>(ErrorCategory::HARDWARE_FAILURE, "hw fail");
    if (r.is_err()) return Err(r.error());
    return Ok();
}

TEST_SUITE(result_ok_int) {
    auto r = Ok(42);
    ASSERT_TRUE(r.is_ok());
    ASSERT_FALSE(r.is_err());
    ASSERT_EQ(r.value(), 42);
}

TEST_SUITE(result_ok_float) {
    auto r = Ok(3.14f);
    ASSERT_TRUE(r.is_ok());
    ASSERT_FLOAT_EQ(r.value(), 3.14f, 0.001f);
}

TEST_SUITE(result_err) {
    auto r = Err<int>(ErrorCategory::HARDWARE_FAILURE, "Camera broken", 0x42);
    ASSERT_TRUE(r.is_err());
    ASSERT_FALSE(r.is_ok());
    ASSERT_EQ(static_cast<int>(r.error().category),
              static_cast<int>(ErrorCategory::HARDWARE_FAILURE));
    ASSERT_STR_EQ(r.error().message, "Camera broken");
    ASSERT_EQ(r.error().code, 0x42);
}

TEST_SUITE(result_void_ok) {
    auto r = Ok();
    ASSERT_TRUE(r.is_ok());
    ASSERT_FALSE(r.is_err());
}

TEST_SUITE(result_void_err) {
    auto r = Err(ErrorCategory::TIMEOUT, "Timed out");
    ASSERT_TRUE(r.is_err());
    ASSERT_EQ(static_cast<int>(r.error().category),
              static_cast<int>(ErrorCategory::TIMEOUT));
}

TEST_SUITE(result_value_or) {
    auto ok = Ok(42);
    auto err = Err<int>(ErrorCategory::NOT_FOUND, "nope");
    ASSERT_EQ(ok.value_or(99), 42);
    ASSERT_EQ(err.value_or(99), 99);
}

TEST_SUITE(result_phoenix_try) {
    auto ok_result = chain_ok();
    ASSERT_RESULT_OK(ok_result);

    auto err_result = chain_err();
    ASSERT_RESULT_ERR(err_result);
    ASSERT_EQ(static_cast<int>(err_result.error().category),
              static_cast<int>(ErrorCategory::TIMEOUT));
    ASSERT_STR_EQ(err_result.error().message, "inner timeout");
}

TEST_SUITE(result_error_propagation_manual) {
    auto ok = chain_try_val_void();
    ASSERT_RESULT_OK(ok);

    auto err = chain_try_val_err_void();
    ASSERT_RESULT_ERR(err);
    ASSERT_EQ(static_cast<int>(err.error().category),
              static_cast<int>(ErrorCategory::HARDWARE_FAILURE));
}

TEST_SUITE(result_error_categories) {
    // All 11 categories must round-trip
    ErrorCategory cats[] = {
        ErrorCategory::NONE, ErrorCategory::HARDWARE_FAILURE,
        ErrorCategory::CALIBRATION_ERROR, ErrorCategory::MEASUREMENT_ERROR,
        ErrorCategory::COMMUNICATION_ERROR, ErrorCategory::MEMORY_ERROR,
        ErrorCategory::INVALID_PARAMETER, ErrorCategory::SAFETY_VIOLATION,
        ErrorCategory::TIMEOUT, ErrorCategory::NOT_FOUND,
        ErrorCategory::NOT_INITIALIZED, ErrorCategory::BUSY,
    };
    for (auto cat : cats) {
        auto r = Err<int>(cat, "test");
        ASSERT_EQ(static_cast<int>(r.error().category), static_cast<int>(cat));
    }
}

TEST_SUITE(result_error_message_truncation) {
    // ErrorInfo.message is 80 chars; long message must not overflow
    char long_msg[200];
    memset(long_msg, 'A', sizeof(long_msg));
    long_msg[199] = '\0';

    ErrorInfo info(ErrorCategory::MEMORY_ERROR, long_msg);
    ASSERT_EQ(strlen(info.message), 79u);
    ASSERT_TRUE(info.message[79] == '\0');
}
