// ==========================================================================
// Test: Logger — Structured Logging Ring-Buffer
// Verifies: IEC 62304 REQ-LOG-001 — Audit trail of significant events
// ==========================================================================
#include "test_framework.h"
#include "phoenix/Core/Logger.h"

using namespace phoenix;

// ── Basic logging ───────────────────────────────────────────────────────

TEST_SUITE(logger_singleton_identity) {
    auto& a = Logger::instance();
    auto& b = Logger::instance();
    ASSERT_TRUE(&a == &b);
}

TEST_SUITE(logger_initial_state) {
    auto& log = Logger::instance();
    log.clear();
    ASSERT_EQ(log.getEntryCount(), 0);
    ASSERT_TRUE(log.getEntry(0) == nullptr);
}

TEST_SUITE(logger_single_entry) {
    auto& log = Logger::instance();
    log.clear();
    log.setLevel(LogLevel::TRACE);

    log.log(LogLevel::INFO, "TEST", "Hello %d", 42);

    ASSERT_EQ(log.getEntryCount(), 1);
    const LogEntry* e = log.getEntry(0); // Most recent
    ASSERT_TRUE(e != nullptr);
    ASSERT_STR_EQ(e->tag.c_str(), "TEST");
    ASSERT_STR_EQ(e->message.c_str(), "Hello 42");
    ASSERT_TRUE(e->level == LogLevel::INFO);
}

TEST_SUITE(logger_zero_arg_overload) {
    auto& log = Logger::instance();
    log.clear();
    log.setLevel(LogLevel::TRACE);

    log.log(LogLevel::WARN, "MOD", "No args here");

    ASSERT_EQ(log.getEntryCount(), 1);
    ASSERT_STR_EQ(log.getEntry(0)->message.c_str(), "No args here");
    ASSERT_TRUE(log.getEntry(0)->level == LogLevel::WARN);
}

TEST_SUITE(logger_multiple_entries_ordering) {
    auto& log = Logger::instance();
    log.clear();
    log.setLevel(LogLevel::TRACE);

    log.log(LogLevel::INFO, "A", "First");
    log.log(LogLevel::INFO, "B", "Second");
    log.log(LogLevel::INFO, "C", "Third");

    ASSERT_EQ(log.getEntryCount(), 3);
    ASSERT_STR_EQ(log.getEntry(0)->message.c_str(), "Third");  // Most recent
    ASSERT_STR_EQ(log.getEntry(1)->message.c_str(), "Second");
    ASSERT_STR_EQ(log.getEntry(2)->message.c_str(), "First");  // Oldest
}

// ── Log level filtering ─────────────────────────────────────────────────

TEST_SUITE(logger_level_filtering) {
    auto& log = Logger::instance();
    log.clear();
    log.setLevel(LogLevel::WARN);

    log.log(LogLevel::DEBUG, "X", "Should be filtered");
    log.log(LogLevel::INFO,  "X", "Should be filtered");
    log.log(LogLevel::WARN,  "X", "Should pass");
    log.log(LogLevel::ERROR, "X", "Should pass");

    ASSERT_EQ(log.getEntryCount(), 2);
    ASSERT_STR_EQ(log.getEntry(0)->message.c_str(), "Should pass");
    ASSERT_TRUE(log.getEntry(0)->level == LogLevel::ERROR);
}

TEST_SUITE(logger_level_get_set) {
    auto& log = Logger::instance();
    log.setLevel(LogLevel::TRACE);
    ASSERT_TRUE(log.getLevel() == LogLevel::TRACE);
    log.setLevel(LogLevel::ERROR);
    ASSERT_TRUE(log.getLevel() == LogLevel::ERROR);
    log.setLevel(LogLevel::DEBUG); // Reset for other tests
}

// ── Ring-buffer wrap-around ─────────────────────────────────────────────

TEST_SUITE(logger_ring_buffer_wraps) {
    auto& log = Logger::instance();
    log.clear();
    log.setLevel(LogLevel::TRACE);

    // Fill beyond capacity
    for (int i = 0; i < Logger::RING_BUFFER_SIZE + 10; i++) {
        log.log(LogLevel::INFO, "FILL", "Entry %d", i);
    }

    // Should cap at RING_BUFFER_SIZE
    ASSERT_EQ(log.getEntryCount(), Logger::RING_BUFFER_SIZE);

    // Most recent should be the last written
    FixedString<128> expected;
    expected.format("Entry %d", Logger::RING_BUFFER_SIZE + 10 - 1);
    ASSERT_STR_EQ(log.getEntry(0)->message.c_str(), expected.c_str());
}

// ── Diagnostics buffer ──────────────────────────────────────────────────

TEST_SUITE(logger_diagnostics_buffer) {
    auto& log = Logger::instance();
    log.clear();
    log.setLevel(LogLevel::TRACE);

    log.log(LogLevel::INFO,  "A", "Info message");
    log.log(LogLevel::WARN,  "B", "Warn message");
    log.log(LogLevel::ERROR, "C", "Error message");

    LogEntry buf[10];
    int n = log.getDiagnosticsBuffer(buf, 10);
    ASSERT_EQ(n, 3);

    // Oldest first in diagnostics buffer
    ASSERT_STR_EQ(buf[0].message.c_str(), "Info message");
    ASSERT_STR_EQ(buf[1].message.c_str(), "Warn message");
    ASSERT_STR_EQ(buf[2].message.c_str(), "Error message");
}

TEST_SUITE(logger_diagnostics_buffer_limited) {
    auto& log = Logger::instance();
    log.clear();
    log.setLevel(LogLevel::TRACE);

    for (int i = 0; i < 5; i++) {
        log.log(LogLevel::INFO, "T", "Msg %d", i);
    }

    LogEntry buf[3];
    int n = log.getDiagnosticsBuffer(buf, 3);
    ASSERT_EQ(n, 3);
    // Returns oldest 3
    ASSERT_STR_EQ(buf[0].message.c_str(), "Msg 0");
}

// ── Filter by level ─────────────────────────────────────────────────────

TEST_SUITE(logger_filter_by_level) {
    auto& log = Logger::instance();
    log.clear();
    log.setLevel(LogLevel::TRACE);

    log.log(LogLevel::DEBUG, "X", "Debug");
    log.log(LogLevel::INFO,  "X", "Info");
    log.log(LogLevel::WARN,  "X", "Warn");
    log.log(LogLevel::ERROR, "X", "Error");
    log.log(LogLevel::FATAL, "X", "Fatal");

    LogEntry buf[10];
    int n = log.getEntriesByLevel(LogLevel::WARN, buf, 10);
    ASSERT_EQ(n, 3); // WARN + ERROR + FATAL
    ASSERT_STR_EQ(buf[0].message.c_str(), "Warn");
    ASSERT_STR_EQ(buf[1].message.c_str(), "Error");
    ASSERT_STR_EQ(buf[2].message.c_str(), "Fatal");
}

// ── Level names ─────────────────────────────────────────────────────────

TEST_SUITE(logger_level_names) {
    ASSERT_STR_EQ(Logger::levelName(LogLevel::TRACE), "TRACE");
    ASSERT_STR_EQ(Logger::levelName(LogLevel::DEBUG), "DEBUG");
    ASSERT_STR_EQ(Logger::levelName(LogLevel::INFO),  "INFO");
    ASSERT_STR_EQ(Logger::levelName(LogLevel::WARN),  "WARN");
    ASSERT_STR_EQ(Logger::levelName(LogLevel::ERROR), "ERROR");
    ASSERT_STR_EQ(Logger::levelName(LogLevel::FATAL), "FATAL");
}

// ── Macros ──────────────────────────────────────────────────────────────

TEST_SUITE(logger_macros) {
    auto& log = Logger::instance();
    log.clear();
    log.setLevel(LogLevel::TRACE);

    PHOENIX_LOGD("Macro", "Debug %d", 1);
    PHOENIX_LOGI("Macro", "Info %d", 2);
    PHOENIX_LOGW("Macro", "Warn %d", 3);
    PHOENIX_LOGE("Macro", "Error %d", 4);

    ASSERT_EQ(log.getEntryCount(), 4);
    ASSERT_TRUE(log.getEntry(0)->level == LogLevel::ERROR);
    ASSERT_STR_EQ(log.getEntry(0)->message.c_str(), "Error 4");
}

TEST_SUITE(logger_timestamp_monotonic) {
    auto& log = Logger::instance();
    log.clear();
    log.setLevel(LogLevel::TRACE);

    log.log(LogLevel::INFO, "T", "First");
    log.log(LogLevel::INFO, "T", "Second");

    // Timestamps should be monotonically non-decreasing
    ASSERT_TRUE(log.getEntry(1)->timestamp_ms <= log.getEntry(0)->timestamp_ms);
}

// ── Clear ───────────────────────────────────────────────────────────────

TEST_SUITE(logger_clear) {
    auto& log = Logger::instance();
    log.setLevel(LogLevel::TRACE);
    log.log(LogLevel::INFO, "X", "Something");
    log.clear();
    ASSERT_EQ(log.getEntryCount(), 0);
}
