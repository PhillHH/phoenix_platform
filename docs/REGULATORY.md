# Regulatory Compliance — Phoenix Platform v108.0

## Software Safety Classification

### IEC 62304 Class C Justification

The Phoenix Platform firmware is classified as **IEC 62304 Class C** (highest safety class) based on ISO 14971 risk analysis:

| Factor | Assessment | Rationale |
|---|---|---|
| Software contribution to hazard | **Can contribute** | Incorrect measurement results could lead to misdiagnosis |
| Severity of harm | **Death or serious injury** | Cardiac markers (Troponin I), sepsis markers (PCT, IL-6) directly affect treatment decisions |
| Probability of harm | **Moderate** | Point-of-care setting without immediate laboratory confirmation |
| Risk control measures | **Software is the primary control** | Calibration accuracy, QC validation, and result display depend entirely on firmware |

**Conclusion:** Because the software can contribute to a hazardous situation resulting in death or serious injury, and external risk control measures do not provide sufficient risk reduction, the software is classified as **Class C** per IEC 62304:2006+A1:2015 §4.3.

### Implications of Class C

- Full software development lifecycle documentation required
- Unit-level verification mandatory for all safety-critical modules
- Detailed architecture documentation required
- All SOUP items must be assessed for risk
- Regression testing required for all changes
- Traceability from requirements to implementation to test

---

## IEC 62304 Compliance Matrix

### Software Development Process Requirements

| IEC 62304 Clause | Requirement | Implementation | Evidence |
|---|---|---|---|
| §5.1 | Software development planning | CMake-based build system, host test runner, version-controlled | `CMakeLists.txt`, git history |
| §5.2 | Software requirements analysis | Requirements embedded as `REQ-xxx-nnn` in source headers | All header files contain REQ tags |
| §5.3 | Software architectural design | Dual-MCU architecture, modular pipeline design | `docs/ARCHITECTURE.md`, header interfaces |
| §5.4 | Software detailed design | Class-level documentation, interface contracts | Header files with Doxygen-style comments |
| §5.5 | Software unit implementation | C++17, heap-free design, Result<T> error handling | `phoenix-measurement/`, `phoenix-ui/` source |
| §5.6 | Software integration | UART IPC protocol, CRC-16 verification | `IPC/UartBridge.h`, `test_uart_bridge.cpp` |
| §5.7 | Software system testing | 115 test suites, 813 assertions, host-based CI | `tests/` directory, test_runner output |
| §5.8 | Software release | Version-tagged builds (v108.0 "Chimera") | `README.md`, version badges |

### Software Maintenance Requirements

| IEC 62304 Clause | Requirement | Implementation | Evidence |
|---|---|---|---|
| §6.1 | Establish maintenance plan | Git-based change control, regression test suite | git workflow, `tests/CMakeLists.txt` |
| §6.2 | Problem & modification analysis | ErrorRegistry persists all errors to NVS | `Core/ErrorRegistry.h` |
| §6.3 | Modification implementation | Feature branches, CI verification | git branch strategy |

### Risk Management Integration (ISO 14971)

| IEC 62304 Clause | Requirement | Implementation | Evidence |
|---|---|---|---|
| §7.1 | Risk analysis update | ErrorCategory enum maps to ISO 14971 risk classes | `Core/Result.h:15` |
| §7.2 | Risk control verification | QC flags on every measurement, benchmark validation | `Analysis/Interfaces.h`, `Services/BenchmarkValidator.h` |
| §7.3 | Risk-benefit analysis | Clinical reference ranges validated per assay | `Calibration/VerifiedAssayRegistry.h` |
| §7.4 | New hazard identification | SafetyManager POST, watchdog, temperature monitoring | `Core/SafetyManager.h` |

---

## Requirements Traceability Matrix

### Safety-Critical Requirements

| Req ID | Description | Source File(s) | Test File(s) | Test Suites |
|---|---|---|---|---|
| REQ-CAL-001 | System SHALL support 5PL curve fitting | `Services/CalibrationService.h` | `test_5pl_math.cpp`, `test_calibration.cpp` | 5PL forward/inverse, LM fitting, factory curves |
| REQ-CAL-002 | Calibration SHALL require ≥ 4 points | `Services/CalibrationService.h:82` | `test_calibration.cpp` | `cal_service_min_points` |
| REQ-MEAS-001 | System SHALL execute measurement in defined sequence | `Analysis/MeasurementPipeline.h` | `test_peak_detection.cpp` | Pipeline stage verification |
| REQ-ALGO-001 | System SHALL support extensible algorithm architecture | `Analysis/Interfaces.h` | `test_peak_detection.cpp` | Strategy pattern interfaces |
| REQ-SAFE-001 | System SHALL perform POST | `Core/SafetyManager.h` | Integration test | RAM, Flash, Camera, LED, UART checks |
| REQ-ERR-001 | System SHALL record and persist all device errors | `Core/ErrorRegistry.h` | `test_error_registry.cpp` | 14 suites: registration, dedup, severity, NVS |
| REQ-LOG-001 | System SHALL maintain audit trail of significant events | `Core/Logger.h` | `test_logger.cpp` | 15 suites: ring-buffer, filtering, diagnostics |
| REQ-BENCH-001 | System SHALL validate results against known benchmarks | `Services/BenchmarkValidator.h` | `test_calibration.cpp` | Reference range validation |

### Communication Requirements

| Req ID | Description | Source File(s) | Test File(s) | Test Suites |
|---|---|---|---|---|
| REQ-IPC-001 | UART frames SHALL be CRC-16/CCITT protected | `IPC/UartBridge.h` | `test_uart_bridge.cpp` | CRC computation, frame encode/decode |
| REQ-IPC-002 | System SHALL detect and recover from sync loss | `IPC/UartBridge.cpp:152` | `test_uart_bridge.cpp` | Resync after invalid bytes |
| REQ-IPC-003 | Max payload SHALL be 1024 bytes | `IPC/UartBridge.h:62` | `test_uart_bridge.cpp` | Payload size validation |

### Data Integrity Requirements

| Req ID | Description | Source File(s) | Test File(s) | Test Suites |
|---|---|---|---|---|
| REQ-NVS-001 | NVS data SHALL use magic number validation | `Services/CalibrationService.h:64`, `Core/ErrorRegistry.h:111` | `test_nvs_storage.cpp` | Magic number roundtrip |
| REQ-NVS-002 | NVS reads SHALL verify version compatibility | `Services/CalibrationService.h:63` | `test_nvs_storage.cpp` | Version check |
| REQ-STR-001 | All strings SHALL be fixed-size (no heap) | `Core/FixedString.h` | `test_fixedstring.cpp` | Construction, format, overflow |
| REQ-RES-001 | All functions SHALL return Result<T> for error propagation | `Core/Result.h` | `test_result.cpp` | Ok/Err paths, TRY macros |

---

## SOUP (Software of Unknown Provenance) List

### Identified SOUP Components

| SOUP Component | Version | Manufacturer | Risk Class | Purpose | Anomaly Assessment |
|---|---|---|---|---|---|
| ESP-IDF | v5.x | Espressif Systems | Medium | RTOS, HAL, WiFi, BLE, NVS, UART drivers | Widely deployed; known errata tracked by vendor; used in medical devices globally |
| FreeRTOS | 10.5+ (bundled with ESP-IDF) | Real Time Engineers Ltd | Medium | Task scheduler, semaphores, queues, timers | Formally verified kernel; SIL-certified variants exist; high industry trust |
| LVGL | 8.x | LVGL LLC | Low | UI rendering on LCD (480×480 ST7701S) | Display only — no influence on measurement result or safety |
| newlib | 4.x (bundled with ESP-IDF) | Red Hat / Cygnus | Low | C standard library (printf, math, string) | Industry-standard; math functions verified against reference implementations |
| PlatformIO | 6.x | PlatformIO Labs | Low | Build system for Wokwi simulation only | Development tool — not present in production firmware |

### SOUP Risk Assessment

| SOUP | Contribution to Hazard | Risk Control |
|---|---|---|
| ESP-IDF UART driver | CRC error could cause corrupted IPC data | CRC-16/CCITT verification on every frame; resync mechanism; error counter |
| ESP-IDF NVS | Calibration data corruption could cause incorrect results | Magic number + version validation on all NVS reads; factory fallback curves |
| ESP-IDF Camera driver | Image corruption could cause incorrect peak detection | 3-frame averaging; QC validation; control line SNR check |
| FreeRTOS scheduler | Task starvation could miss watchdog feed | SafetyManager runs at priority 6 (highest); separate core affinity |
| LVGL | Display error could show incorrect result | Result values verified on Measurement MCU before transmission |
| newlib math (powf, fabsf) | Numerical errors could affect 5PL calculation | Unit tests verify forward/inverse accuracy across all 30 assays; R² validation |

### SOUP Version Control

- ESP-IDF version is locked in `sdkconfig` and `idf_component.yml`
- FreeRTOS version is determined by ESP-IDF version
- LVGL version is locked in `idf_component.yml`
- All SOUP updates require regression testing (115 suites, 813 assertions)

---

## Safety-Critical Function Mapping

### SafetyManager Responsibilities

```
Boot Sequence:
  ┌─────────────────────────────────────┐
  │ SafetyManager::performPowerOnSelfTest()  │
  ├─────────────────────────────────────┤
  │ 1. testRAM()      — PSRAM write/read pattern           │
  │ 2. testFlash()    — NVS partition integrity             │
  │ 3. testCamera()   — OV2686 chip ID + test pattern       │
  │ 4. testLED()      — White, UV, RGB self-test            │
  │ 5. testUART()     — Ping/Pong with peer MCU             │
  └─────────────────┬───────────────────┘
                    │ All pass?
          ┌─────────┼─────────┐
          │ YES     │         │ NO
          ▼         │         ▼
    Normal operation│    emergencyShutdown()
                    │    → LEDs off
                    │    → Log to ErrorRegistry
                    │    → Notify UI MCU
                    │    → Enter safe state
```

### Runtime Monitoring

| Monitor Function | Frequency | Threshold | Action on Failure |
|---|---|---|---|
| `checkMemoryIntegrity()` | Every 10s | Heap < 32 kB | `SAFE_HEAP_LOW` → warning |
| `monitorTemperature()` | Every 30s | > 85°C | `SAFE_OVER_TEMPERATURE` → shutdown |
| `checkVoltages()` | Every 60s | < 3.0 V | `SAFE_VOLTAGE_LOW` → warning |
| `feedWatchdog()` | Every 5s | Timeout 15s | `SAFE_WATCHDOG_TIMEOUT` → reset |

### Watchdog Architecture

```
Priority 6 (highest)
┌───────────────────────────────────┐
│ safety_task (Core 0, 4096B stack) │
│                                   │
│  while(true) {                    │
│    feedWatchdog();                 │
│    checkMemoryIntegrity();         │
│    monitorTemperature();           │
│    checkVoltages();                │
│    vTaskDelay(5000ms);            │
│  }                                │
└───────────────────────────────────┘
```

The watchdog runs at the highest FreeRTOS priority and is pinned to Core 0, separate from the measurement pipeline (Core 1). This ensures the safety monitor cannot be starved by a long-running measurement.

---

## Error Handling Architecture

### Error Flow

```
Detection                    Registration                   Action
┌──────────────┐           ┌──────────────┐           ┌──────────────┐
│ PHOENIX_TRY  │──Err()──►│ ErrorRegistry │──persist──►│ NVS Storage  │
│ returns Err  │           │ .registerError│           │              │
└──────────────┘           └──────┬───────┘           └──────────────┘
                                  │
                                  ▼
                           ┌──────────────┐
                           │ Logger       │──audit──► Ring-buffer (100)
                           │ PHOENIX_LOGE │           available via UART
                           └──────┬───────┘
                                  │
                                  ▼
                           ┌──────────────┐
                           │ UartBridge   │──notify──► UI MCU
                           │ .sendError() │           displays to user
                           └──────────────┘
```

### Error Deduplication and Severity Escalation

When the same error code is registered multiple times:
1. `occurrence_count` is incremented
2. `last_seen_ms` is updated
3. If the new severity is higher, severity is **escalated** (never downgraded)
4. No duplicate entries are created

When the 50-entry buffer is full:
- The oldest `LOW`-severity entry is evicted
- `CRITICAL` and `HIGH` entries are never evicted
- If no `LOW` entries exist, the error is lost (logged to Logger)

---

## Verification Summary

### Test Coverage

| Test File | Suites | Assertions | Module Under Test |
|---|---|---|---|
| `test_5pl_math.cpp` | 19 | ~150 | FivePL_G forward/inverse, all 30 assays |
| `test_peak_detection.cpp` | 14 | ~100 | 7-point peaks, T/C ratio, profile analysis |
| `test_calibration.cpp` | 18 | ~120 | LM fitting, factory curves, CalibrationService |
| `test_result.cpp` | 12 | ~80 | Result<T> Ok/Err, TRY macros |
| `test_fixedstring.cpp` | 14 | ~90 | FixedString<N> operations, formatting |
| `test_uart_bridge.cpp` | 15 | ~100 | CRC-16, frame encoding, commands |
| `test_nvs_storage.cpp` | 8 | ~50 | NVS roundtrip, magic validation |
| `test_logger.cpp` | 15 | ~70 | Ring-buffer, level filtering, macros |
| `test_error_registry.cpp` | 14 | ~53 | Deduplication, severity, buffer-full |
| **Total** | **115** | **813** | |

### Build Targets

| Target | Environment | Compiler | Purpose |
|---|---|---|---|
| `phoenix-measurement` | ESP-IDF v5.x | xtensa-esp32s3-elf-g++ | Production firmware (ESP32 #2) |
| `phoenix-ui` | ESP-IDF v5.x | xtensa-esp32s3-elf-g++ | Production firmware (ESP32 #1) |
| `test_runner` | Linux/macOS host | g++ 13+ (gnu++17) | Unit test verification |
| `wokwi-test` | PlatformIO + Arduino | esp32s3 Arduino core | Simulation |

### Continuous Verification

```bash
# Build and run all tests (host)
cd tests && cmake -B build && cmake --build build && ./build/test_runner
# Expected: 115 suites, 813 assertions — all passing

# Build production firmware
cd phoenix-measurement && idf.py build
cd phoenix-ui && idf.py build
```

---

## Audit Trail

### Logger as Audit Trail (REQ-LOG-001)

The `Logger` singleton records all significant device events in a 100-entry ring-buffer stored in .bss:

| Event Category | Example Events |
|---|---|
| Startup | POST results, initialization sequence |
| Measurement | Pipeline start/stop, progress, result values |
| Calibration | Workflow start, points added, fit results |
| Communication | UART errors, CRC mismatches, connection status |
| Safety | Temperature warnings, voltage alerts, watchdog events |
| User Action | Screen navigation, measurement requests |

Each `LogEntry` contains:
- `timestamp_ms` — milliseconds since boot (monotonic)
- `level` — TRACE through FATAL
- `tag` — module identifier (16 chars)
- `message` — formatted event description (128 chars)

The diagnostics buffer can be exported via UART to the UI MCU using `IpcCommand::DIAGNOSTICS_REPORT` (0x13).

### ErrorRegistry as Persistent Error Log (REQ-ERR-001)

The `ErrorRegistry` persists errors across reboots via NVS. Each error entry includes:
- Error code, severity, category
- Message and recovery hint
- First and last occurrence timestamps
- Total occurrence count

This provides a complete device error history for field service and regulatory audits.
