# Phoenix Platform v108.0 "Chimera"

![Version](https://img.shields.io/badge/version-108.0-blue)
![Build](https://img.shields.io/badge/build-ESP--IDF%20v5.x-green)
![License](https://img.shields.io/badge/license-proprietary-red)
![IEC 62304](https://img.shields.io/badge/IEC%2062304-Class%20C-orange)

**Point-of-Care Diagnostik-Firmware for the Igloo Pro Analyzer**

Dual-MCU embedded firmware for a lateral-flow immunoassay reader with optical quantitation, 5-parameter logistic calibration, and IEC 62304 Class C compliance.

```
 ┌─────────────────┐    UART    ┌──────────────────┐
 │   ESP32 #1      │◄──────────►│   ESP32 #2       │
 │   phoenix-ui    │  CRC-16    │ phoenix-measure   │
 │                 │  921600 bd │                   │
 │ • ST7701S LCD   │            │ • OV2686 Camera   │
 │ • Touch (GT911) │            │ • RGB LEDs        │
 │ • WiFi / LIMS   │            │ • 5PL Analysis    │
 │ • LVGL UI       │            │ • Calibration     │
 └─────────────────┘            └──────────────────┘
```

## Quick Start

### Prerequisites

- [ESP-IDF v5.x](https://docs.espressif.com/projects/esp-idf/en/stable/) (both MCUs)
- [PlatformIO](https://platformio.org/) (for Wokwi simulation)
- GNU g++ 13+ (for host unit tests)
- CMake 3.16+

### Clone

```bash
git clone <repo-url> phoenix_platform
cd phoenix_platform
```

### Build — ESP-IDF (Production)

```bash
# Measurement MCU (ESP32 #2)
cd phoenix-measurement
idf.py set-target esp32s3
idf.py build
idf.py flash -p /dev/ttyUSB0

# UI MCU (ESP32 #1)
cd ../phoenix-ui
idf.py set-target esp32s3
idf.py build
idf.py flash -p /dev/ttyUSB1
```

### Build — Host Unit Tests

```bash
cd tests
cmake -B build && cmake --build build
./build/test_runner
# 115 suites, 813 assertions — all passing
```

### Wokwi Simulation

```bash
cd wokwi-test
pio run                     # PlatformIO build (Arduino framework)
# Upload diagram.json + wokwi.toml to https://wokwi.com
# or: pio run -t upload    # With Wokwi CLI
```

## Directory Structure

```
phoenix_platform/
├── phoenix-measurement/          # ESP32 #2 — Measurement MCU firmware
│   └── main/
│       ├── include/phoenix/
│       │   ├── Analysis/         # Dx365 algorithm, peak detection, pipeline
│       │   ├── Calibration/      # 30 verified assays, factory curves, color chart
│       │   ├── Core/             # Result<T>, FixedString, Logger, ErrorRegistry
│       │   ├── HAL/              # Camera (OV2686), LED controller interfaces
│       │   ├── IPC/              # UART bridge protocol (CRC-16 frames)
│       │   └── Services/         # CalibrationService (LM fitting), BenchmarkValidator
│       └── src/                  # Implementation files
│
├── phoenix-ui/                   # ESP32 #1 — UI MCU firmware
│   └── main/
│       ├── include/phoenix/
│       │   ├── Core/             # Shared types (Result, FixedString, Logger)
│       │   ├── IPC/              # MeasurementProxy, UartBridge
│       │   └── UI/               # PhoenixUI (LVGL), CircularMenu
│       └── src/
│           ├── Connectivity/     # WiFi, LIMS integration
│           ├── UI/Screens/       # Home, Measurement, Result, Calibration, etc.
│           └── ...
│
├── wokwi-test/                   # Wokwi simulation (PlatformIO + ESP-IDF)
│   ├── shared/                   # phoenix_test_core.h (single source of truth)
│   ├── src/main.cpp              # Arduino entry (5 lines, includes shared header)
│   └── main/main.cpp             # ESP-IDF entry (5 lines, includes shared header)
│
├── tests/                        # Host unit tests (g++17, no ESP32 needed)
│   ├── test_framework.h          # Lightweight test framework (IEC 62304)
│   ├── test_5pl_math.cpp         # 5PL forward/inverse, 30 assays
│   ├── test_peak_detection.cpp   # 7-point peak descriptor, T/C ratio
│   ├── test_calibration.cpp      # LM fitting, factory curves
│   ├── test_result.cpp           # Result<T> Ok/Err paths
│   ├── test_fixedstring.cpp      # FixedString<N> operations
│   ├── test_uart_bridge.cpp      # CRC16, frame encode/decode
│   ├── test_nvs_storage.cpp      # NVS stub roundtrip
│   ├── test_logger.cpp           # Ring-buffer, level filtering
│   ├── test_error_registry.cpp   # Error dedup, severity escalation
│   └── host_stubs/               # ESP-IDF header stubs for host build
│
└── docs/                         # Documentation
    ├── ARCHITECTURE.md           # System design, IPC protocol, pipeline
    ├── CALIBRATION.md            # 5PL model, LM fitting, assay table
    ├── API_REFERENCE.md          # Module APIs, error codes, workflow
    └── REGULATORY.md             # IEC 62304 compliance, SOUP, traceability
```

## Key Features

- **30 verified assays** from real Dx365 MCP data (CRP, HbA1c, Troponin I, IgE, Vitamin D, ...)
- **5-Parameter Logistic (5PL)** calibration with embedded Levenberg-Marquardt fitting
- **7-point peak descriptor** for LFA strip analysis
- **Heap-free design** — all data structures use fixed-size buffers in .bss
- **CRC-16/CCITT** protected UART inter-processor communication
- **Structured audit logging** with 100-entry ring-buffer (IEC 62304 REQ-LOG-001)
- **Persistent error registry** with NVS storage and deduplication
- **115 unit test suites, 813 assertions** running on host (g++17)

## Documentation

| Document | Description |
|---|---|
| [Architecture](docs/ARCHITECTURE.md) | System design, IPC protocol, memory budget |
| [Calibration](docs/CALIBRATION.md) | 5PL model, fitting algorithm, assay registry |
| [API Reference](docs/API_REFERENCE.md) | Module APIs, error codes, code examples |
| [Regulatory](docs/REGULATORY.md) | IEC 62304 compliance, SOUP list, traceability |

### Operations Runbooks

| Runbook | Audience | Description |
|---|---|---|
| [Manufacturing](docs/runbooks/MANUFACTURING.md) | Production technicians | Flash, POST, calibration, QC procedure |
| [Troubleshooting](docs/runbooks/TROUBLESHOOTING.md) | All technicians | Symptom → Diagnosis → Fix tables |
| [Field Service](docs/runbooks/FIELD_SERVICE.md) | Field service | OTA/USB update, recalibration, diagnostics |
| [Development](docs/runbooks/DEVELOPMENT.md) | Developers | Environment setup, build, debug, CI/CD |

## Technology Stack

| Component | Technology |
|---|---|
| MCU | ESP32-S3 (2x) |
| Framework | ESP-IDF v5.x |
| Language | C++17 (gnu++17) |
| UI | LVGL 8.x |
| Camera | OV2686 (DVP) |
| Display | ST7701S (480x480 RGB) |
| Touch | GT911 (I2C) |
| RTOS | FreeRTOS |
| Simulation | Wokwi + PlatformIO |
