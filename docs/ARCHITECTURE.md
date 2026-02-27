# Architecture — Phoenix Platform v108.0

## System Overview

Phoenix is a dual-MCU Point-of-Care diagnostics platform. Two ESP32-S3 modules communicate over a CRC-protected UART link. The Measurement MCU handles optical acquisition and quantitative analysis; the UI MCU drives the display, touch input, and connectivity.

```
                         ┌───────────────────────────────────────────────┐
                         │              Igloo Pro Analyzer               │
                         │                                               │
  ┌──────────────────────┼───────────────────┐  ┌───────────────────────┼──────────────────┐
  │  ESP32 #1 — UI MCU                       │  │  ESP32 #2 — Measurement MCU              │
  │                                          │  │                                          │
  │  ┌──────────┐  ┌──────────┐              │  │              ┌──────────┐  ┌──────────┐  │
  │  │ ST7701S  │  │  GT911   │              │  │              │  OV2686  │  │ RGB LEDs │  │
  │  │ 480x480  │  │  Touch   │              │  │              │  Camera  │  │  4-ch    │  │
  │  │ RGB LCD  │  │  I2C     │              │  │              │  DVP     │  │  LEDC    │  │
  │  └────┬─────┘  └────┬─────┘              │  │              └────┬─────┘  └────┬─────┘  │
  │       │              │                    │  │                   │              │        │
  │  ┌────┴──────────────┴─────┐              │  │  ┌────────────────┴──────────────┴──┐    │
  │  │       LVGL UI           │              │  │  │     Measurement Pipeline         │    │
  │  │  PhoenixUI + Screens    │              │  │  │  Profile → Peaks → Calibration   │    │
  │  └────────────┬────────────┘              │  │  └────────────────┬─────────────────┘    │
  │               │                           │  │                   │                      │
  │  ┌────────────┴────────────┐              │  │  ┌────────────────┴─────────────────┐    │
  │  │   MeasurementProxy      │              │  │  │    CalibrationService             │    │
  │  │   (async command proxy)  │              │  │  │    (LM 5PL fitting, NVS)         │    │
  │  └────────────┬────────────┘              │  │  └────────────────┬─────────────────┘    │
  │               │                           │  │                   │                      │
  │  ┌────────────┴────────────┐              │  │  ┌────────────────┴─────────────────┐    │
  │  │       UartBridge        │◄─── UART ───►│  │  │          UartBridge               │    │
  │  │  TX=17 RX=16  921600bd │   CRC-16     │  │  │   TX=17 RX=16  921600bd          │    │
  │  └─────────────────────────┘              │  │  └──────────────────────────────────┘    │
  │               │                           │  │                   │                      │
  │  ┌────────────┴────────────┐              │  │  ┌────────────────┴─────────────────┐    │
  │  │  WiFi / LIMS / BLE      │              │  │  │      SafetyManager               │    │
  │  │  (Cloud connectivity)   │              │  │  │  POST, Watchdog, Diagnostics     │    │
  │  └─────────────────────────┘              │  │  └──────────────────────────────────┘    │
  └───────────────────────────────────────────┘  └─────────────────────────────────────────┘
```

## ESP32 #1 — UI MCU Pin Assignment

| Function | GPIO | Interface | Notes |
|---|---|---|---|
| UART TX (to Meas) | 17 | UART1 | 921600 baud, 8N1 |
| UART RX (from Meas) | 16 | UART1 | |
| LCD Data D0-D7 | 39-46 | RGB Parallel | ST7701S 480x480 |
| LCD PCLK | 38 | RGB | Pixel clock |
| LCD HSYNC | 47 | RGB | |
| LCD VSYNC | 48 | RGB | |
| LCD DE | 0 | RGB | Data enable |
| LCD Backlight | 1 | LEDC PWM | |
| Touch SDA | 8 | I2C0 | GT911 capacitive |
| Touch SCL | 9 | I2C0 | |
| Touch INT | 3 | GPIO Input | Interrupt driven |
| Touch RST | 2 | GPIO Output | |
| WiFi | Internal | ESP32 WiFi | 802.11 b/g/n |
| BLE | Internal | ESP32 BLE | BLE 5.0 |

## ESP32 #2 — Measurement MCU Pin Assignment

| Function | GPIO | Interface | Notes |
|---|---|---|---|
| UART TX (to UI) | 17 | UART1 | 921600 baud, 8N1 |
| UART RX (from UI) | 16 | UART1 | |
| Camera D0-D7 | 11-18 | DVP 8-bit | OV2686 2MP |
| Camera PCLK | 10 | DVP | Pixel clock |
| Camera VSYNC | 6 | DVP | |
| Camera HREF | 7 | DVP | |
| Camera XCLK | 40 | DVP | 20 MHz |
| Camera SDA | 4 | SCCB/I2C | Configuration |
| Camera SCL | 5 | SCCB/I2C | |
| Camera PWDN | 41 | GPIO Output | Power down |
| Camera RESET | 42 | GPIO Output | |
| White LED | 35 | LEDC Ch0 | LFA illumination |
| UV LED 365nm | 36 | LEDC Ch1 | Fluorescence excitation |
| Status LED R | 37 | LEDC Ch2 | RGB status ring |
| Status LED G | 38 | LEDC Ch3 | |
| Status LED B | 39 | LEDC Ch4 | |

## UART IPC Protocol

### Frame Format

```
 Byte:  0     1     2     3     4     5..N+4   N+5   N+6
      ┌─────┬─────┬─────┬─────┬─────┬────────┬─────┬─────┐
      │ 0xAA│ 0x55│LEN_H│LEN_L│ CMD │PAYLOAD │CRC_H│CRC_L│
      └─────┴─────┴─────┴─────┴─────┴────────┴─────┴─────┘
       SYNC word   Length (N)  Command  0..1024   CRC-16/CCITT
                                        bytes    (over CMD+PAYLOAD)
```

| Field | Size | Description |
|---|---|---|
| SYNC | 2 bytes | `0xAA55` — frame start marker |
| LENGTH | 2 bytes | Payload length (big-endian, 0..1024) |
| COMMAND | 1 byte | `IpcCommand` enum value |
| PAYLOAD | 0..1024 bytes | Command-specific data |
| CRC-16 | 2 bytes | CRC-16/CCITT over CMD + PAYLOAD |

**Frame overhead:** 7 bytes (SYNC + LENGTH + CMD + CRC)
**Max frame size:** 1031 bytes (7 + 1024)

### Command Table

| Command | Code | Direction | Payload | Description |
|---|---|---|---|---|
| `ACK` | `0x01` | Meas → UI | — | Command acknowledged |
| `NACK` | `0x02` | Meas → UI | Error info | Command rejected |
| `STATUS_REPORT` | `0x10` | Meas → UI | DiagnosticsReport | System status |
| `MEASUREMENT_PROGRESS` | `0x11` | Meas → UI | pct(1B) + msg | Progress update |
| `MEASUREMENT_RESULT` | `0x12` | Meas → UI | MeasurementResult | Final result |
| `DIAGNOSTICS_REPORT` | `0x13` | Meas → UI | DiagnosticsReport | POST / health |
| `IMAGE_THUMBNAIL` | `0x14` | Meas → UI | JPEG data | Downscaled preview |
| `ERROR_REPORT` | `0x15` | Meas → UI | cat(1B) + msg | Error notification |
| `CALIBRATION_STATUS` | `0x16` | Meas → UI | Status data | Calibration progress |
| `CMD_START_MEASUREMENT` | `0x80` | UI → Meas | AssayConfig | Begin measurement |
| `CMD_CANCEL_MEASUREMENT` | `0x81` | UI → Meas | — | Abort in progress |
| `CMD_START_CALIBRATION` | `0x82` | UI → Meas | Analyte name | Begin calibration |
| `CMD_ADD_CAL_POINT` | `0x83` | UI → Meas | conc + signal | Add data point |
| `CMD_FINISH_CALIBRATION` | `0x84` | UI → Meas | — | Fit and save |
| `CMD_SET_EXPOSURE` | `0x85` | UI → Meas | uint8_t | Camera exposure |
| `CMD_SET_LED` | `0x86` | UI → Meas | LEDConfig | LED control |
| `CMD_CAPTURE_PREVIEW` | `0x87` | UI → Meas | — | Single preview frame |
| `CMD_RUN_DIAGNOSTICS` | `0x88` | UI → Meas | — | Run POST |
| `CMD_GET_STATUS` | `0x89` | UI → Meas | — | Request status |
| `CMD_SHUTDOWN` | `0x8A` | UI → Meas | — | Graceful shutdown |
| `PING` | `0xF0` | Bidirectional | — | Connection test |
| `PONG` | `0xF1` | Bidirectional | — | Connection response |

### Measurement Sequence Diagram

```
  UI MCU                                    Measurement MCU
    │                                             │
    │──── CMD_START_MEASUREMENT (0x80) ──────────►│
    │                                             │── LED warmup
    │◄──── MEASUREMENT_PROGRESS (0x11) [10%] ────│── Image capture
    │◄──── MEASUREMENT_PROGRESS (0x11) [30%] ────│── Profile extraction
    │◄──── MEASUREMENT_PROGRESS (0x11) [50%] ────│── Peak detection
    │◄──── MEASUREMENT_PROGRESS (0x11) [70%] ────│── Calibration
    │◄──── MEASUREMENT_PROGRESS (0x11) [90%] ────│── QC validation
    │                                             │
    │◄──── MEASUREMENT_RESULT (0x12) ────────────│── Result with concentration
    │                                             │
    │──── ACK (0x01) ────────────────────────────►│
    │                                             │
```

## Measurement Pipeline (10 Stages)

```
  Stage 1        Stage 2        Stage 3         Stage 4         Stage 5
 ┌──────────┐  ┌──────────┐  ┌───────────┐  ┌───────────┐  ┌───────────┐
 │ Preflight│─►│LED Warmup│─►│  Capture   │─►│  Extract  │─►│ Baseline  │
 │  Checks  │  │  500ms   │  │ 3-frame avg│  │  Profile  │  │ Estimation│
 └──────────┘  └──────────┘  └───────────┘  └───────────┘  └───────────┘
                                                                   │
  Stage 10       Stage 9        Stage 8        Stage 7        Stage 6
 ┌──────────┐  ┌──────────┐  ┌───────────┐  ┌───────────┐  ┌───────────┐
 │  Result  │◄─│  LED Off  │◄─│    QC     │◄─│Calibrate  │◄─│   Peak    │
 │  Output  │  │  Cleanup  │  │ Validation│  │   5PL     │  │ Detection │
 └──────────┘  └──────────┘  └───────────┘  └───────────┘  └───────────┘
```

| Stage | Class | Description |
|---|---|---|
| 1. Preflight | `MeasurementPipeline` | Check camera, LED, calibration availability |
| 2. LED Warmup | `ILEDController` | White LED on, stabilize 500ms |
| 3. Capture | `ICameraController` | Capture 3 frames, average for noise reduction |
| 4. Profile Extract | `IProfileExtractor` | ROI → 1D intensity profile (green channel) |
| 5. Baseline | `IBaselineEstimator` | Polynomial baseline estimation and subtraction |
| 6. Peak Detection | `IPeakFinder` / `Dx365PeakDetector` | 7-point symmetric peak descriptor |
| 7. Calibration | `CalibrationService` | 5PL inverse: T/C ratio → concentration |
| 8. QC Validation | `MeasurementPipeline` | Control line SNR, range check, benchmark |
| 9. LED Off | `ILEDController` | Cleanup, power save |
| 10. Result | `MeasurementResult` | Concentration, QC flags, confidence |

## FreeRTOS Task Architecture

### Measurement MCU

| Task | Priority | Stack | Core | Description |
|---|---|---|---|---|
| `measurement_task` | 5 (high) | 8192 B | Core 1 | Pipeline execution |
| `ipc_rx_task` | 4 | 4096 B | Core 0 | UART receive + dispatch |
| `safety_task` | 6 (highest) | 4096 B | Core 0 | Watchdog feed, health check |
| `main_task` | 1 (idle+1) | 4096 B | Core 0 | Initialization, event loop |

### UI MCU

| Task | Priority | Stack | Core | Description |
|---|---|---|---|---|
| `lvgl_task` | 3 | 8192 B | Core 1 | LVGL tick + render (16ms) |
| `ipc_rx_task` | 4 | 4096 B | Core 0 | UART receive + callbacks |
| `wifi_task` | 2 | 4096 B | Core 0 | WiFi + LIMS sync |
| `main_task` | 1 | 4096 B | Core 0 | Initialization, event loop |

## Memory Budget

### Heap-Free Design Rationale

IEC 62304 Class C requires deterministic memory behavior. All Phoenix data structures use fixed-size buffers allocated in .bss (zero-initialized static storage). This eliminates:

- Heap fragmentation over device lifetime (24/7 operation)
- Non-deterministic allocation times during real-time measurement
- Memory leak risk in safety-critical paths

### Static Memory Allocation

| Component | Size | Segment | Notes |
|---|---|---|---|
| `ImageBuffer` | 307 kB | .bss | 640x480 grayscale |
| `LineProfile` (x2) | 3.2 kB | .bss | 2 x 400 floats |
| `Logger` ring-buffer | 15.2 kB | .bss | 100 x LogEntry (152 B) |
| `ErrorRegistry` | 7.5 kB | .bss | 50 x ErrorEntry (150 B) |
| `CalibrationData` | ~2 kB | .bss / NVS | Active calibration |
| `VerifiedAssayRegistry` | ~12 kB | .rodata | 30 assay configs |
| `FactoryCalibrationRegistry` | ~8 kB | stack | 8 factory curves |
| `ServiceLocator` | 0.6 kB | .bss | 16 x Entry (34 B) |
| `FixedString` instances | varies | stack/.bss | 16 / 32 / 64 / 128 / 256 B |
| **Total static** | **~356 kB** | | Well within ESP32-S3 512 kB SRAM |

### NVS (Non-Volatile Storage) Usage

| Namespace | Key | Size | Description |
|---|---|---|---|
| `cal_store` | `<cal_id>` | ~2 kB | Saved calibration curves |
| `err_reg` | `err_data` | ~7.5 kB | Persisted error entries |
| `cc_cal` | `cal_data` | ~0.5 kB | Color chart calibration |
| `audit` | `trail_*` | ~2 kB | Audit trail entries |
