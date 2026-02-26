# Phoenix Platform v108.0 "Chimera" — Strukturierter Projektreport

**Datum:** 2026-02-26
**Analysiert durch:** Automatisierte Code-Analyse
**Commit-Basis:** Branch `claude/analyze-project-structure-UNkOR`

---

## 1. Projektstruktur

### 1.1 Gesamtübersicht

Das Projekt besteht aus **3 Subprojekten** für eine Dual-MCU medizinische Gerätefirmware:

| Subprojekt | Zweck | Build-System | C++ Standard |
|---|---|---|---|
| `phoenix-measurement` | Kamera, Analyse, Kalibrierung (ESP32 #2) | ESP-IDF CMake | C++20 |
| `phoenix-ui` | Display, Touch, WiFi, LIMS (ESP32 #1) | ESP-IDF CMake | C++20 |
| `wokwi-test` | Wokwi-Simulator Integrationstest | PlatformIO + Arduino | gnu++17 |

### 1.2 Dateiliste mit Größen

#### phoenix-measurement (17 Dateien)

| Datei | Größe | Beschreibung |
|---|---|---|
| `main/include/phoenix/Core/Result.h` | 5.6K | Error-Handling Framework (Result<T>) |
| `main/include/phoenix/Core/FixedString.h` | 1.9K | Heap-freie Fixed-Size Strings |
| `main/include/phoenix/Core/ServiceLocator.h` | 2.5K | Typsicherer Service-Registry |
| `main/include/phoenix/Core/SafetyManager.h` | 1.8K | IEC 62304 Safety-Monitor |
| `main/include/phoenix/Core/AssayTechnology.h` | 634B | Technology-Enum (4 Typen) |
| `main/include/phoenix/Analysis/Dx365Algorithm.h` | 16K | 5PL-Modell, Peak-Detektion, Assay-Config |
| `main/include/phoenix/Analysis/MeasurementPipeline.h` | 5.2K | 9-Stufen Mess-Pipeline |
| `main/include/phoenix/Analysis/Interfaces.h` | 4.4K | Strategy-Pattern Analyse-Interfaces |
| `main/include/phoenix/Calibration/VerifiedAssayRegistry.h` | 21K | 30 verifizierte Assays |
| `main/include/phoenix/Calibration/FactoryCalibration.h` | 16K | 8 Fabrik-Kalibrierungskurven |
| `main/include/phoenix/Calibration/ColorChartCalibration.h` | 9.0K | 11-Strip Farbkarten-Kalibrierung |
| `main/include/phoenix/Services/CalibrationService.h` | 4.7K | 5PL Levenberg-Marquardt Fitting |
| `main/include/phoenix/Services/BenchmarkValidator.h` | 2.8K | Klinische Referenzbereiche |
| `main/include/phoenix/IPC/UartBridge.h` | 4.3K | UART IPC Protokoll |
| `main/include/phoenix/HAL/Interfaces.h` | 3.0K | HAL-Abstraktion (Kamera, LED) |
| `main/include/phoenix/HAL/CameraController_OV2686.h` | 1.9K | OV2686 DVP Kamera-Treiber |
| `main/src/*.cpp` (16 Dateien) | ~133K | Implementierungen |

#### phoenix-ui (20 Dateien)

| Datei | Größe | Beschreibung |
|---|---|---|
| `main/include/phoenix/Core/Result.h` | 5.1K | Error-Handling (vereinfachter PHOENIX_TRY) |
| `main/include/phoenix/Core/FixedString.h` | 1.9K | Identisch mit measurement |
| `main/include/phoenix/Core/ServiceLocator.h` | 2.5K | Identisch mit measurement |
| `main/include/phoenix/Core/MeasurementTypes.h` | 4.9K | UI-seitige Mess-Typen |
| `main/include/phoenix/Core/AssayTechnology.h` | 7.2K | 16 Demo-Assays, 4 Technologien |
| `main/include/phoenix/UI/PhoenixUI.h` | 1.6K | LVGL UI-Manager |
| `main/include/phoenix/UI/CircularMenu.h` | 1.4K | Animiertes Kreismenü |
| `main/include/phoenix/IPC/UartBridge.h` | 4.3K | UART IPC (Pins: TX=43, RX=44) |
| `main/include/phoenix/IPC/MeasurementProxy.h` | 2.1K | UI→Measurement Proxy |
| `main/src/*.cpp` (19 Dateien) | ~167K | Implementierungen |

#### wokwi-test (2 Dateien)

| Datei | Größe | Beschreibung |
|---|---|---|
| `src/main.cpp` | 43K | PlatformIO/Arduino-Version (1055 Zeilen) |
| `main/main.cpp` | 42K | ESP-IDF-Version (1046 Zeilen) |

### 1.3 Abhängigkeiten

```
phoenix-measurement
├── ESP-IDF v5.3: driver, freertos, nvs_flash, esp_timer, esp_pm
└── Intern: Core → HAL → Analysis → Calibration → Services → IPC

phoenix-ui
├── ESP-IDF v5.3: driver, freertos, nvs_flash, esp_wifi, esp_http_client,
│                  esp_timer, lvgl, esp_lcd, esp_pm, mbedtls
└── Intern: Core → HAL → UI → Services → Connectivity → IPC

wokwi-test
├── PlatformIO: espressif32 + Arduino Framework
└── ESP-IDF APIs: ledc, gpio, nvs, freertos, esp_timer, esp_log
```

---

## 2. platformio.ini Analyse

**Datei:** `wokwi-test/platformio.ini`

```ini
[env:esp32dev]
platform = espressif32
board = esp32dev
framework = arduino
monitor_speed = 115200
build_flags =
    -std=gnu++17
    -DCORE_DEBUG_LEVEL=4
    -DBOARD_HAS_PSRAM=0
build_unflags = -std=gnu++11
```

| Parameter | Wert | Bemerkung |
|---|---|---|
| **Board** | `esp32dev` | ESP32-WROOM-32D |
| **Framework** | `arduino` | Arduino Core for ESP32 |
| **C++ Standard** | `gnu++17` | Override von Arduino-Default `gnu++11` |
| **Build-Flags** | `-std=gnu++17`, `CORE_DEBUG_LEVEL=4`, `BOARD_HAS_PSRAM=0` | |
| **Unflags** | `-std=gnu++11` | Entfernt Arduino-Default |

### Kritische Beobachtung: C++ Standard Mismatch

| Subprojekt | C++ Standard | Compiler |
|---|---|---|
| `phoenix-measurement` | **C++20** (CMake) | GCC 13.2 (ESP-IDF v5.3) |
| `phoenix-ui` | **C++20** (CMake) | GCC 13.2 (ESP-IDF v5.3) |
| `wokwi-test` | **gnu++17** (PlatformIO) | GCC (Arduino toolchain) |

Die ESP-IDF Projekte setzen C++20 (`CMAKE_CXX_STANDARD 20`), der Wokwi-Test nur gnu++17. Kein kritisches Problem, da der Code kein C++20-Feature aktiv nutzt.

---

## 3. Kompilierung

### 3.1 PlatformIO Build (`pio run`)

**Status:** Fehlgeschlagen — ESP32 Platform konnte nicht heruntergeladen werden (Netzwerk-Fehler in Sandbox-Umgebung).

```
Platform Manager: Installing espressif32
HTTPClientError:
```

### 3.2 GCC Syntax-Check (Stub-Headers)

Als Ersatz wurde `g++ 13.3.0` mit Stub-Headers für ESP-IDF/Arduino verwendet.

#### 3.2.1 gnu++17 (konfigurierter Standard) — 0 ERRORS, 7 WARNINGS

```
WARNINGS mit -Wall -Wextra -Wpedantic:
```

| # | Datei:Zeile | Typ | Beschreibung |
|---|---|---|---|
| W1 | `src/main.cpp:628` | `-Wunused-variable` | `err_pct` berechnet aber nicht verwendet |
| W2 | `src/main.cpp:689` | `-Wunused-variable` | `tc2` berechnet aber nicht verwendet |
| W3 | `src/main.cpp:715` | `-Wunused-variable` | `crc2` berechnet aber nicht verwendet |
| W4 | `src/main.cpp:907` | `-Wunused-variable` | `min_heap` berechnet aber nicht verwendet |
| W5 | `src/main.cpp:937` | `-Wunused-variable` | `uptime` berechnet aber nicht verwendet |
| W6 | `src/main.cpp:945` | `-Wunused-parameter` | `arg` Parameter nicht verwendet |
| W7 | `main/main.cpp:537,547` | `-Wunused-parameter` | `test_name`/`name` in `check()`/`check_float()` (ESP-IDF-Version) |

#### 3.2.2 gnu++17 + `-Wconversion -Wshadow` — 0 ERRORS, 13+ WARNINGS zusätzlich

| # | Datei:Zeile | Typ | Beschreibung |
|---|---|---|---|
| W8 | `src/main.cpp:259` | `-Wconversion` | `int` → `uint16_t` in `expect_left` Berechnung |
| W9 | `src/main.cpp:260-261` | `-Wconversion` | `int` → `uint16_t` in `expect_right` Berechnung |
| W10 | `src/main.cpp:269` | `-Wconversion` | `int` → `float` bei `bl_count` Division |
| W11 | `src/main.cpp:313` | `-Wconversion` | `int` → `float` bei `count` Division |
| W12 | `src/main.cpp:452,458,464` | `-Wconversion` | `int` → `float` bei Loop-Index Subtraktion |
| W13 | `src/main.cpp:848,852` | `-Wconversion` | `int` → `uint8_t` bei `led_set(CH_G, duty)` |

> **Hinweis:** `-Wconversion` und `-Werror` sind im `phoenix-measurement/CMakeLists.txt` auskommentiert (Zeile 69), was darauf hinweist, dass diese Warnings bekannt sind.

#### 3.2.3 gnu++11 — 8 ERRORS (Fatal)

| # | Datei:Zeile | Typ | Beschreibung |
|---|---|---|---|
| **E1** | `src/main.cpp:131` | `error` | `constexpr` Constructor hat nicht-leeren Body |
| **E2** | `src/main.cpp:423` | `error` | Keine `operator=` für `FivePL_G` von Brace-Init-List |
| **E3** | `src/main.cpp:426` | `error` | Keine `operator=` für `FivePL_G` von Brace-Init-List |
| **E4** | `src/main.cpp:429` | `error` | Keine `operator=` für `FivePL_G` von Brace-Init-List |
| **E5** | `src/main.cpp:602` | `error` | Kann `{...}` nicht nach `FivePL_G` konvertieren |
| **E6** | `src/main.cpp:873` | `error` | Kann `{...}` nicht nach `FivePL_G` konvertieren |
| **E7** | `src/main.cpp:423-429` | `error` | (Auch in `main/main.cpp`, identisch) |
| **E8** | `src/main.cpp:602,873` | `error` | (Auch in `main/main.cpp`, identisch) |

#### 3.2.4 gnu++14 — 0 ERRORS, gleiche Warnings wie gnu++17

gnu++14 kompiliert fehlerfrei, da relaxed constexpr und NSDMIs in Aggregates ab C++14 unterstützt werden.

---

## 4. Code-Analyse

### 4.1 Namespaces

| Namespace | Verwendet in | Beschreibung |
|---|---|---|
| `phoenix` | Alle Dateien | Haupt-Namespace |
| `phoenix::ui` | `CircularMenu.h` | UI-Widgets |
| (global) | `wokwi-test` (Sections 7-9) | LED-Control, Test-Suite |

### 4.2 Klassen und Structs

#### Core-Framework

| Typ | Datei | Zeile | Art |
|---|---|---|---|
| `ErrorInfo` | `Core/Result.h` | :31 | `struct` mit NSDMI + constexpr default ctor |
| `Result<T>` | `Core/Result.h` | :57 | `class` Template, Rust-style Ok/Err |
| `Result<void>` | `Core/Result.h` | :103 | `class` Template-Spezialisierung |
| `FixedString<N>` | `Core/FixedString.h` | :14 | `class` Template, heap-frei |
| `ServiceLocator` | `Core/ServiceLocator.h` | :16 | `class` Singleton, type-erased |
| `SafetyManager` | `Core/SafetyManager.h` | :28 | `class`, IEC 62304 POST + Runtime |
| `DiagnosticsReport` | `Core/SafetyManager.h` | :14 | `struct` mit NSDMI |

#### Analysis / Algorithmen

| Typ | Datei | Zeile | Art |
|---|---|---|---|
| `FivePL_G` | `Analysis/Dx365Algorithm.h` | :40 | `struct` mit NSDMI, 5PL-Modell |
| `PeakDescriptor` | `Analysis/Dx365Algorithm.h` | :75 | `struct` 7-Punkt Peak |
| `LineProfile` | `Analysis/Dx365Algorithm.h` | :102 | `struct` mit NSDMI, 400 float Array |
| `AssayLine` | `Analysis/Dx365Algorithm.h` | :117 | `struct` ohne NSDMI |
| `AssayConfig` | `Analysis/Dx365Algorithm.h` | :133 | `struct` Assay-Konfiguration |
| `Dx365PeakDetector` | `Analysis/Dx365Algorithm.h` | :175 | `class` Peak-Erkennung |
| `Dx365MeasurementPipeline` | `Analysis/Dx365Algorithm.h` | :189 | `class` Mess-Pipeline |
| `MeasurementPipeline` | `Analysis/MeasurementPipeline.h` | :52 | `class` Haupt-Pipeline (9 Stufen) |
| `Profile1D` | `Analysis/Interfaces.h` | :18 | `struct` mit NSDMI |
| `Peak` | `Analysis/Interfaces.h` | :27 | `struct` ohne NSDMI |
| `MeasurementResult` | `Analysis/Interfaces.h` | :47 | `struct` mit NSDMI |
| `IProfileExtractor` | `Analysis/Interfaces.h` | :92 | `class` Interface (virtual) |
| `IBaselineEstimator` | `Analysis/Interfaces.h` | :98 | `class` Interface (virtual) |
| `IPeakFinder` | `Analysis/Interfaces.h` | :104 | `class` Interface (virtual) |

#### Kalibrierung

| Typ | Datei | Zeile | Art |
|---|---|---|---|
| `FivePLParams` | `Services/CalibrationService.h` | :14 | `struct` mit NSDMI (A,B,C,D,E) |
| `CalibrationData` | `Services/CalibrationService.h` | :25 | `struct` NVS-persistiert |
| `CalibrationService` | `Services/CalibrationService.h` | :42 | `class` Levenberg-Marquardt |
| `ColorChartCalibrator` | `Calibration/ColorChartCalibration.h` | :130 | `class` 11-Strip Pipeline |
| `VerifiedAssayRegistry` | `Calibration/VerifiedAssayRegistry.h` | :425 | `struct` 30 Assays |
| `FactoryCalibrationRegistry` | `Calibration/FactoryCalibration.h` | :339 | `struct` 8 Fabrik-Kurven |
| `BenchmarkValidator` | `Services/BenchmarkValidator.h` | :35 | `class` klinische Referenzbereiche |

#### HAL

| Typ | Datei | Zeile | Art |
|---|---|---|---|
| `ICameraController` | `HAL/Interfaces.h` | :34 | `class` Interface (virtual) |
| `ILEDController` | `HAL/Interfaces.h` | :52 | `class` Interface (virtual) |
| `CameraController_OV2686` | `HAL/CameraController_OV2686.h` | :23 | `class` OV2686 Treiber |
| `ImageBuffer` | `HAL/Interfaces.h` | :17 | `struct` mit NSDMI |

#### IPC

| Typ | Datei | Zeile | Art |
|---|---|---|---|
| `UartBridge` | `IPC/UartBridge.h` | :68 | `class` UART Frame-Protokoll |
| `MeasurementProxy` | `IPC/MeasurementProxy.h` | :30 | `class` UI→Measurement Proxy |

#### UI

| Typ | Datei | Zeile | Art |
|---|---|---|---|
| `PhoenixUI` | `UI/PhoenixUI.h` | :19 | `class` LVGL Screen-Manager |
| `CircularMenu` | `UI/CircularMenu.h` | :18 | `class` Animiertes Kreismenü |

### 4.3 Externe Libraries

| Library | Subprojekt | Verwendung |
|---|---|---|
| **ESP-IDF v5.3** | measurement, ui | Basis-Framework |
| **FreeRTOS** | measurement, ui, wokwi | Tasks, Delays, Semaphore |
| **LVGL** | ui | Touch-Display GUI |
| **NVS Flash** | measurement, ui, wokwi | Persistenter Speicher |
| **ESP WiFi** | ui | WLAN-Verbindung |
| **ESP HTTP Client** | ui | LIMS-Integration |
| **mbedTLS** | ui | TLS/Bearer-Token Auth |
| **ESP LCD** | ui | ST7701S Display-Treiber |
| **LEDC** | measurement, wokwi | LED PWM-Steuerung |
| **Arduino Core** | wokwi | Serial, setup()/loop() |

### 4.4 Enums

| Enum | Datei | Typ |
|---|---|---|
| `ErrorCategory` | `Core/Result.h` | `enum class : uint8_t` (12 Werte) |
| `Technology` | `Core/AssayTechnology.h` | `enum class : uint8_t` (4 Werte) |
| `SignalType` | `Analysis/Dx365Algorithm.h` | `enum class : uint8_t` (4 Werte) |
| `ScaleType` | `Analysis/Dx365Algorithm.h` | `enum class : uint8_t` (2 Werte) |
| `IpcCommand` | `IPC/UartBridge.h` | `enum class : uint8_t` (20+ Werte) |
| `LEDMode` | `HAL/Interfaces.h` | `enum class : uint8_t` (4 Werte) |
| `QCFlag` | `Analysis/Interfaces.h` | `enum class : uint8_t` (Bitflags) |
| `PipelineState` | `Analysis/MeasurementPipeline.h` | `enum class : uint8_t` (11 Werte) |
| `ScreenID` | `UI/PhoenixUI.h` | `enum class : uint8_t` (7 Werte) |

---

## 5. Spezifische Prüfungen

### 5.1 Alle `constexpr` Verwendungen

#### Problematisch bei C++11:

| Datei | Zeile | Code | C++11? | C++14+? |
|---|---|---|---|---|
| `Core/FixedString.h` | :16 | `constexpr FixedString() { buf_[0] = '\0'; }` | **FEHLER** — Body nicht leer | OK (Relaxed constexpr) |
| `Core/Result.h` | :36 | `constexpr ErrorInfo() = default;` | **FEHLER** — NSDMIs machen Ctor non-trivial | OK |

#### Unkritisch (alle C++ Standards):

| Datei | Zeile | Code | Status |
|---|---|---|---|
| `wokwi-test/src/main.cpp` | :181-182 | `static constexpr size_t MAX_PROFILE_LEN = 400;` | OK |
| `wokwi-test/src/main.cpp` | :358-360 | `static constexpr uint16_t FRAME_SYNC = 0xAA55;` | OK |
| `wokwi-test/src/main.cpp` | :478-488 | `static constexpr gpio_num_t PIN_LED_*` | OK |
| `wokwi-test/src/main.cpp` | :916 | `constexpr size_t TEST_SIZE = 4096;` | OK |
| `HAL/Interfaces.h` | :18-20 | `static constexpr uint16_t IMAGE_WIDTH = 640;` | OK |
| Diverse | Diverse | `static constexpr` für Konfigurationswerte | OK |

### 5.2 Brace-Initialisierungen `= { ... }` bei Struct-Zuweisungen

#### Pattern A: Aggregate Init von Structs MIT NSDMIs (C++11 INKOMPATIBEL)

`FivePL_G` hat NSDMIs (`float A = 0.0f;` etc.) und ist daher in C++11 **kein Aggregate**.

| Datei | Zeile | Code | C++11? | C++14+? |
|---|---|---|---|---|
| `wokwi-test/src/main.cpp` | :423 | `cfg.div_5pl = { -0.000202f, 1.59834f, 255.486f, 0.28367f, 10.0f };` | **FEHLER** | OK |
| `wokwi-test/src/main.cpp` | :426 | `cfg.test_5pl = { 0.01896f, 1.75754f, 117.834f, 5.82638f, 3.77055f };` | **FEHLER** | OK |
| `wokwi-test/src/main.cpp` | :429 | `cfg.control_5pl = { -12581.79f, ... };` | **FEHLER** | OK |
| `wokwi-test/src/main.cpp` | :602 | `FivePL_G ige = { -0.000202f, ... };` | **FEHLER** | OK |
| `wokwi-test/src/main.cpp` | :873 | `FivePL_G cal = { -0.000202f, ... };` | **FEHLER** | OK |
| `Dx365Algorithm.h` | :264-266 | `cfg.lines[n] = { ... };` (in Assay-Definitionen) | OK (kein NSDMI auf AssayLine) | OK |
| `VerifiedAssayRegistry.h` | Diverse | `c.test_5pl = { ... };` (30 Assays) | **FEHLER** | OK |
| `FactoryCalibration.h` | :77-84 | `cal.verify_points[n] = {0.0f, 0.050f, 15.0f};` | OK (kein NSDMI auf VerificationPoint) | OK |

#### Pattern B: Value Init `= {}` — In allen Standards OK

| Datei | Zeile | Code | Status |
|---|---|---|---|
| `wokwi-test/src/main.cpp` | :413 | `AssayConfig cfg = {};` | OK — ruft Default-Ctor |
| `wokwi-test/src/main.cpp` | :333 | `PeakDescriptor peak = {};` | OK — Aggregate ohne NSDMI |
| `wokwi-test/src/main.cpp` | :442 | `LineProfile profile = {};` | OK — Default-Ctor |
| `wokwi-test/src/main.cpp` | :491 | `ledc_timer_config_t timer_cfg = {};` | OK — C struct |

#### Pattern C: Brace-Init von `AssayLine` Array-Elementen

```cpp
cfg.lines[0] = {"ctrl", true, 47.0f, 44.0f, 0, "ctrl"};
```

`AssayLine` hat **keine NSDMIs**, daher ist es in **allen C++ Standards ein Aggregate**. Die `String32`/`String64`-Member werden über ihren Converting Constructor `FixedString(const char*)` initialisiert. **Kein Problem.**

### 5.3 C++11 vs C++14/17 Inkompatibilitäten — Zusammenfassung

| Feature | Minimum-Standard | Verwendet in | Auswirkung |
|---|---|---|---|
| Relaxed constexpr (Body mit Statements) | **C++14** | `FixedString()` Ctor | Fatal Error bei C++11 |
| NSDMIs in Aggregates | **C++14** | `FivePL_G` Brace-Init | Fatal Error bei C++11 |
| `constexpr` defaulted Ctor mit NSDMIs | **C++14** | `ErrorInfo()` | Fatal Error bei C++11 |
| `[[nodiscard]]` Attribut | **C++17** | `Result.h` (ESP-IDF Version) | Ignored oder Warning bei C++11/14 |
| Range-based for mit `auto&` | **C++11** | Überall | OK |
| `enum class` | **C++11** | Überall | OK |
| Template variadic args | **C++11** | `FixedString::format()` | OK |
| `= default` / `= delete` | **C++11** | `ErrorInfo`, Interfaces | OK |

> **Fazit:** Der Code ist **C++14-kompatibel**, benötigt aber **mindestens C++14**. C++11 ist nicht möglich. Die `platformio.ini` setzt korrekt `gnu++17`.

---

## 6. Fix-Vorschläge

### Fix 1: Unused Variables (W1-W5) — `wokwi-test/src/main.cpp`

**Problem:** 5 Variablen werden berechnet aber nur für ESP_LOGI genutzt, das im Stub leer ist.

```cpp
// Zeile 628: err_pct berechnet aber nur in ESP_LOGI verwendet
float err_pct = fabsf(recovered - conc) / conc * 100.0f;
```

**Fix:** `(void)` Cast oder `[[maybe_unused]]` (C++17):

```cpp
// Option A: Expliziter void-Cast
float err_pct = fabsf(recovered - conc) / conc * 100.0f;
(void)err_pct;  // Used in ESP_LOGI below

// Option B: [[maybe_unused]] (C++17)
[[maybe_unused]] float err_pct = fabsf(recovered - conc) / conc * 100.0f;
```

Betrifft auch:
- Zeile 689: `tc2`
- Zeile 715: `crc2`
- Zeile 907: `min_heap`
- Zeile 937: `uptime`

### Fix 2: Unused Parameter (W6) — `wokwi-test/src/main.cpp:945`

```cpp
// Vorher:
static void phoenix_main_task(void* arg) {

// Nachher:
static void phoenix_main_task(void* /* arg */) {
```

### Fix 3: -Wconversion Warnings (W8-W13)

**Zeile 259-261:** Integer-Promotion bei uint16_t Arithmetik:

```cpp
// Vorher (Zeile 259):
uint16_t expect_left = (center > half_w + 20) ? center - half_w - 20 : 0;

// Nachher:
uint16_t expect_left = (center > half_w + 20)
    ? static_cast<uint16_t>(center - half_w - 20) : uint16_t(0);
```

**Zeile 269, 313:** int → float bei Division:

```cpp
// Vorher (Zeile 269):
if (bl_count > 0) baseline /= bl_count;

// Nachher:
if (bl_count > 0) baseline /= static_cast<float>(bl_count);
```

**Zeile 452, 458, 464:** int Loop-Variable → float:

```cpp
// Vorher (Zeile 452):
for (int i = 20; i < 75; i++) {
    float x = (i - 47.0f) / 10.0f;

// Nachher:
for (int i = 20; i < 75; i++) {
    float x = (static_cast<float>(i) - 47.0f) / 10.0f;
```

**Zeile 848, 852:** int → uint8_t bei LED PWM:

```cpp
// Vorher (Zeile 847-848):
for (int duty = 0; duty <= 255; duty += 15) {
    led_set(CH_G, duty);

// Nachher:
for (int duty = 0; duty <= 255; duty += 15) {
    led_set(CH_G, static_cast<uint8_t>(duty));
```

### Fix 4: Falls C++11 Kompatibilität gewünscht wäre (E1-E8)

> **Hinweis:** Derzeit NICHT nötig, da platformio.ini korrekt gnu++17 setzt. Nur relevant, falls jemand den Arduino-Default gnu++11 nutzen will.

**E1 — constexpr FixedString() mit nicht-leerem Body (Zeile 131):**

```cpp
// Vorher:
constexpr FixedString() { buf_[0] = '\0'; }

// Fix für C++11 (nicht nötig für C++14+):
FixedString() { buf_[0] = '\0'; }
// Oder: constexpr aus buf_[] NSDMI entfernen und im Ctor-Body initialisieren
```

**E2-E8 — FivePL_G Aggregate Init mit NSDMIs:**

```cpp
// Vorher (Zeile 423):
cfg.div_5pl = { -0.000202f, 1.59834f, 255.486f, 0.28367f, 10.0f };

// Fix für C++11: Explizite Member-Zuweisung
cfg.div_5pl.A = -0.000202f;
cfg.div_5pl.B = 1.59834f;
cfg.div_5pl.C = 255.486f;
cfg.div_5pl.D = 0.28367f;
cfg.div_5pl.G = 10.0f;

// Oder: NSDMIs aus FivePL_G entfernen (macht es zum Aggregate in C++11)
struct FivePL_G {
    float A, B, C, D, G;  // ohne = 0.0f etc.
    // ...
};
```

### Fix 5: ESP-IDF CMakeLists.txt — `-Werror` / `-Wconversion` deaktiviert

**Datei:** `phoenix-measurement/main/CMakeLists.txt:69`

```cmake
# -Werror -Wconversion  # Re-enable after all warnings are resolved
```

**Empfehlung:** Die Conversion-Warnings in den Source-Dateien fixen (siehe Fix 3) und dann `-Wconversion` wieder aktivieren. `-Werror` sollte für IEC 62304 Class C Compliance aktiviert sein.

### Fix 6: Duplizierter Code — wokwi-test/src/main.cpp vs main/main.cpp

Die Dateien `wokwi-test/src/main.cpp` (1055 Zeilen) und `wokwi-test/main/main.cpp` (1046 Zeilen) sind nahezu identisch. Unterschiede:

- `src/main.cpp` hat zusätzliche `Serial.printf()`-Debug-Ausgaben
- `src/main.cpp` hat `Serial.printf()` auch in `check()` Funktion bei FAIL

**Empfehlung:** Eine der beiden Dateien als Single-Source-of-Truth verwenden oder über `#ifdef` die Unterschiede steuern.

---

## 7. Gesamtbewertung

| Aspekt | Status | Details |
|---|---|---|
| Kompiliert mit gnu++17 | **OK** | 0 Errors, nur Warnings |
| Kompiliert mit gnu++14 | **OK** | 0 Errors |
| Kompiliert mit gnu++11 | **FAIL** | 8 Errors (constexpr, aggregate init) |
| Code-Qualität | **Gut** | Saubere Architektur, Strategy Pattern, IEC 62304 |
| -Wconversion clean | **Nein** | ~7 Conversion Warnings |
| -Wunused clean | **Nein** | ~7 Unused Variable/Parameter Warnings |
| C++ Standard konsistent | **Teilweise** | C++20 (ESP-IDF) vs gnu++17 (PlatformIO) |
| Duplizierter Code | **Ja** | 2x wokwi main.cpp mit minimalen Unterschieden |

### Architektur-Highlights

- Rust-style `Result<T>` Error-Handling ohne Exceptions (`-fno-exceptions`)
- 30 verifizierte Assays aus echten Dx365 MCP-Projekten
- 5PL + Levenberg-Marquardt Kalibrierung
- IPC über UART mit CRC-16/CCITT Frame-Protokoll
- LVGL Touch-UI mit Circular Menu, 9 Sprachen
- IEC 62304 Class C Compliance (Safety Manager, Audit Trail, POST)
- Heap-freies Design (FixedString, keine STL-Container im Core)
