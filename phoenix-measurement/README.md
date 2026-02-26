# Phoenix v108.0 "Chimera" — Measurement MCU Firmware

## Target Hardware
- **MCU:** ESP32-WROOM-32D (Igloo Pro)
- **Role:** ESP32 #2 — Kamera, Analyse, Kalibrierung
- **Camera:** OV2686 2MP DVP (Supertek SHWX01)
- **IPC:** UART1 921600 baud → ESP32 #1 (UI/Connectivity)

## Architecture

```
ESP32 #2 (Measurement MCU)
┌─────────────────────────────────────────────┐
│                                             │
│  app_main()                                 │
│    └─ main_task (Core 1, 16kB stack)        │
│         │                                   │
│         ├─ SafetyManager (POST + runtime)   │
│         ├─ UartBridge (IPC to UI MCU)       │
│         ├─ CameraController_OV2686          │
│         ├─ IglooLEDController               │
│         │                                   │
│         └─ MeasurementPipeline              │
│              ├─ DefaultProfileExtractor      │
│              ├─ MedianBaselineEstimator      │
│              ├─ SimplePeakFinder            │
│              ├─ CalibrationService (5PL)    │
│              └─ BenchmarkValidator          │
│                                             │
│  ServiceLocator (type-erased registry)      │
│  AuditTrail (NVS-backed event log)          │
└──────────────────┬──────────────────────────┘
                   │ UART1 (TX=17, RX=16)
                   │ 921600 baud
                   │ Frame: [AA55][LEN][CMD][PAYLOAD][CRC16]
                   │
┌──────────────────┴──────────────────────────┐
│  ESP32 #1 (UI/Connectivity MCU)             │
│  - LVGL Circular Menu                       │
│  - WiFi/BLE                                 │
│  - Cloud Sync                               │
│  - LIMS Integration                         │
│  (separate firmware project — Phase 3)      │
└─────────────────────────────────────────────┘
```

## Build

```bash
# Prerequisites: ESP-IDF v5.3+
. $HOME/esp/esp-idf/export.sh

# Add esp32-camera component
idf.py add-dependency "espressif/esp32-camera"

# Build
idf.py set-target esp32
idf.py build

# Flash
idf.py -p /dev/ttyUSB0 flash monitor
```

## Measurement Pipeline Flow

```
1. VALIDATING     (5%)   Pre-flight: camera + LED self-test
2. LED_WARMUP    (10%)   White LED on, 500ms stabilize
3. CAPTURING     (25%)   3-frame average, grayscale 640x480
4. EXTRACTING    (40-50%) Auto-ROI → 1D profile extraction
5. BASELINE      (60%)   Median rolling baseline estimation
6. PEAK_DETECTION(70%)   Local maxima + SNR + FWHM + area
7. CALIBRATING   (80%)   5PL inverse → concentration
8. QC_VALIDATION (90%)   Benchmark + QC flags
9. COMPLETE     (100%)   Result sent to UI MCU via UART
```

## IPC Protocol

| Direction | Command | Description |
|-----------|---------|-------------|
| UI→Meas | `0x80` CMD_START_MEASUREMENT | Start pipeline |
| UI→Meas | `0x81` CMD_CANCEL_MEASUREMENT | Abort |
| UI→Meas | `0x82` CMD_START_CALIBRATION | Begin cal workflow |
| UI→Meas | `0x83` CMD_ADD_CAL_POINT | Add concentration+signal |
| UI→Meas | `0x84` CMD_FINISH_CALIBRATION | Fit + validate + save |
| UI→Meas | `0x88` CMD_RUN_DIAGNOSTICS | Run POST |
| Meas→UI | `0x11` MEASUREMENT_PROGRESS | pct + message |
| Meas→UI | `0x12` MEASUREMENT_RESULT | Full MeasurementResult |
| Meas→UI | `0x13` DIAGNOSTICS_REPORT | DiagnosticsReport |
| Meas→UI | `0x15` ERROR_REPORT | ErrorCategory + message |
| Both     | `0xF0/F1` PING/PONG | Connection test |

## File Structure

```
phoenix-measurement/
├── CMakeLists.txt                    # Root ESP-IDF project
├── sdkconfig.defaults                # Hardware config
├── README.md
└── main/
    ├── CMakeLists.txt                # Component registration
    ├── include/phoenix/
    │   ├── Core/
    │   │   ├── Result.h              # Error handling (no exceptions)
    │   │   ├── FixedString.h         # Heap-free strings
    │   │   ├── ServiceLocator.h      # Service registry
    │   │   └── SafetyManager.h       # POST + safety
    │   ├── HAL/
    │   │   ├── Interfaces.h          # Camera + LED interfaces
    │   │   └── CameraController_OV2686.h
    │   ├── Analysis/
    │   │   ├── Interfaces.h          # Profile, Peak, Result types
    │   │   └── MeasurementPipeline.h # Pipeline orchestrator
    │   ├── Services/
    │   │   ├── CalibrationService.h  # 5PL curve fitting
    │   │   └── BenchmarkValidator.h  # Clinical reference ranges
    │   └── IPC/
    │       └── UartBridge.h          # UART IPC protocol
    └── src/
        ├── main.cpp                  # Entry point
        ├── Core/
        │   ├── SafetyManager.cpp
        │   └── AuditTrail.cpp
        ├── HAL/
        │   ├── CameraController_OV2686.cpp
        │   └── LEDController.cpp
        ├── Analysis/
        │   ├── ImageProcessor.cpp    # Profile + Baseline + PeakFinder
        │   ├── MeasurementPipeline.cpp
        │   └── CurveCorrection.cpp
        ├── Services/
        │   ├── CalibrationService.cpp
        │   └── BenchmarkValidator.cpp
        └── IPC/
            ├── UartBridge.cpp
            └── CommandProtocol.cpp
```

## Compliance Notes
- **IEC 62304 Class C:** Full traceability, safety manager, audit trail
- **ISO 14971:** Risk categories mapped to ErrorCategory enum
- **No exceptions / no RTTI:** `-fno-exceptions -fno-rtti` for deterministic behavior
- **Result<T>:** Rust-style error propagation via PHOENIX_TRY macro
- **Fixed-size allocations:** No heap fragmentation in long-running operation
