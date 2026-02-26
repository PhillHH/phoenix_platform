##############################################################################
# PHOENIX MEDICAL DEVICE FIRMWARE v108.0 "Chimera"
# Target: ESP32 #2 — Measurement MCU (Kamera, Analyse, Kalibrierung)
# Hardware: ESP32-WROOM-32D, OV2686 Camera, LED Ring
# IPC: UART to ESP32 #1 (UI/Connectivity MCU)
#
# IEC 62304 Class C · ISO 14971 Risk Management
# Build: ESP-IDF v5.3 / GCC 13.2
##############################################################################

cmake_minimum_required(VERSION 3.16)

# ─── Feature Toggles ───────────────────────────────────────────────────
option(PHOENIX_AI_ENABLED        "Enable on-device AI QC"            OFF)
option(PHOENIX_TRF_ENABLED       "Enable time-resolved fluorescence" OFF)
option(PHOENIX_BENCHMARK_ENABLED "Enable benchmark validation"       ON)
option(PHOENIX_COLOR_CAL_ENABLED "Enable colorimetric assay support" ON)

# ─── Compiler Setup ────────────────────────────────────────────────────
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# IEC 62304: All warnings are errors in production
set(EXTRA_CXXFLAGS "-Wall -Wextra -Werror -Wpedantic -Wconversion -Wshadow")
set(EXTRA_CXXFLAGS "${EXTRA_CXXFLAGS} -fno-exceptions -fno-rtti")  # No exceptions on ESP32

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(phoenix_measurement_mcu)
