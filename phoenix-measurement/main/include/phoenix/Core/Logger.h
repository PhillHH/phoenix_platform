// ==========================================================================
// FILE: include/phoenix/Core/Logger.h
// Phoenix v108.0 — Structured Logging for IEC 62304 Class C Audit Trail
// REQ-LOG-001: System SHALL maintain audit trail of all significant events
//
// Heap-free design: static ring-buffer in .bss, FixedString formatting
// Dual-platform: ESP_LOGx on ESP32, ANSI printf on host
// ==========================================================================
#pragma once

#include "phoenix/Core/FixedString.h"
#include <cstdint>
#include <cstring>

#ifndef PHOENIX_HOST_TEST
  #include <esp_log.h>
  #include <esp_timer.h>
#else
  #include <cstdio>
  #include <chrono>
#endif

namespace phoenix {

// ── Log Levels (ordered by severity) ────────────────────────────────────

enum class LogLevel : uint8_t {
    TRACE  = 0,
    DEBUG  = 1,
    INFO   = 2,
    WARN   = 3,
    ERROR  = 4,
    FATAL  = 5,
};

// ── Log Entry (fixed-size, no heap) ─────────────────────────────────────

struct LogEntry {
    uint32_t        timestamp_ms = 0;      // Milliseconds since boot
    LogLevel        level        = LogLevel::INFO;
    FixedString<16> tag;                   // Module identifier
    FixedString<128> message;              // Formatted message
};

// ── Logger Singleton ────────────────────────────────────────────────────
//
// Ring-buffer for the last 100 log entries.
// Entirely in .bss (no heap). Meyers singleton for safe initialization.

class Logger {
public:
    static constexpr int RING_BUFFER_SIZE = 100;

    static Logger& instance() {
        static Logger logger;
        return logger;
    }

    // Runtime log-level filter
    void setLevel(LogLevel min_level) { min_level_ = min_level; }
    LogLevel getLevel() const { return min_level_; }

    // Core log function — called by macros
    template <typename... Args>
    void log(LogLevel level, const char* tag, const char* fmt, Args... args) {
        if (level < min_level_) return;

        LogEntry& entry = entries_[write_idx_];
        entry.timestamp_ms = currentTimeMs();
        entry.level = level;
        entry.tag = tag;
        entry.message.format(fmt, args...);

        write_idx_ = (write_idx_ + 1) % RING_BUFFER_SIZE;
        if (count_ < RING_BUFFER_SIZE) count_++;

        // Forward to platform output
        platformOutput(level, tag, entry.message.c_str());
    }

    // Zero-arg overload (avoids format-security warning)
    void log(LogLevel level, const char* tag, const char* msg) {
        if (level < min_level_) return;

        LogEntry& entry = entries_[write_idx_];
        entry.timestamp_ms = currentTimeMs();
        entry.level = level;
        entry.tag = tag;
        entry.message = msg;

        write_idx_ = (write_idx_ + 1) % RING_BUFFER_SIZE;
        if (count_ < RING_BUFFER_SIZE) count_++;

        platformOutput(level, tag, msg);
    }

    // ── Diagnostics buffer access ───────────────────────────────────

    int getEntryCount() const { return count_; }

    // Get entry by reverse index (0 = most recent)
    const LogEntry* getEntry(int reverse_idx) const {
        if (reverse_idx < 0 || reverse_idx >= count_) return nullptr;
        int idx = (write_idx_ - 1 - reverse_idx + RING_BUFFER_SIZE) % RING_BUFFER_SIZE;
        return &entries_[idx];
    }

    // Copy up to max_entries into output buffer, returns count copied
    int getDiagnosticsBuffer(LogEntry* out, int max_entries) const {
        int to_copy = (max_entries < count_) ? max_entries : count_;
        for (int i = 0; i < to_copy; i++) {
            int idx = (write_idx_ - count_ + i + RING_BUFFER_SIZE) % RING_BUFFER_SIZE;
            out[i] = entries_[idx];
        }
        return to_copy;
    }

    // Filter by level
    int getEntriesByLevel(LogLevel min, LogEntry* out, int max_entries) const {
        int copied = 0;
        for (int i = 0; i < count_ && copied < max_entries; i++) {
            int idx = (write_idx_ - count_ + i + RING_BUFFER_SIZE) % RING_BUFFER_SIZE;
            if (entries_[idx].level >= min) {
                out[copied++] = entries_[idx];
            }
        }
        return copied;
    }

    void clear() {
        write_idx_ = 0;
        count_ = 0;
    }

    // ── Level name utilities ────────────────────────────────────────

    static const char* levelName(LogLevel level) {
        switch (level) {
            case LogLevel::TRACE: return "TRACE";
            case LogLevel::DEBUG: return "DEBUG";
            case LogLevel::INFO:  return "INFO";
            case LogLevel::WARN:  return "WARN";
            case LogLevel::ERROR: return "ERROR";
            case LogLevel::FATAL: return "FATAL";
            default:              return "???";
        }
    }

private:
    Logger() = default;
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    LogEntry entries_[RING_BUFFER_SIZE] = {};
    int      write_idx_ = 0;
    int      count_     = 0;
    LogLevel min_level_ = LogLevel::DEBUG;

    static uint32_t currentTimeMs() {
#ifndef PHOENIX_HOST_TEST
        return static_cast<uint32_t>(esp_timer_get_time() / 1000ULL);
#else
        static auto start = std::chrono::steady_clock::now();
        auto now = std::chrono::steady_clock::now();
        return static_cast<uint32_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count());
#endif
    }

    static void platformOutput(LogLevel level, const char* tag, const char* msg) {
#ifndef PHOENIX_HOST_TEST
        switch (level) {
            case LogLevel::TRACE: ESP_LOGD(tag, "%s", msg); break;
            case LogLevel::DEBUG: ESP_LOGD(tag, "%s", msg); break;
            case LogLevel::INFO:  ESP_LOGI(tag, "%s", msg); break;
            case LogLevel::WARN:  ESP_LOGW(tag, "%s", msg); break;
            case LogLevel::ERROR: ESP_LOGE(tag, "%s", msg); break;
            case LogLevel::FATAL: ESP_LOGE(tag, "[FATAL] %s", msg); break;
        }
#else
        const char* color = "";
        const char* reset = "\033[0m";
        switch (level) {
            case LogLevel::TRACE: color = "\033[90m";   break; // Gray
            case LogLevel::DEBUG: color = "\033[36m";   break; // Cyan
            case LogLevel::INFO:  color = "\033[32m";   break; // Green
            case LogLevel::WARN:  color = "\033[33m";   break; // Yellow
            case LogLevel::ERROR: color = "\033[31m";   break; // Red
            case LogLevel::FATAL: color = "\033[31;1m"; break; // Bold red
        }
        printf("%s[%s] %s: %s%s\n", color, levelName(level), tag, msg, reset);
#endif
    }
};

} // namespace phoenix

// ── Convenience Macros ──────────────────────────────────────────────────
//
// Usage:
//   PHOENIX_LOG(LogLevel::INFO, "MyModule", "Value=%d", 42);
//   PHOENIX_LOGI("MyModule", "Initialized");
//   PHOENIX_LOGW("MyModule", "Temperature high: %.1f", temp);

#define PHOENIX_LOG(level, tag, fmt, ...) \
    ::phoenix::Logger::instance().log(level, tag, fmt, ##__VA_ARGS__)

#define PHOENIX_LOGT(tag, fmt, ...) \
    PHOENIX_LOG(::phoenix::LogLevel::TRACE, tag, fmt, ##__VA_ARGS__)

#define PHOENIX_LOGD(tag, fmt, ...) \
    PHOENIX_LOG(::phoenix::LogLevel::DEBUG, tag, fmt, ##__VA_ARGS__)

#define PHOENIX_LOGI(tag, fmt, ...) \
    PHOENIX_LOG(::phoenix::LogLevel::INFO, tag, fmt, ##__VA_ARGS__)

#define PHOENIX_LOGW(tag, fmt, ...) \
    PHOENIX_LOG(::phoenix::LogLevel::WARN, tag, fmt, ##__VA_ARGS__)

#define PHOENIX_LOGE(tag, fmt, ...) \
    PHOENIX_LOG(::phoenix::LogLevel::ERROR, tag, fmt, ##__VA_ARGS__)

#define PHOENIX_LOGF(tag, fmt, ...) \
    PHOENIX_LOG(::phoenix::LogLevel::FATAL, tag, fmt, ##__VA_ARGS__)
