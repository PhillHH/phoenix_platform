// ==========================================================================
// Test: FixedString<N> — Heap-free string for embedded use
// Verifies: No heap allocation, truncation safety, format correctness
// ==========================================================================
#include "test_framework.h"
#include "phoenix/Core/FixedString.h"

using namespace phoenix;

TEST_SUITE(fixedstring_default_construct) {
    FixedString<32> s;
    ASSERT_TRUE(s.empty());
    ASSERT_EQ(s.length(), 0u);
    ASSERT_STR_EQ(s.c_str(), "");
    ASSERT_EQ(s.capacity(), 31u);
}

TEST_SUITE(fixedstring_construct_from_cstr) {
    FixedString<32> s("hello");
    ASSERT_FALSE(s.empty());
    ASSERT_EQ(s.length(), 5u);
    ASSERT_STR_EQ(s.c_str(), "hello");
}

TEST_SUITE(fixedstring_construct_from_nullptr) {
    FixedString<16> s(nullptr);
    ASSERT_TRUE(s.empty());
    ASSERT_EQ(s.length(), 0u);
}

TEST_SUITE(fixedstring_overflow_truncation) {
    // FixedString<8> has capacity 7; "abcdefghij" (10 chars) must truncate
    FixedString<8> s("abcdefghij");
    ASSERT_EQ(s.length(), 7u);
    ASSERT_STR_EQ(s.c_str(), "abcdefg");
    // Must be null-terminated
    ASSERT_TRUE(s.c_str()[7] == '\0');
}

TEST_SUITE(fixedstring_comparison) {
    FixedString<32> s("IgE-LFT");
    ASSERT_TRUE(s == "IgE-LFT");
    ASSERT_TRUE(s != "CRP");
    ASSERT_FALSE(s == "ige-lft");  // Case sensitive
    ASSERT_FALSE(s != "IgE-LFT");
}

TEST_SUITE(fixedstring_format) {
    FixedString<32> s;
    bool ok = s.format("val=%d, f=%.1f", 42, 3.14);
    ASSERT_TRUE(ok);
    ASSERT_STR_EQ(s.c_str(), "val=42, f=3.1");
}

TEST_SUITE(fixedstring_format_overflow) {
    FixedString<8> s;
    bool ok = s.format("long string that exceeds buffer %d", 12345);
    ASSERT_FALSE(ok);
    // Buffer must still be null-terminated
    ASSERT_TRUE(strlen(s.c_str()) < 8);
}

TEST_SUITE(fixedstring_clear) {
    FixedString<32> s("hello");
    ASSERT_FALSE(s.empty());
    s.clear();
    ASSERT_TRUE(s.empty());
    ASSERT_EQ(s.length(), 0u);
}

TEST_SUITE(fixedstring_append) {
    FixedString<16> s("hello");
    bool ok = s.append(" world");
    ASSERT_TRUE(ok);
    ASSERT_STR_EQ(s.c_str(), "hello world");

    // Append that would overflow
    ok = s.append("this is way too long");
    ASSERT_FALSE(ok);
    // Original must be unchanged
    ASSERT_STR_EQ(s.c_str(), "hello world");
}

TEST_SUITE(fixedstring_subscript) {
    FixedString<16> s("abc");
    ASSERT_EQ(s[0], 'a');
    ASSERT_EQ(s[1], 'b');
    ASSERT_EQ(s[2], 'c');

    // Mutable access
    s[0] = 'X';
    ASSERT_STR_EQ(s.c_str(), "Xbc");
}

TEST_SUITE(fixedstring_copy) {
    FixedString<32> a("original");
    FixedString<32> b = a;
    ASSERT_STR_EQ(b.c_str(), "original");

    // Modifying copy must not affect original
    b.format("modified");
    ASSERT_STR_EQ(a.c_str(), "original");
    ASSERT_STR_EQ(b.c_str(), "modified");
}

TEST_SUITE(fixedstring_assignment) {
    FixedString<32> a("first");
    FixedString<32> b("second");
    a = b;
    ASSERT_STR_EQ(a.c_str(), "second");
}

TEST_SUITE(fixedstring_type_aliases) {
    // Verify type aliases have correct capacities
    String16 s16;
    ASSERT_EQ(s16.capacity(), 15u);

    String32 s32;
    ASSERT_EQ(s32.capacity(), 31u);

    String64 s64;
    ASSERT_EQ(s64.capacity(), 63u);
}
