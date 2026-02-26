// ==========================================================================
// FILE: include/phoenix/Core/FixedString.h
// Heap-free fixed-size strings for ESP32 embedded use
// ==========================================================================
#pragma once

#include <cstdint>
#include <cstring>
#include <cstdio>

namespace phoenix {

template <size_t N>
class FixedString {
public:
    constexpr FixedString() { buf_[0] = '\0'; }

    FixedString(const char* s) {
        if (s) {
            strncpy(buf_, s, N - 1);
            buf_[N - 1] = '\0';
        } else {
            buf_[0] = '\0';
        }
    }

    [[nodiscard]] const char* c_str()    const { return buf_; }
    [[nodiscard]] size_t      length()   const { return strlen(buf_); }
    [[nodiscard]] bool        empty()    const { return buf_[0] == '\0'; }
    [[nodiscard]] size_t      capacity() const { return N - 1; }

    void clear() { buf_[0] = '\0'; }

    bool append(const char* s) {
        size_t cur = length();
        size_t add = s ? strlen(s) : 0;
        if (cur + add >= N) return false;
        strncat(buf_, s, N - cur - 1);
        return true;
    }

    // printf-style formatting
    template <typename... Args>
    bool format(const char* fmt, Args... args) {
        int n = snprintf(buf_, N, fmt, args...);
        return n >= 0 && static_cast<size_t>(n) < N;
    }

    bool operator==(const char* s) const { return strcmp(buf_, s) == 0; }
    bool operator!=(const char* s) const { return strcmp(buf_, s) != 0; }

    char& operator[](size_t i) { return buf_[i]; }
    const char& operator[](size_t i) const { return buf_[i]; }

private:
    char buf_[N] = {};
};

// Common sizes used throughout Phoenix
using String16  = FixedString<16>;
using String32  = FixedString<32>;
using String64  = FixedString<64>;
using String128 = FixedString<128>;
using String256 = FixedString<256>;

} // namespace phoenix
