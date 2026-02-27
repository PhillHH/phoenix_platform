# API Reference — Phoenix Platform v108.0

## Module Overview

```
phoenix::
├── Core/
│   ├── Result<T>           — Error handling without exceptions
│   ├── FixedString<N>      — Heap-free strings
│   ├── Logger              — Structured logging ring-buffer
│   ├── ErrorRegistry       — Persistent error tracking
│   ├── SafetyManager       — POST, watchdog, diagnostics
│   └── ServiceLocator      — Type-erased service registry
│
├── HAL/
│   ├── ICameraController   — Camera abstraction (OV2686)
│   └── ILEDController      — LED abstraction (White/UV/RGB)
│
├── Analysis/
│   ├── FivePL_G            — 5PL model (Dx365 convention)
│   ├── Dx365PeakDetector   — 7-point peak detection
│   ├── IProfileExtractor   — Image → 1D profile
│   ├── IBaselineEstimator  — Baseline estimation/subtraction
│   ├── IPeakFinder         — Peak finding interface
│   └── MeasurementPipeline — 10-stage pipeline orchestrator
│
├── Calibration/
│   ├── CalibrationService      — LM fitting, NVS storage
│   ├── VerifiedAssayRegistry   — 30 verified assays
│   ├── FactoryCalibration      — 8 factory reference curves
│   └── ColorChartCalibrator    — 11-strip color chart calibration
│
├── Services/
│   └── BenchmarkValidator  — Clinical reference range validation
│
└── IPC/
    ├── UartBridge          — CRC-16 UART frame transport
    └── MeasurementProxy    — Async command proxy (UI MCU)
```

---

## Core Module

### `Result<T>` — Error Handling

**File:** `Core/Result.h`

Rust-style `Result<T>` type for embedded C++ — no exceptions, no heap allocation.

#### Types

```cpp
// Error categories (ISO 14971 risk classification)
enum class ErrorCategory : uint8_t {
    NONE, HARDWARE_FAILURE, CALIBRATION_ERROR, MEASUREMENT_ERROR,
    COMMUNICATION_ERROR, MEMORY_ERROR, INVALID_PARAMETER,
    SAFETY_VIOLATION, TIMEOUT, NOT_FOUND, NOT_INITIALIZED, BUSY
};

// Error info (fixed-size, 85 bytes)
struct ErrorInfo {
    ErrorCategory category;
    char message[80];
    uint32_t code;
};
```

#### Construction

```cpp
// Success
Result<int> ok_val = Ok(42);
Result<void> ok_void = Ok();

// Error
Result<int> err_val = Err<int>(ErrorCategory::HARDWARE_FAILURE, "Camera init failed");
Result<void> err_void = Err(ErrorCategory::TIMEOUT, "No response");
```

#### Usage Patterns

```cpp
// Check and extract
Result<float> res = calibration.signalToConcentration(signal);
if (res.is_ok()) {
    float concentration = res.value();
} else {
    ErrorInfo err = res.error();
    // err.category, err.message, err.code
}

// value_or (with default)
float conc = res.value_or(0.0f);

// PHOENIX_TRY — propagate errors (like Rust's ? operator)
Result<void> init() {
    PHOENIX_TRY(camera.init());      // Returns Err if camera fails
    PHOENIX_TRY(led.init());         // Returns Err if LED fails
    return Ok();
}

// PHOENIX_TRY_VAL — propagate and bind value
Result<float> measure() {
    PHOENIX_TRY_VAL(image, camera.captureImage());
    PHOENIX_TRY_VAL(profile, extractor.extractProfile(image, roi));
    return Ok(profile.values[0]);
}
```

### `FixedString<N>` — Heap-Free Strings

**File:** `Core/FixedString.h`

Fixed-size character buffer with string operations. No heap allocation.

| Alias | Size | Typical Use |
|---|---|---|
| `String16` | 16 bytes | Tags, short IDs |
| `String32` | 32 bytes | Assay codes, calibration IDs |
| `String64` | 64 bytes | Names, error messages |
| `String128` | 128 bytes | Log messages, descriptions |
| `String256` | 256 bytes | Long messages, paths |

```cpp
// Construction
FixedString<64> name("C-Reactive Protein");
String32 code = "CRP";

// Operations
const char* c = code.c_str();     // "CRP"
size_t len = code.length();        // 3
bool empty = code.empty();         // false
size_t cap = code.capacity();      // 31

// Formatting
String128 msg;
msg.format("Concentration: %.2f ng/mL", 42.5f);

// Comparison
if (code == "CRP") { /* match */ }

// Append
String64 full;
full.append("Hello ");
full.append("World");
```

### `Logger` — Structured Logging

**File:** `Core/Logger.h`

Singleton ring-buffer logger with 100 entries, entirely in .bss. Dual-platform: ESP_LOGx on ESP32, ANSI-colored printf on host.

#### Log Levels

| Level | Value | ESP32 Equivalent | Color (Host) |
|---|---|---|---|
| `TRACE` | 0 | `ESP_LOGD` | Gray |
| `DEBUG` | 1 | `ESP_LOGD` | Cyan |
| `INFO` | 2 | `ESP_LOGI` | Green |
| `WARN` | 3 | `ESP_LOGW` | Yellow |
| `ERROR` | 4 | `ESP_LOGE` | Red |
| `FATAL` | 5 | `ESP_LOGE` | Bold Red |

#### API

```cpp
class Logger {
public:
    static constexpr int RING_BUFFER_SIZE = 100;
    static Logger& instance();

    // Level filter
    void setLevel(LogLevel min_level);
    LogLevel getLevel() const;

    // Log (variadic + zero-arg overload)
    template<typename... Args>
    void log(LogLevel level, const char* tag, const char* fmt, Args... args);
    void log(LogLevel level, const char* tag, const char* msg);

    // Diagnostics
    int getEntryCount() const;
    const LogEntry* getEntry(int reverse_idx) const;  // 0 = most recent
    int getDiagnosticsBuffer(LogEntry* out, int max) const;
    int getEntriesByLevel(LogLevel min, LogEntry* out, int max) const;
    void clear();

    static const char* levelName(LogLevel level);
};
```

#### Macros

```cpp
PHOENIX_LOGT("CAM", "Frame captured at %ums", timestamp);
PHOENIX_LOGD("CAL", "R² = %.4f", r_squared);
PHOENIX_LOGI("PIPE", "Measurement started");
PHOENIX_LOGW("TEMP", "Temperature high: %.1f°C", temp);
PHOENIX_LOGE("UART", "CRC mismatch: 0x%04X", crc);
PHOENIX_LOGF("SAFE", "Emergency shutdown: %s", reason);
```

### `ErrorRegistry` — Persistent Error Tracking

**File:** `Core/ErrorRegistry.h`

50-entry error registry with deduplication, severity escalation, and NVS persistence.

#### Error Severity (ISO 14971)

| Severity | Value | Patient Risk | Action |
|---|---|---|---|
| `LOW` | 0 | None | Log only |
| `MEDIUM` | 1 | Acceptable | Degrade gracefully |
| `HIGH` | 2 | May affect accuracy | Alert user |
| `CRITICAL` | 3 | Patient safety risk | Immediate action |

#### API

```cpp
class ErrorRegistry {
public:
    static constexpr int MAX_ERRORS = 50;
    static ErrorRegistry& instance();

    // Register (deduplicates by code, escalates severity)
    void registerError(uint16_t code, ErrorSeverity severity,
                       ErrorCategory category, const char* message,
                       const char* recovery_hint = nullptr);

    // Query
    int getErrorCount() const;
    const ErrorEntry* findByCode(uint16_t code) const;
    int countBySeverity(ErrorSeverity min) const;
    int countByCategory(ErrorCategory cat) const;
    bool hasCriticalErrors() const;
    void clearErrors();

    // NVS persistence
    Result<void> persistToNvs();
    Result<void> loadFromNvs();
};
```

#### Usage

```cpp
auto& reg = ErrorRegistry::instance();

reg.registerError(ErrorCode::HW_CAMERA_INIT_FAIL,
                  ErrorSeverity::HIGH,
                  ErrorCategory::HARDWARE_FAILURE,
                  "Camera init failed",
                  "Check CSI ribbon cable");

// Query
if (reg.hasCriticalErrors()) {
    // Block measurements
}

// Persist across reboots
reg.persistToNvs();
```

### `SafetyManager` — POST and Health Monitoring

**File:** `Core/SafetyManager.h`

```cpp
class SafetyManager {
public:
    Result<DiagnosticsReport> performPowerOnSelfTest();
    Result<void> checkMemoryIntegrity();
    Result<void> monitorTemperature();
    Result<void> checkVoltages();
    void emergencyShutdown(const char* reason);
    Result<void> initWatchdog(uint32_t timeout_ms);
    void feedWatchdog();
    DiagnosticsReport getLastReport() const;
};
```

The `DiagnosticsReport` struct contains heap stats, temperature, battery voltage, and per-subsystem pass/fail flags.

### `ServiceLocator` — Service Registry

**File:** `Core/ServiceLocator.h`

Type-erased service registry with 16 fixed slots.

```cpp
auto& loc = ServiceLocator::getInstance();

// Register
CalibrationService cal_svc;
loc.registerService("calibration", &cal_svc);

// Retrieve
auto res = loc.getService<CalibrationService>("calibration");
if (res.is_ok()) {
    CalibrationService* svc = res.value();
}

// Check
bool has = loc.hasService("calibration");
```

---

## HAL Module

### `ICameraController` — Camera Interface

**File:** `HAL/Interfaces.h`

```cpp
class ICameraController {
public:
    virtual Result<void>        initialize(const CameraConfig& cfg) = 0;
    virtual Result<ImageBuffer> captureImage() = 0;
    virtual Result<void>        setExposure(uint8_t value) = 0;     // 0–255
    virtual Result<void>        setGain(uint8_t value) = 0;         // 0–255
    virtual Result<void>        selfTest() = 0;
    virtual void                deinitialize() = 0;
};
```

Implementation: `CameraController_OV2686` for the Supertek SHWX01 module (DVP 8-bit, 640×480 grayscale).

### `ILEDController` — LED Interface

**File:** `HAL/Interfaces.h`

```cpp
enum class LEDMode : uint8_t {
    OFF, WHITE, UV_365NM, STATUS_RGB
};

class ILEDController {
public:
    virtual Result<void> initialize() = 0;
    virtual Result<void> setMode(const LEDConfig& cfg) = 0;
    virtual Result<void> selfTest() = 0;
    virtual void         off() = 0;
};
```

---

## Analysis Module

### `FivePL_G` — 5PL Model (Dx365)

**File:** `Analysis/Dx365Algorithm.h`

```cpp
struct FivePL_G {
    float A, B, C, D, G;

    float evaluate(float x) const;  // concentration → signal
    float inverse(float y) const;   // signal → concentration
};
```

### `Dx365PeakDetector` — 7-Point Peak Detection

**File:** `Analysis/Dx365Algorithm.h`

```cpp
class Dx365PeakDetector {
public:
    Result<void> detectPeaks(
        const LineProfile& profile,
        const AssayLine* expected_lines,
        uint8_t num_lines,
        PeakDescriptor* out_peaks,
        uint8_t& out_count);

    static float computePeakValue(const LineProfile& profile, const PeakDescriptor& peak);
    static float computeTCRatio(float test_value, float control_value);
};
```

The 7-point peak descriptor captures: `leftExpect → leftLimit → leftPeak → center → rightPeak → rightLimit → rightExpect`.

### `MeasurementPipeline` — 10-Stage Orchestrator

**File:** `Analysis/MeasurementPipeline.h`

```cpp
class MeasurementPipeline {
public:
    MeasurementPipeline(
        ICameraController* camera, ILEDController* led,
        IProfileExtractor* extractor, IBaselineEstimator* baseline,
        IPeakFinder* peaks, CalibrationService* calibration,
        BenchmarkValidator* benchmark = nullptr);

    Result<MeasurementResult> runMeasurement(const PipelineConfig& config);
    void cancel();
    PipelineState getState() const;
    void setProgressCallback(ProgressCallback cb, void* ctx);
    const ImageBuffer* getLastImage() const;
    const Profile1D* getLastProfile() const;
};
```

Pipeline states: `IDLE → VALIDATING → LED_WARMUP → CAPTURING → EXTRACTING → BASELINE → PEAK_DETECTION → CALIBRATING → QC_VALIDATION → COMPLETE`.

### Analysis Interfaces (Strategy Pattern)

**File:** `Analysis/Interfaces.h`

```cpp
class IProfileExtractor {
    virtual Result<Profile1D> extractProfile(const ImageBuffer& image, const ROI& roi) = 0;
};

class IBaselineEstimator {
    virtual Result<Profile1D> estimateBaseline(const Profile1D& raw) = 0;
    virtual Result<Profile1D> subtractBaseline(const Profile1D& raw, const Profile1D& baseline) = 0;
};

class IPeakFinder {
    virtual Result<PeakResult> findPeaks(const Profile1D& corrected,
                                          float min_height = 0.05f,
                                          float min_distance = 0.5f) = 0;
};
```

---

## Calibration Module

### `CalibrationService` — LM Fitting

**File:** `Services/CalibrationService.h`

```cpp
class CalibrationService {
public:
    // Calibration workflow
    Result<void>        startCalibrationWorkflow(const char* analyte);
    Result<void>        addCalibrationPoint(float concentration, float signal);
    Result<FivePLParams> fitCurve();
    Result<void>        validateCalibration();
    Result<void>        saveCalibration(const char* id);

    // Retrieve
    Result<CalibrationData> getActiveCalibration() const;
    Result<CalibrationData> loadCalibration(const char* id);

    // Apply
    Result<float> signalToConcentration(float signal) const;
    Result<float> concentrationToSignal(float conc) const;

    bool hasActiveCalibration() const;
    uint8_t getPointCount() const;
};
```

### `VerifiedAssayRegistry` — 30 Assays

**File:** `Calibration/VerifiedAssayRegistry.h`

```cpp
struct VerifiedAssayRegistry {
    AssayConfig assays[32];
    size_t count;

    void registerAll();
    const AssayConfig* findById(const char* id) const;
    const AssayConfig* findByLoinc(const char* loinc) const;
};

VerifiedAssayRegistry& getVerifiedAssays();
```

### `FactoryCalibrationRegistry` — 8 Factory Curves

**File:** `Calibration/FactoryCalibration.h`

```cpp
struct FactoryCalibrationRegistry {
    FactoryCalibrationCurve curves[16];
    size_t count;

    void registerAll();
    const FactoryCalibrationCurve* findByCode(const char* code) const;
    bool verifyAgainstFactory(const char* code, const FivePLParams& field_params) const;
};

FactoryCalibrationRegistry& getFactoryCalibrations();
```

### `ColorChartCalibrator` — 11-Strip Calibration

**File:** `Calibration/ColorChartCalibration.h`

```cpp
class ColorChartCalibrator {
public:
    Result<void> setStripReading(uint8_t strip_id, const StripReading& reading);
    Result<void> validateReadings();
    Result<ColorChartCalibrationResult> computeCalibration();
    Result<void> saveCalibration(const ColorChartCalibrationResult& cal);
    Result<ColorChartCalibrationResult> loadCalibration();

    float applyIntensityCorrection(float raw) const;
    void  applyWhiteBalance(float& r, float& g, float& b) const;
    float getGoldNPReference() const;
    bool  hasActiveCalibration() const;
};
```

---

## IPC Module

### `UartBridge` — CRC-16 Frame Transport

**File:** `IPC/UartBridge.h`

```cpp
class UartBridge {
public:
    Result<void> initialize(const UartConfig& config = {});
    void         deinitialize();

    // Send
    Result<void> send(IpcCommand cmd, const void* payload = nullptr, size_t len = 0);
    Result<void> sendResult(const MeasurementResult& result);
    Result<void> sendProgress(uint8_t percentage, const char* message);
    Result<void> sendError(ErrorCategory cat, const char* message);

    // Receive
    Result<IpcCommand> receive(void* payload_out, size_t* len_out,
                                uint32_t timeout_ms = 1000);
    bool hasData() const;
    Result<void> ping(uint32_t timeout_ms = 500);

    // Statistics
    uint32_t getTxCount() const;
    uint32_t getRxCount() const;
    uint32_t getErrorCount() const;
};
```

### `MeasurementProxy` — Async Command Proxy (UI MCU)

**File:** `IPC/MeasurementProxy.h`

```cpp
class MeasurementProxy {
public:
    explicit MeasurementProxy(UartBridge* uart);

    // Commands to Measurement MCU
    Result<void> startMeasurement();
    Result<void> cancelMeasurement();
    Result<void> startCalibration(const char* analyte);
    Result<void> addCalibrationPoint(float concentration, float signal);
    Result<void> finishCalibration();
    Result<void> requestDiagnostics();
    Result<void> requestStatus();
    Result<void> ping();

    // Async callbacks
    void setCallbacks(const ProxyCallbacks& cb);
    void poll(uint32_t timeout_ms = 10);

    const MeasurementResult* getLastResult() const;
    bool isConnected() const;
};
```

---

## UI Module

### `PhoenixUI` — LVGL Screen Manager

**File:** `UI/PhoenixUI.h`

```cpp
enum class ScreenID : uint8_t {
    HOME, MEASUREMENT, RESULT, CALIBRATION,
    PATIENT, SETTINGS, SYSTEM_MENU
};

class PhoenixUI {
public:
    Result<void> initialize(lv_disp_t* disp);
    void runTick();
    void navigateTo(ScreenID screen);

    void onMeasurementProgress(uint8_t pct, const char* msg);
    void onMeasurementResult(const MeasurementResult& result);
    void onMeasurementError(ErrorCategory cat, const char* msg);

    void setProxy(MeasurementProxy* proxy);
};
```

---

## Error Codes Reference

### Hardware Errors (0x0100 – 0x01FF)

| Code | Symbol | Severity | Description |
|---|---|---|---|
| `0x0100` | `HW_CAMERA_INIT_FAIL` | HIGH | Camera initialization failed |
| `0x0101` | `HW_CAMERA_CAPTURE_FAIL` | HIGH | Image capture failure |
| `0x0110` | `HW_LED_FAILURE` | MEDIUM | LED controller fault |
| `0x0111` | `HW_LED_OVERCURRENT` | HIGH | LED current limit exceeded |
| `0x0120` | `HW_TEMP_SENSOR_FAIL` | MEDIUM | Temperature sensor not responding |
| `0x0121` | `HW_TEMP_OVER_LIMIT` | CRITICAL | Operating temperature exceeded |
| `0x0130` | `HW_ADC_FAILURE` | MEDIUM | ADC conversion fault |
| `0x0140` | `HW_FLASH_FAILURE` | HIGH | Flash memory read/write error |

### Calibration Errors (0x0200 – 0x02FF)

| Code | Symbol | Severity | Description |
|---|---|---|---|
| `0x0200` | `CAL_FIT_DIVERGED` | MEDIUM | LM fitting did not converge |
| `0x0201` | `CAL_R2_TOO_LOW` | MEDIUM | R² below acceptance threshold |
| `0x0202` | `CAL_RESIDUALS_HIGH` | MEDIUM | Residual error too large |
| `0x0210` | `CAL_EXPIRED` | LOW | Calibration past expiry date |
| `0x0211` | `CAL_NOT_FOUND` | MEDIUM | No calibration data for assay |
| `0x0220` | `CAL_NVS_SAVE_FAIL` | HIGH | NVS write failure |
| `0x0221` | `CAL_NVS_LOAD_FAIL` | MEDIUM | NVS read failure |
| `0x0230` | `CAL_INSUFFICIENT_POINTS` | MEDIUM | < 4 calibration points |

### Measurement Errors (0x0300 – 0x03FF)

| Code | Symbol | Severity | Description |
|---|---|---|---|
| `0x0300` | `MEAS_PROFILE_INVALID` | MEDIUM | 1D profile extraction failed |
| `0x0301` | `MEAS_PEAK_NOT_FOUND` | MEDIUM | Expected peak not detected |
| `0x0302` | `MEAS_CONTROL_LINE_WEAK` | MEDIUM | Control line SNR below threshold |
| `0x0310` | `MEAS_QC_FAIL` | MEDIUM | QC validation failed |
| `0x0311` | `MEAS_OUT_OF_RANGE` | LOW | Concentration outside reportable range |
| `0x0320` | `MEAS_IMAGE_CORRUPT` | HIGH | Camera image data corrupt |
| `0x0321` | `MEAS_ROI_INVALID` | MEDIUM | ROI detection failed |

### Communication Errors (0x0400 – 0x04FF)

| Code | Symbol | Severity | Description |
|---|---|---|---|
| `0x0400` | `COMM_UART_CRC_ERROR` | LOW | CRC-16 mismatch on received frame |
| `0x0401` | `COMM_UART_FRAME_TIMEOUT` | LOW | Frame not received within timeout |
| `0x0402` | `COMM_UART_SYNC_LOST` | MEDIUM | Sync word not found (resync needed) |
| `0x0403` | `COMM_UART_OVERFLOW` | MEDIUM | RX buffer overflow |
| `0x0410` | `COMM_WIFI_DISCONNECT` | LOW | WiFi connection lost |
| `0x0420` | `COMM_BLE_FAILURE` | LOW | BLE communication failure |
| `0x0430` | `COMM_IPC_NACK` | MEDIUM | Measurement MCU rejected command |

### Safety Errors (0x0500 – 0x05FF)

| Code | Symbol | Severity | Description |
|---|---|---|---|
| `0x0500` | `SAFE_POST_FAIL` | CRITICAL | Power-on self test failed |
| `0x0501` | `SAFE_WATCHDOG_TIMEOUT` | CRITICAL | Watchdog timer expired |
| `0x0502` | `SAFE_MEMORY_CORRUPTION` | CRITICAL | RAM integrity check failed |
| `0x0503` | `SAFE_HEAP_LOW` | HIGH | Free heap below safe threshold |
| `0x0504` | `SAFE_STACK_OVERFLOW` | CRITICAL | Task stack overflow detected |
| `0x0510` | `SAFE_VOLTAGE_LOW` | HIGH | Battery/supply voltage low |
| `0x0511` | `SAFE_OVER_TEMPERATURE` | CRITICAL | Over-temperature shutdown |
| `0x05FF` | `SAFE_EMERGENCY_SHUTDOWN` | CRITICAL | Emergency shutdown triggered |

---

## Example Workflow: Measurement from Start to Result

### Step 1 — UI MCU: Start Measurement

```cpp
// UI MCU (phoenix-ui)
MeasurementProxy proxy(&uart);

proxy.setCallbacks({
    .on_result   = [](const MeasurementResult& r, void*) {
        printf("Concentration: %.2f %s\n", r.concentration_ng_ml, r.unit.c_str());
    },
    .on_progress = [](uint8_t pct, const char* msg, void*) {
        printf("[%d%%] %s\n", pct, msg);
    },
    .on_error    = [](ErrorCategory cat, const char* msg, void*) {
        printf("ERROR: %s\n", msg);
    },
});

PHOENIX_TRY(proxy.startMeasurement());
```

### Step 2 — Measurement MCU: Pipeline Execution

```cpp
// Measurement MCU (phoenix-measurement)
MeasurementPipeline pipeline(&camera, &led, &extractor,
                              &baseline, &peaks, &calibration);

pipeline.setProgressCallback([](const PipelineProgress& p, void* ctx) {
    auto* uart = static_cast<UartBridge*>(ctx);
    uart->sendProgress(p.percentage, p.message.c_str());
}, &uart);

auto result = pipeline.runMeasurement(config);
if (result.is_ok()) {
    uart.sendResult(result.value());
} else {
    uart.sendError(result.error().category, result.error().message);
}
```

### Step 3 — Calibration Workflow

```cpp
CalibrationService cal;
PHOENIX_TRY(cal.startCalibrationWorkflow("CRP"));

// Add serial dilution points
PHOENIX_TRY(cal.addCalibrationPoint(0.0f,   0.05f));
PHOENIX_TRY(cal.addCalibrationPoint(5.0f,   0.43f));
PHOENIX_TRY(cal.addCalibrationPoint(25.0f,  3.50f));
PHOENIX_TRY(cal.addCalibrationPoint(100.0f, 7.55f));
PHOENIX_TRY(cal.addCalibrationPoint(200.0f, 8.16f));

PHOENIX_TRY_VAL(params, cal.fitCurve());
// params.r_squared should be > 0.990

PHOENIX_TRY(cal.validateCalibration());
PHOENIX_TRY(cal.saveCalibration("CRP-2026-02"));

// Apply to measurement
PHOENIX_TRY_VAL(concentration, cal.signalToConcentration(3.50f));
// concentration ≈ 25.0 mg/L
```

### Step 4 — Color Chart Calibration

```cpp
ColorChartCalibrator cc;

// Acquire readings from camera
for (int i = 0; i < 11; i++) {
    StripReading reading = captureStrip(i);
    PHOENIX_TRY(cc.setStripReading(i, reading));
}

PHOENIX_TRY(cc.validateReadings());
PHOENIX_TRY_VAL(cal_result, cc.computeCalibration());

if (cal_result.r_squared > 0.95f) {
    PHOENIX_TRY(cc.saveCalibration(cal_result));
}

// Apply corrections to measurement data
float corrected = cc.applyIntensityCorrection(raw_pixel);
```

---

## QC Flags Reference

| Flag | Value | Meaning |
|---|---|---|
| `NONE` | `0x00` | No QC issues |
| `CONTROL_LINE_WEAK` | `0x01` | Control line SNR below threshold |
| `HIGH_BACKGROUND` | `0x02` | Background noise too high |
| `LOW_SNR` | `0x04` | Signal-to-noise ratio insufficient |
| `OUT_OF_RANGE` | `0x08` | Concentration outside reportable range |
| `CALIBRATION_OLD` | `0x10` | Calibration nearing or past expiry |

QC flags are bitwise-OR combined:

```cpp
if (hasFlag(result.qc_flags, QCFlag::CONTROL_LINE_WEAK)) {
    // Warn: control line is weak, result may be unreliable
}
```
