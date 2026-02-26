// ==========================================================================
// Phoenix Test Framework — Lightweight, no-heap, IEC 62304 verification
//
// Runs on:
//   - Host (g++): ANSI color output, exit code 0/1
//   - ESP32 (PlatformIO): ESP_LOGI output
//
// Usage:
//   TEST_SUITE(test_my_feature) {
//       ASSERT_TRUE(1 + 1 == 2);
//       ASSERT_EQ(42, 42);
//       ASSERT_FLOAT_EQ(3.14f, 3.14f, 0.001f);
//       auto r = phoenix::Ok(42);
//       ASSERT_RESULT_OK(r);
//   }
// ==========================================================================
#pragma once

#include <cstdio>
#include <cmath>
#include <cstring>
#include <cstdint>

namespace phoenix_test {

// ── ANSI colors (host only) ─────────────────────────────────────────────

#ifdef PHOENIX_HOST_TEST
  #define PT_GREEN   "\033[32m"
  #define PT_RED     "\033[31m"
  #define PT_YELLOW  "\033[33m"
  #define PT_BOLD    "\033[1m"
  #define PT_RESET   "\033[0m"
  #define PT_LOG(fmt, ...) printf(fmt "\n", ##__VA_ARGS__)
#else
  #include <esp_log.h>
  #define PT_GREEN   ""
  #define PT_RED     ""
  #define PT_YELLOW  ""
  #define PT_BOLD    ""
  #define PT_RESET   ""
  #define PT_LOG(fmt, ...) ESP_LOGI("TEST", fmt, ##__VA_ARGS__)
#endif

// ── Test statistics ──────────────────────────────────────────────────────

struct TestStats {
    int passed  = 0;
    int failed  = 0;
    int skipped = 0;
    int suites  = 0;
    const char* current_suite = "";
};

inline TestStats& stats() {
    static TestStats s;
    return s;
}

// ── Suite registration ───────────────────────────────────────────────────

static constexpr int MAX_SUITES = 128;

using SuiteFn = void(*)();

struct SuiteEntry {
    const char* name;
    SuiteFn     fn;
};

struct SuiteRegistry {
    SuiteEntry entries[MAX_SUITES] = {};
    int count = 0;

    static SuiteRegistry& instance() {
        static SuiteRegistry reg;
        return reg;
    }
};

inline int register_suite(const char* name, SuiteFn fn) {
    auto& reg = SuiteRegistry::instance();
    int idx = reg.count++;
    reg.entries[idx] = {name, fn};
    return idx;
}

// ── Core assertion functions ──────────────────────────────────────────────

inline void assert_true(bool cond, const char* expr, const char* file, int line) {
    if (cond) {
        stats().passed++;
    } else {
        stats().failed++;
        PT_LOG("  %sFAIL%s: %s  (%s:%d)", PT_RED, PT_RESET, expr, file, line);
    }
}

inline void assert_eq_int(int64_t a, int64_t b, const char* expr_a,
                           const char* expr_b, const char* file, int line) {
    if (a == b) {
        stats().passed++;
    } else {
        stats().failed++;
        PT_LOG("  %sFAIL%s: %s == %s  (got %lld vs %lld)  (%s:%d)",
               PT_RED, PT_RESET, expr_a, expr_b,
               (long long)a, (long long)b, file, line);
    }
}

inline void assert_float_eq(float actual, float expected, float tolerance,
                             const char* expr, const char* file, int line) {
    float diff = fabsf(actual - expected);
    if (diff <= tolerance) {
        stats().passed++;
    } else {
        stats().failed++;
        PT_LOG("  %sFAIL%s: %s  (actual=%.6f, expected=%.6f, diff=%.6f, tol=%.6f)  (%s:%d)",
               PT_RED, PT_RESET, expr,
               (double)actual, (double)expected, (double)diff, (double)tolerance,
               file, line);
    }
}

inline void assert_str_eq(const char* actual, const char* expected,
                           const char* expr_a, const char* expr_b,
                           const char* file, int line) {
    if (actual && expected && strcmp(actual, expected) == 0) {
        stats().passed++;
    } else {
        stats().failed++;
        PT_LOG("  %sFAIL%s: %s == %s  (got \"%s\" vs \"%s\")  (%s:%d)",
               PT_RED, PT_RESET, expr_a, expr_b,
               actual ? actual : "(null)", expected ? expected : "(null)",
               file, line);
    }
}

inline void skip_test(const char* reason, const char* file, int line) {
    stats().skipped++;
    PT_LOG("  %sSKIP%s: %s  (%s:%d)", PT_YELLOW, PT_RESET, reason, file, line);
}

// ── Suite lifecycle ──────────────────────────────────────────────────────

inline void begin_suite(const char* name) {
    stats().current_suite = name;
    stats().suites++;
    PT_LOG("");
    PT_LOG("%s═══ %s ═══%s", PT_YELLOW, name, PT_RESET);
}

// ── Runner ───────────────────────────────────────────────────────────────

inline int run_all() {
    PT_LOG("%s══════════════════════════════════════════════════════════%s", PT_BOLD, PT_RESET);
    PT_LOG("%s Phoenix Test Runner — IEC 62304 Verification%s", PT_BOLD, PT_RESET);
    PT_LOG("%s══════════════════════════════════════════════════════════%s", PT_BOLD, PT_RESET);

    auto& reg = SuiteRegistry::instance();
    for (int i = 0; i < reg.count; i++) {
        begin_suite(reg.entries[i].name);
        reg.entries[i].fn();
    }

    auto& s = stats();
    int total = s.passed + s.failed + s.skipped;

    PT_LOG("");
    PT_LOG("%s══════════════════════════════════════════════════════════%s", PT_BOLD, PT_RESET);
    PT_LOG(" RESULTS: %d suites, %d/%d passed, %d failed, %d skipped",
           s.suites, s.passed, total, s.failed, s.skipped);
    PT_LOG("%s══════════════════════════════════════════════════════════%s", PT_BOLD, PT_RESET);

    if (s.failed == 0) {
        PT_LOG(" %sALL %d TESTS PASSED%s", PT_GREEN, s.passed, PT_RESET);
        return 0;
    } else {
        PT_LOG(" %s%d TESTS FAILED%s", PT_RED, s.failed, PT_RESET);
        return 1;
    }
}

} // namespace phoenix_test

// ── Macros ───────────────────────────────────────────────────────────────

#define ASSERT_TRUE(expr) \
    phoenix_test::assert_true((expr), #expr, __FILE__, __LINE__)

#define ASSERT_FALSE(expr) \
    phoenix_test::assert_true(!(expr), "!" #expr, __FILE__, __LINE__)

#define ASSERT_EQ(a, b) \
    phoenix_test::assert_eq_int(static_cast<int64_t>(a), static_cast<int64_t>(b), \
                                #a, #b, __FILE__, __LINE__)

#define ASSERT_FLOAT_EQ(actual, expected, tolerance) \
    phoenix_test::assert_float_eq((actual), (expected), (tolerance), \
                                  #actual " ~= " #expected, __FILE__, __LINE__)

#define ASSERT_STR_EQ(actual, expected) \
    phoenix_test::assert_str_eq((actual), (expected), #actual, #expected, \
                                __FILE__, __LINE__)

#define ASSERT_RESULT_OK(result) \
    phoenix_test::assert_true((result).is_ok(), \
                              #result ".is_ok()", __FILE__, __LINE__)

#define ASSERT_RESULT_ERR(result) \
    phoenix_test::assert_true((result).is_err(), \
                              #result ".is_err()", __FILE__, __LINE__)

#define SKIP_TEST(reason) \
    phoenix_test::skip_test(reason, __FILE__, __LINE__)

#define TEST_SUITE(name) \
    static void test_suite_fn_##name(); \
    static int test_suite_reg_##name = \
        phoenix_test::register_suite(#name, test_suite_fn_##name); \
    static void test_suite_fn_##name()
