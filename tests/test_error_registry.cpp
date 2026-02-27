// ==========================================================================
// Test: ErrorRegistry — Persistent Error Tracking
// Verifies: IEC 62304 REQ-ERR-001 — Record and persist all device errors
// ==========================================================================
#include "test_framework.h"
#include "phoenix/Core/ErrorRegistry.h"

using namespace phoenix;

// ── Basic registration ──────────────────────────────────────────────────

TEST_SUITE(error_registry_singleton) {
    auto& a = ErrorRegistry::instance();
    auto& b = ErrorRegistry::instance();
    ASSERT_TRUE(&a == &b);
}

TEST_SUITE(error_registry_initial_empty) {
    auto& reg = ErrorRegistry::instance();
    reg.clearErrors();
    ASSERT_EQ(reg.getErrorCount(), 0);
    ASSERT_FALSE(reg.hasCriticalErrors());
}

TEST_SUITE(error_registry_register_single) {
    auto& reg = ErrorRegistry::instance();
    reg.clearErrors();

    reg.registerError(ErrorCode::HW_CAMERA_INIT_FAIL,
                      ErrorSeverity::HIGH,
                      ErrorCategory::HARDWARE_FAILURE,
                      "Camera init failed",
                      "Check CSI ribbon cable");

    ASSERT_EQ(reg.getErrorCount(), 1);
    const ErrorEntry* e = reg.findByCode(ErrorCode::HW_CAMERA_INIT_FAIL);
    ASSERT_TRUE(e != nullptr);
    ASSERT_STR_EQ(e->message.c_str(), "Camera init failed");
    ASSERT_STR_EQ(e->recovery_hint.c_str(), "Check CSI ribbon cable");
    ASSERT_EQ(e->occurrence_count, 1);
    ASSERT_TRUE(e->severity == ErrorSeverity::HIGH);
    ASSERT_TRUE(e->category == ErrorCategory::HARDWARE_FAILURE);
}

// ── Deduplication ───────────────────────────────────────────────────────

TEST_SUITE(error_registry_deduplication) {
    auto& reg = ErrorRegistry::instance();
    reg.clearErrors();

    reg.registerError(ErrorCode::COMM_UART_CRC_ERROR,
                      ErrorSeverity::MEDIUM,
                      ErrorCategory::COMMUNICATION_ERROR,
                      "CRC mismatch");

    reg.registerError(ErrorCode::COMM_UART_CRC_ERROR,
                      ErrorSeverity::MEDIUM,
                      ErrorCategory::COMMUNICATION_ERROR,
                      "CRC mismatch");

    reg.registerError(ErrorCode::COMM_UART_CRC_ERROR,
                      ErrorSeverity::MEDIUM,
                      ErrorCategory::COMMUNICATION_ERROR,
                      "CRC mismatch");

    // Should still be 1 entry with count=3
    ASSERT_EQ(reg.getErrorCount(), 1);
    const ErrorEntry* e = reg.findByCode(ErrorCode::COMM_UART_CRC_ERROR);
    ASSERT_TRUE(e != nullptr);
    ASSERT_EQ(e->occurrence_count, 3);
}

TEST_SUITE(error_registry_severity_escalation) {
    auto& reg = ErrorRegistry::instance();
    reg.clearErrors();

    reg.registerError(ErrorCode::HW_TEMP_OVER_LIMIT,
                      ErrorSeverity::MEDIUM,
                      ErrorCategory::HARDWARE_FAILURE,
                      "Temperature high");

    reg.registerError(ErrorCode::HW_TEMP_OVER_LIMIT,
                      ErrorSeverity::CRITICAL,
                      ErrorCategory::HARDWARE_FAILURE,
                      "Temperature critical");

    const ErrorEntry* e = reg.findByCode(ErrorCode::HW_TEMP_OVER_LIMIT);
    ASSERT_TRUE(e != nullptr);
    ASSERT_TRUE(e->severity == ErrorSeverity::CRITICAL);
    ASSERT_EQ(e->occurrence_count, 2);
}

// ── Multiple errors ─────────────────────────────────────────────────────

TEST_SUITE(error_registry_multiple_codes) {
    auto& reg = ErrorRegistry::instance();
    reg.clearErrors();

    reg.registerError(ErrorCode::HW_CAMERA_INIT_FAIL,
                      ErrorSeverity::HIGH,
                      ErrorCategory::HARDWARE_FAILURE,
                      "Camera fail");

    reg.registerError(ErrorCode::CAL_R2_TOO_LOW,
                      ErrorSeverity::MEDIUM,
                      ErrorCategory::CALIBRATION_ERROR,
                      "Bad R2");

    reg.registerError(ErrorCode::MEAS_CONTROL_LINE_WEAK,
                      ErrorSeverity::LOW,
                      ErrorCategory::MEASUREMENT_ERROR,
                      "CL weak");

    ASSERT_EQ(reg.getErrorCount(), 3);
    ASSERT_TRUE(reg.findByCode(ErrorCode::HW_CAMERA_INIT_FAIL) != nullptr);
    ASSERT_TRUE(reg.findByCode(ErrorCode::CAL_R2_TOO_LOW) != nullptr);
    ASSERT_TRUE(reg.findByCode(ErrorCode::MEAS_CONTROL_LINE_WEAK) != nullptr);
    ASSERT_TRUE(reg.findByCode(0x9999) == nullptr); // Not registered
}

// ── Count by severity ───────────────────────────────────────────────────

TEST_SUITE(error_registry_count_by_severity) {
    auto& reg = ErrorRegistry::instance();
    reg.clearErrors();

    reg.registerError(0x0001, ErrorSeverity::LOW, ErrorCategory::NONE, "Low1");
    reg.registerError(0x0002, ErrorSeverity::LOW, ErrorCategory::NONE, "Low2");
    reg.registerError(0x0003, ErrorSeverity::MEDIUM, ErrorCategory::NONE, "Med");
    reg.registerError(0x0004, ErrorSeverity::HIGH, ErrorCategory::NONE, "High");
    reg.registerError(0x0005, ErrorSeverity::CRITICAL, ErrorCategory::NONE, "Crit");

    ASSERT_EQ(reg.countBySeverity(ErrorSeverity::LOW), 5);
    ASSERT_EQ(reg.countBySeverity(ErrorSeverity::MEDIUM), 3);
    ASSERT_EQ(reg.countBySeverity(ErrorSeverity::HIGH), 2);
    ASSERT_EQ(reg.countBySeverity(ErrorSeverity::CRITICAL), 1);
    ASSERT_TRUE(reg.hasCriticalErrors());
}

// ── Count by category ───────────────────────────────────────────────────

TEST_SUITE(error_registry_count_by_category) {
    auto& reg = ErrorRegistry::instance();
    reg.clearErrors();

    reg.registerError(ErrorCode::HW_CAMERA_INIT_FAIL,
                      ErrorSeverity::HIGH, ErrorCategory::HARDWARE_FAILURE, "HW1");
    reg.registerError(ErrorCode::HW_LED_FAILURE,
                      ErrorSeverity::MEDIUM, ErrorCategory::HARDWARE_FAILURE, "HW2");
    reg.registerError(ErrorCode::CAL_FIT_DIVERGED,
                      ErrorSeverity::MEDIUM, ErrorCategory::CALIBRATION_ERROR, "CAL1");
    reg.registerError(ErrorCode::COMM_UART_CRC_ERROR,
                      ErrorSeverity::LOW, ErrorCategory::COMMUNICATION_ERROR, "COMM1");

    ASSERT_EQ(reg.countByCategory(ErrorCategory::HARDWARE_FAILURE), 2);
    ASSERT_EQ(reg.countByCategory(ErrorCategory::CALIBRATION_ERROR), 1);
    ASSERT_EQ(reg.countByCategory(ErrorCategory::COMMUNICATION_ERROR), 1);
    ASSERT_EQ(reg.countByCategory(ErrorCategory::MEASUREMENT_ERROR), 0);
}

// ── Error code ranges ───────────────────────────────────────────────────

TEST_SUITE(error_code_ranges) {
    // Hardware: 0x0100-0x01FF
    ASSERT_TRUE(ErrorCode::HW_CAMERA_INIT_FAIL >= 0x0100);
    ASSERT_TRUE(ErrorCode::HW_FLASH_FAILURE <= 0x01FF);

    // Calibration: 0x0200-0x02FF
    ASSERT_TRUE(ErrorCode::CAL_FIT_DIVERGED >= 0x0200);
    ASSERT_TRUE(ErrorCode::CAL_INSUFFICIENT_POINTS <= 0x02FF);

    // Measurement: 0x0300-0x03FF
    ASSERT_TRUE(ErrorCode::MEAS_PROFILE_INVALID >= 0x0300);
    ASSERT_TRUE(ErrorCode::MEAS_ROI_INVALID <= 0x03FF);

    // Communication: 0x0400-0x04FF
    ASSERT_TRUE(ErrorCode::COMM_UART_CRC_ERROR >= 0x0400);
    ASSERT_TRUE(ErrorCode::COMM_IPC_NACK <= 0x04FF);

    // Safety: 0x0500-0x05FF
    ASSERT_TRUE(ErrorCode::SAFE_POST_FAIL >= 0x0500);
    ASSERT_TRUE(ErrorCode::SAFE_EMERGENCY_SHUTDOWN <= 0x05FF);
}

// ── Clear ───────────────────────────────────────────────────────────────

TEST_SUITE(error_registry_clear) {
    auto& reg = ErrorRegistry::instance();
    reg.registerError(ErrorCode::SAFE_POST_FAIL,
                      ErrorSeverity::CRITICAL, ErrorCategory::SAFETY_VIOLATION,
                      "POST fail");
    ASSERT_TRUE(reg.getErrorCount() > 0);

    reg.clearErrors();
    ASSERT_EQ(reg.getErrorCount(), 0);
    ASSERT_FALSE(reg.hasCriticalErrors());
    ASSERT_TRUE(reg.findByCode(ErrorCode::SAFE_POST_FAIL) == nullptr);
}

// ── Buffer full behavior ────────────────────────────────────────────────

TEST_SUITE(error_registry_buffer_full) {
    auto& reg = ErrorRegistry::instance();
    reg.clearErrors();

    // Fill to MAX_ERRORS with LOW severity
    for (int i = 0; i < ErrorRegistry::MAX_ERRORS; i++) {
        reg.registerError(static_cast<uint16_t>(0xF000 + i),
                          ErrorSeverity::LOW, ErrorCategory::NONE, "Fill");
    }
    ASSERT_EQ(reg.getErrorCount(), ErrorRegistry::MAX_ERRORS);

    // One more LOW should overwrite oldest LOW
    reg.registerError(0xFFFF, ErrorSeverity::MEDIUM,
                      ErrorCategory::NONE, "Overflow entry");

    // Count stays at MAX — one LOW was evicted
    ASSERT_EQ(reg.getErrorCount(), ErrorRegistry::MAX_ERRORS);
    ASSERT_TRUE(reg.findByCode(0xFFFF) != nullptr);
}

// ── Severity names ──────────────────────────────────────────────────────

TEST_SUITE(error_severity_names) {
    ASSERT_STR_EQ(severityName(ErrorSeverity::LOW), "LOW");
    ASSERT_STR_EQ(severityName(ErrorSeverity::MEDIUM), "MEDIUM");
    ASSERT_STR_EQ(severityName(ErrorSeverity::HIGH), "HIGH");
    ASSERT_STR_EQ(severityName(ErrorSeverity::CRITICAL), "CRITICAL");
}

// ── ErrorEntry active flag ──────────────────────────────────────────────

TEST_SUITE(error_entry_active_flag) {
    ErrorEntry e;
    ASSERT_FALSE(e.active()); // Default code=0 → inactive

    e.code = ErrorCode::HW_LED_FAILURE;
    ASSERT_TRUE(e.active());
}

// ── No recovery hint ────────────────────────────────────────────────────

TEST_SUITE(error_registry_no_recovery_hint) {
    auto& reg = ErrorRegistry::instance();
    reg.clearErrors();

    reg.registerError(ErrorCode::MEAS_QC_FAIL,
                      ErrorSeverity::MEDIUM,
                      ErrorCategory::MEASUREMENT_ERROR,
                      "QC failed"); // No recovery hint

    const ErrorEntry* e = reg.findByCode(ErrorCode::MEAS_QC_FAIL);
    ASSERT_TRUE(e != nullptr);
    ASSERT_TRUE(e->recovery_hint.empty());
}
