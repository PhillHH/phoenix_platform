// ==========================================================================
// Phoenix Test Runner — Host Entry Point
// Runs all registered test suites and returns exit code 0/1
//
// Build: cmake -B build && cmake --build build && ./build/test_runner
// ==========================================================================
#include "test_framework.h"

// Each .cpp file auto-registers its TEST_SUITE via static initializers.
// We just need to link them and call run_all().

int main() {
    return phoenix_test::run_all();
}
