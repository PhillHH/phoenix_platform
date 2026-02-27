# Changelog

All notable changes to the Phoenix Platform are documented in this file.

Format based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

## [v108.0.1] — 2026-02-27

### Added

- **Unit test framework** — 115 test suites, 813 assertions covering 5PL math,
  peak detection, LM calibration fitting, Result<T>, FixedString<N>, UART/CRC-16,
  NVS storage, Logger, and ErrorRegistry (`tests/`)
- **Structured Logger** — Heap-free ring-buffer (100 entries) with severity levels,
  `PHOENIX_LOGx` macros, dual-platform (ESP32 + host) support
  (`phoenix-measurement/main/include/phoenix/Core/Logger.h`)
- **ErrorRegistry** — Centralized error tracking with deduplication, severity
  escalation, NVS persistence, and 38 defined error codes
  (`phoenix-measurement/main/include/phoenix/Core/ErrorRegistry.h`)
- **Project documentation** (IEC 62304 compliant):
  - `docs/ARCHITECTURE.md` — System design, IPC protocol, memory budget
  - `docs/CALIBRATION.md` — 5PL model, LM fitting, all 30 assay configurations
  - `docs/API_REFERENCE.md` — Module APIs, 38 error codes, code examples
  - `docs/REGULATORY.md` — IEC 62304 compliance, SOUP list, traceability matrix
- **Operations runbooks**:
  - `docs/runbooks/MANUFACTURING.md` — 7-step flash/POST/calibrate/QC procedure
  - `docs/runbooks/TROUBLESHOOTING.md` — Symptom → Diagnosis → Fix tables
  - `docs/runbooks/FIELD_SERVICE.md` — OTA/USB update, recalibration, diagnostics
  - `docs/runbooks/DEVELOPMENT.md` — Environment setup, build, debug, CI/CD
- **Wokwi simulation configuration** — `diagram.json` with 4 LEDs + 220Ω resistors,
  UART TX→RX loopback, push button; `SIMULATION.md` documentation

### Changed

- **Wokwi code deduplication** — Extracted shared logic into
  `wokwi-test/shared/phoenix_test_core.h` (single source of truth); both
  `src/main.cpp` and `main/main.cpp` are now 5-line entry points
- **README.md** — Added documentation table, runbooks section, technology stack,
  directory structure, and build instructions

### Fixed

- **Compiler warnings** — Resolved all warnings across 17 sites with `-Wall -Wextra
  -Wpedantic -Wconversion -Wshadow -Werror`:
  - Unused parameters in `computePeakValue()`, `analyzeProfile()`,
    `handleStartMeasurement()`, `Logger::platformOutput()`
  - `memset` on non-trivial type in `CalibrationService::startCalibrationWorkflow()`
    replaced with value initialization
  - Integer-to-float conversion warnings in `Dx365Algorithm.cpp` and
    `CurveCorrection.cpp` via explicit `static_cast`
  - Format string portability (`%lu` → `%u` with `static_cast<unsigned>()`) in
    `AuditTrail.cpp`, `PatientManager.cpp`
  - Unused variables `mean_exp` and `x0` in `ColorChartCalibration.cpp`
  - Narrowing conversion in `ResultStorage.cpp` ternary expression
- **String64→String32 assignment** — Added `.c_str()` for cross-size FixedString
  assignment in `Dx365Algorithm.cpp`
- **Static initialization order** — SuiteRegistry uses Meyers singleton to prevent
  SEGFAULT in test framework
- **5PL inverse edge cases** — Degenerate curve and at-asymptote tests corrected
- **LM factory curve fitting** — Skip competitive assays with negative B parameter

## [v108.0] — 2026-02-26

### Added

- Initial commit of Phoenix Platform v108.0 "Chimera"
- Dual-MCU firmware: ESP32 (measurement) + ESP32-S3 (UI)
- 30 verified assay configurations from Dx365 MCP data
- 5PL calibration with Levenberg-Marquardt fitting
- 7-point peak descriptor for LFA strip analysis
- CRC-16/CCITT UART inter-processor communication
- LVGL touch UI with circular menu
- Color Chart calibration (DXR.007.01)
- Wokwi simulation project (8 test suites)
