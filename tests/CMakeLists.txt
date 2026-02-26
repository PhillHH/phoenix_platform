# ==========================================================================
# Phoenix Unit Tests — Host Build (g++)
#
# Usage:
#   cd tests && cmake -B build && cmake --build build && ./build/test_runner
# ==========================================================================
cmake_minimum_required(VERSION 3.16)
project(phoenix_tests LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── Include paths ─────────────────────────────────────────────────────────
# Host stubs MUST come before production headers to shadow ESP-IDF includes
include_directories(BEFORE SYSTEM ${CMAKE_CURRENT_SOURCE_DIR}/host_stubs)
include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../phoenix-measurement/main/include)

# ── Defines ───────────────────────────────────────────────────────────────
add_definitions(-DPHOENIX_HOST_TEST)

# ── Production sources needed for tests ───────────────────────────────────
set(PHOENIX_SOURCES
    ../phoenix-measurement/main/src/Analysis/Dx365Algorithm.cpp
    ../phoenix-measurement/main/src/Services/CalibrationService.cpp
)

# ── Test sources ──────────────────────────────────────────────────────────
set(TEST_SOURCES
    test_result.cpp
    test_fixedstring.cpp
    test_5pl_math.cpp
    test_peak_detection.cpp
    test_calibration.cpp
    test_uart_bridge.cpp
    test_nvs_storage.cpp
    run_all.cpp
)

# ── Build ─────────────────────────────────────────────────────────────────
add_executable(test_runner ${TEST_SOURCES} ${PHOENIX_SOURCES})

target_compile_options(test_runner PRIVATE
    -Wall -Wextra -Wpedantic
    -Wno-unused-variable
    -Wno-unused-function
    -Wno-format-security
)
