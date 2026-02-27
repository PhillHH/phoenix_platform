# Development Runbook — Phoenix Platform

## Development Environment Setup

### ESP-IDF v5.3 Installation

#### Linux (Ubuntu/Debian)

```bash
# Install prerequisites
sudo apt-get update
sudo apt-get install -y git wget flex bison gperf python3 python3-pip \
    python3-venv cmake ninja-build ccache libffi-dev libssl-dev dfu-util \
    libusb-1.0-0

# Clone ESP-IDF
mkdir -p ~/esp
cd ~/esp
git clone -b v5.3 --recursive https://github.com/espressif/esp-idf.git
cd esp-idf

# Install toolchains (esp32 + esp32s3 for both MCUs)
./install.sh esp32,esp32s3

# Activate (add to ~/.bashrc for persistence)
. ~/esp/esp-idf/export.sh
```

Verify:
```bash
idf.py --version
# Expected: ESP-IDF v5.3
```

#### macOS

```bash
# Install prerequisites
brew install cmake ninja dfu-util python3 ccache

# Clone and install (same as Linux)
mkdir -p ~/esp && cd ~/esp
git clone -b v5.3 --recursive https://github.com/espressif/esp-idf.git
cd esp-idf
./install.sh esp32,esp32s3
. ~/esp/esp-idf/export.sh
```

#### Windows

1. Download the [ESP-IDF Tools Installer](https://dl.espressif.com/dl/esp-idf/) for v5.3
2. Run the installer — it installs Python, Git, CMake, Ninja, and the toolchains
3. Use the "ESP-IDF Command Prompt" shortcut for all commands below

### PlatformIO in VS Code

1. Open VS Code
2. Go to **Extensions** (Ctrl+Shift+X)
3. Search for **"PlatformIO IDE"** and install
4. Wait for PlatformIO to finish installing its dependencies
5. Restart VS Code

Verify:
```bash
pio --version
# Expected: PlatformIO Core, version 6.x
```

### Wokwi VS Code Extension

1. In VS Code, go to **Extensions**
2. Search for **"Wokwi Simulator"** and install
3. Get a free license key at [wokwi.com/license](https://wokwi.com/license)
4. Press **F1** > **"Wokwi: Request a New License"** and paste your key

### Host Test Toolchain (g++17)

#### Linux
```bash
sudo apt-get install -y g++ cmake
g++ --version
# Requires g++ 13+ for full C++17 support
```

#### macOS
```bash
brew install gcc cmake
# Use g++-13 or later (not Apple Clang)
export CXX=g++-13
```

#### Windows
```bash
# Via MSYS2
pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-cmake
```

---

## Build Instructions

### phoenix-measurement (ESP32 #2 — Measurement MCU)

```bash
cd phoenix-measurement

# First time: set target
idf.py set-target esp32

# Build
idf.py build

# Flash (connect to left USB-C port)
idf.py -p /dev/ttyUSB0 flash

# Monitor serial output
idf.py -p /dev/ttyUSB0 monitor

# Build + Flash + Monitor (combined)
idf.py -p /dev/ttyUSB0 flash monitor
```

**Build configuration:** `sdkconfig.defaults` sets:
- Target: ESP32 (WROOM-32D)
- Flash: 4MB, QIO, 80MHz
- PSRAM: 4MB, 80MHz
- CPU: 240MHz dual-core
- Watchdog: 10s timeout
- Stack overflow canary: enabled
- Heap poisoning: comprehensive

### phoenix-ui (ESP32-S3 — UI MCU)

```bash
cd phoenix-ui

# First time: set target
idf.py set-target esp32s3

# Build
idf.py build

# Flash (connect to right USB-C port, USB-JTAG)
idf.py -p /dev/ttyACM0 flash

# Monitor
idf.py -p /dev/ttyACM0 monitor
```

**Build configuration:** `sdkconfig.defaults` sets:
- Target: ESP32-S3
- Flash: 16MB, QIO, 80MHz
- PSRAM: 8MB Octal, 80MHz
- CPU: 240MHz dual-core
- LVGL: 64KB memory, 16-bit color, Montserrat fonts (14–48pt)
- LCD RGB: ISR IRAM safe
- Console: USB-JTAG

### wokwi-test (Simulation)

```bash
cd wokwi-test

# PlatformIO build (Arduino framework)
pio run

# Upload to Wokwi simulator
pio run --target upload
```

**Configuration:** `platformio.ini` sets:
- Platform: espressif32
- Board: esp32dev
- Framework: Arduino
- C++ standard: gnu++17
- Shared header: `-I shared`

### Host Unit Tests

```bash
cd tests

# Configure and build
cmake -B build
cmake --build build

# Run all 115 suites
./build/test_runner
# Expected: 115 suites, 813 assertions — all passing

# Run with verbose output
./build/test_runner 2>&1 | head -50
```

**Requirements:**
- g++ 13+ or clang++ 16+ with `-std=gnu++17`
- CMake 3.16+
- No ESP-IDF needed — uses host stubs in `tests/host_stubs/`

---

## Debugging

### Serial Monitor

Both MCUs output structured logs via their respective debug interfaces.

| MCU | Port | Interface | Baud Rate |
|---|---|---|---|
| Measurement (ESP32) | `/dev/ttyUSB0` | UART0 (USB-UART bridge) | 115200 |
| UI (ESP32-S3) | `/dev/ttyACM0` | USB-JTAG (native USB) | 115200 |

#### Expected Boot Sequence (Measurement MCU)

```
I (325) cpu_start: Starting scheduler on PRO CPU.
I (325) cpu_start: Starting scheduler on APP CPU.
I (340) PHOENIX: Phoenix Measurement MCU v108.0 "Chimera"
I (345) SAFETY: Power-On Self Test starting...
I (350) SAFETY: RAM test: PASS
I (355) SAFETY: Flash test: PASS
I (380) SAFETY: Camera test: PASS
I (410) SAFETY: LED test: PASS
I (450) SAFETY: UART test: PASS
I (450) SAFETY: POST complete — overall health: 100.0%
I (455) IPC: UART1 initialized (921600 baud, TX=17, RX=16)
```

#### Log Level Prefixes

| Prefix | Level | Color (ANSI) |
|---|---|---|
| `T (...)` | TRACE | Gray |
| `D (...)` | DEBUG | Cyan |
| `I (...)` | INFO | Green |
| `W (...)` | WARN | Yellow |
| `E (...)` | ERROR | Red |
| `[FATAL]` | FATAL | Bold Red |

#### Dual Monitor Setup

To monitor both MCUs simultaneously in split terminals:

```bash
# Terminal 1 (left pane)
idf.py -p /dev/ttyUSB0 monitor

# Terminal 2 (right pane)
idf.py -p /dev/ttyACM0 monitor
```

Or with tmux:
```bash
tmux new-session -d -s phoenix \
    "idf.py -p /dev/ttyUSB0 monitor" \; \
    split-window -h \
    "idf.py -p /dev/ttyACM0 monitor" \; \
    attach
```

### JTAG Debugging

#### Hardware Connection

The ESP32-S3 UI MCU supports JTAG via its native USB interface (no external adapter needed).

The ESP32 Measurement MCU requires an external JTAG adapter:

| JTAG Pin | ESP32 GPIO | Adapter Pin |
|---|---|---|
| TDI | GPIO 12 | TDI |
| TDO | GPIO 15 | TDO |
| TCK | GPIO 13 | TCK |
| TMS | GPIO 14 | TMS |
| GND | GND | GND |

#### OpenOCD Configuration

```bash
# ESP32 Measurement MCU (with external adapter)
openocd -f interface/ftdi/esp32_devkitj_v1.cfg -f target/esp32.cfg

# ESP32-S3 UI MCU (built-in USB-JTAG)
openocd -f board/esp32s3-builtin.cfg
```

#### GDB Session

```bash
# Connect GDB to OpenOCD
xtensa-esp32-elf-gdb -x gdbinit build/phoenix-measurement.elf

# gdbinit contents:
# target remote :3333
# monitor reset halt
# thb app_main
# continue
```

#### Common Debug Commands

```gdb
# Set breakpoint in measurement pipeline
break MeasurementPipeline::runMeasurement
# Step through
next
# Print variables
print result.concentration_ng_ml
# View FreeRTOS task list
monitor esp32 tasks
# View memory usage
monitor esp32 heap info
```

### Wokwi Simulation

#### Starting a Simulation

1. Open the `wokwi-test/` directory in VS Code
2. Press **F1** > **"Wokwi: Start Simulator"**
3. The simulator opens in a new panel with the ESP32 virtual board
4. Serial output appears in the terminal

#### diagram.json

The `diagram.json` file defines the virtual hardware. The Wokwi simulation uses a single ESP32 (not dual-MCU) with test stubs.

#### Breakpoints in Simulation

1. Set breakpoints in VS Code by clicking the gutter
2. The Wokwi simulator supports GDB debugging
3. Press **F1** > **"Wokwi: Start Simulator and Debug"**
4. Use the VS Code debug panel for stepping, variable inspection

#### Limitations

- Single ESP32 only (no dual-MCU UART IPC simulation)
- No real camera hardware — uses test patterns
- No real LED output — simulated via GPIO state
- Performance is approximate — not cycle-accurate

---

## Git Workflow

### Branch Naming Convention

```
feature/<ticket-id>-<short-description>    # New features
bugfix/<ticket-id>-<short-description>     # Bug fixes
release/v<major>.<minor>                    # Release branches
hotfix/v<major>.<minor>.<patch>             # Emergency fixes
```

Examples:
```
feature/PHX-123-add-dengue-assay
bugfix/PHX-456-fix-peak-detection-overflow
release/v109.0
hotfix/v108.0.1
```

### Commit Message Format (Conventional Commits)

```
<type>(<scope>): <subject>

<body>

<footer>
```

#### Types

| Type | Description |
|---|---|
| `feat` | New feature |
| `fix` | Bug fix |
| `docs` | Documentation only |
| `test` | Adding or updating tests |
| `refactor` | Code change that neither fixes a bug nor adds a feature |
| `perf` | Performance improvement |
| `chore` | Build system, CI, or other non-code changes |

#### Scopes

| Scope | Area |
|---|---|
| `cal` | Calibration (5PL, Color Chart, factory) |
| `meas` | Measurement pipeline |
| `ipc` | UART bridge, proxy |
| `ui` | LVGL screens, UI logic |
| `hal` | Camera, LED hardware abstraction |
| `core` | Result, FixedString, Logger, ErrorRegistry |
| `safety` | SafetyManager, POST, watchdog |
| `ci` | CI/CD pipeline |

#### Examples

```
feat(cal): add field recalibration with Color Chart DXR.007.01

fix(meas): prevent peak detection overflow when profile length > 350

test(core): add ring-buffer wrap-around test for Logger

docs(runbook): add manufacturing QC procedure
```

### PR Review Checklist

Before requesting review, ensure:

- [ ] **Code compiles** for both targets: `idf.py build` passes for phoenix-measurement AND phoenix-ui
- [ ] **Host tests pass**: all 115 suites, 813 assertions
- [ ] **No new warnings**: build with `-Wall -Wextra -Werror`
- [ ] **No heap allocations** added in measurement-critical paths
- [ ] **Result<T> used** for all fallible operations (no raw error codes)
- [ ] **PHOENIX_LOG macros used** (not raw `ESP_LOGx` or `printf`)
- [ ] **Error codes added** to `ErrorRegistry.h` for new error conditions
- [ ] **REQ-xxx-nnn tags** added for new requirements
- [ ] **Tests added** for new functionality (aim for >90% branch coverage)
- [ ] **Documentation updated** if public API changed
- [ ] **Conventional commit messages** used
- [ ] **No secrets** committed (.env, credentials, API keys)

---

## CI/CD Pipeline

### GitHub Actions Workflow

The following workflow runs on every push and pull request. Create this file at `.github/workflows/ci.yml`:

```yaml
name: Phoenix CI

on:
  push:
    branches: [main, develop, "release/**"]
  pull_request:
    branches: [main, develop]

jobs:
  host-tests:
    name: Host Unit Tests
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4

      - name: Install g++ 13
        run: |
          sudo apt-get update
          sudo apt-get install -y g++-13
          sudo update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-13 100

      - name: Build tests
        run: |
          cd tests
          cmake -B build -DCMAKE_CXX_COMPILER=g++-13
          cmake --build build

      - name: Run tests
        run: |
          cd tests
          ./build/test_runner 2>&1 | tee test_output.log
          # Verify all tests pass
          grep -q "ALL .* TESTS PASSED" test_output.log

      - name: Upload test results
        if: always()
        uses: actions/upload-artifact@v4
        with:
          name: test-results
          path: tests/test_output.log

  build-measurement:
    name: Build Measurement MCU
    runs-on: ubuntu-latest
    container: espressif/idf:v5.3
    steps:
      - uses: actions/checkout@v4

      - name: Build phoenix-measurement
        run: |
          cd phoenix-measurement
          idf.py set-target esp32
          idf.py build

      - name: Upload firmware
        uses: actions/upload-artifact@v4
        with:
          name: phoenix-measurement-firmware
          path: |
            phoenix-measurement/build/bootloader/bootloader.bin
            phoenix-measurement/build/partition_table/partition-table.bin
            phoenix-measurement/build/phoenix-measurement.bin

  build-ui:
    name: Build UI MCU
    runs-on: ubuntu-latest
    container: espressif/idf:v5.3
    steps:
      - uses: actions/checkout@v4

      - name: Build phoenix-ui
        run: |
          cd phoenix-ui
          idf.py set-target esp32s3
          idf.py build

      - name: Upload firmware
        uses: actions/upload-artifact@v4
        with:
          name: phoenix-ui-firmware
          path: |
            phoenix-ui/build/bootloader/bootloader.bin
            phoenix-ui/build/partition_table/partition-table.bin
            phoenix-ui/build/phoenix-ui.bin

  build-wokwi:
    name: Build Wokwi Simulation
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4

      - name: Setup PlatformIO
        uses: actions/cache@v4
        with:
          path: ~/.platformio
          key: pio-${{ hashFiles('wokwi-test/platformio.ini') }}

      - name: Install PlatformIO
        run: pip install platformio

      - name: Build wokwi-test
        run: |
          cd wokwi-test
          pio run
```

### Pipeline Stages

```
Push / PR
    │
    ├── host-tests         (2 min)   ── g++ build + 115 test suites
    │
    ├── build-measurement  (5 min)   ── ESP-IDF esp32 build
    │
    ├── build-ui           (5 min)   ── ESP-IDF esp32s3 build
    │
    └── build-wokwi        (3 min)   ── PlatformIO Arduino build
```

All four jobs run in parallel. PR merge requires all jobs to pass.

### Local Pre-Commit Check

Run before pushing:

```bash
# Quick check: host tests only (30 seconds)
cd tests && cmake -B build && cmake --build build && ./build/test_runner

# Full check: both firmware builds + tests (10 minutes)
cd phoenix-measurement && idf.py build && cd ..
cd phoenix-ui && idf.py build && cd ..
cd tests && cmake -B build && cmake --build build && ./build/test_runner
```

---

## Project Configuration Reference

### ESP32 Measurement MCU (`sdkconfig.defaults`)

| Setting | Value | Rationale |
|---|---|---|
| `CONFIG_IDF_TARGET` | `esp32` | ESP32-WROOM-32D |
| `CONFIG_ESPTOOLPY_FLASHSIZE_4MB` | `y` | 4MB flash |
| `CONFIG_SPIRAM` | `y` | 4MB PSRAM for image buffer |
| `CONFIG_ESP32_DEFAULT_CPU_FREQ_240` | `y` | Max CPU for image processing |
| `CONFIG_FREERTOS_HZ` | `1000` | 1ms tick for timing accuracy |
| `CONFIG_ESP_TASK_WDT_TIMEOUT_S` | `10` | Watchdog timeout |
| `CONFIG_HEAP_POISONING_COMPREHENSIVE` | `y` | Debug: detect heap corruption |
| `CONFIG_FREERTOS_CHECK_STACKOVERFLOW_CANARY` | `y` | Debug: detect stack overflow |

### ESP32-S3 UI MCU (`sdkconfig.defaults`)

| Setting | Value | Rationale |
|---|---|---|
| `CONFIG_IDF_TARGET` | `esp32s3` | ESP32-S3 with USB-JTAG |
| `CONFIG_ESPTOOLPY_FLASHSIZE_16MB` | `y` | 16MB flash (fonts, UI assets) |
| `CONFIG_SPIRAM_MODE_OCT` | `y` | 8MB Octal PSRAM |
| `CONFIG_LV_MEM_SIZE_KILOBYTES` | `64` | LVGL memory pool |
| `CONFIG_LV_COLOR_DEPTH_16` | `y` | 16-bit color |
| `CONFIG_LV_DISP_DEF_REFR_PERIOD` | `16` | 60 FPS refresh |
| `CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG` | `y` | Debug via native USB |

---

## Useful Commands Quick Reference

```bash
# ── Build ────────────────────────────────────
idf.py build                           # Build current project
idf.py fullclean                       # Clean all build artifacts
idf.py menuconfig                      # Interactive SDK configuration
idf.py size                            # Show firmware size breakdown
idf.py size-components                 # Show size per component

# ── Flash ────────────────────────────────────
idf.py flash                           # Flash default port
idf.py -p /dev/ttyUSB0 flash           # Flash specific port
idf.py -p /dev/ttyUSB0 flash monitor   # Flash and open monitor

# ── Monitor ──────────────────────────────────
idf.py monitor                         # Open serial monitor
# Ctrl+]  → Exit monitor
# Ctrl+T Ctrl+H → Help
# Ctrl+T Ctrl+R → Reset device

# ── NVS ──────────────────────────────────────
idf.py -p PORT erase-flash             # Full flash erase (loses all NVS)
idf.py -p PORT erase-otadata           # Reset OTA boot partition

# ── Test ─────────────────────────────────────
cd tests && cmake -B build && cmake --build build && ./build/test_runner

# ── PlatformIO ───────────────────────────────
pio run                                # Build
pio run -t upload                      # Upload to board/Wokwi
pio run -t clean                       # Clean build
pio device monitor                     # Serial monitor

# ── Git ──────────────────────────────────────
git log --oneline -20                  # Recent commits
git diff --stat HEAD~1                 # Files changed in last commit
```
