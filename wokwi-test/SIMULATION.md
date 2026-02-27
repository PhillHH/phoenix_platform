# Wokwi Simulation — Phoenix Platform v108.0

## Overview

The `wokwi-test/` subproject simulates the core Phoenix algorithms on a single ESP32 DevKit V1 inside the [Wokwi simulator](https://wokwi.com). The real Igloo Pro hardware uses two ESP32 modules, but the simulation runs all algorithm tests on one MCU with a UART loopback wire for IPC self-testing.

## Hardware Layout

```
                 ┌─────────────────────────────┐
                 │      Wokwi Virtual Board     │
                 │                               │
    ┌───┐ 220Ω ┌┤ GPIO 2  (PIN_LED_R)          │
    │RED├──/\/\/┤│                               │
    └─┬─┘      └┤                               │
      GND       │                               │
                 │                               │
    ┌───┐ 220Ω ┌┤ GPIO 14 (PIN_LED_G)          │
    │GRN├──/\/\/┤│                               │
    └─┬─┘      └┤                               │
      GND       │                               │
                 │                               │
    ┌───┐ 220Ω ┌┤ GPIO 13 (PIN_LED_B)          │
    │BLU├──/\/\/┤│                               │
    └─┬─┘      └┤                               │
      GND       │  ESP32 DevKit V1              │
                 │                               │
    ┌───┐ 220Ω ┌┤ GPIO 4  (PIN_LED_WHITE)      │
    │WHT├──/\/\/┤│                               │
    └─┬─┘      └┤                               │
      GND       │                               │
                 │  TX (GPIO 1) ──────┐          │
                 │  RX (GPIO 3) ◄─────┘ Loopback│
                 │                               │
    ┌───┐       ┌┤ GPIO 15 (TEST Button)        │
    │BTN├───────┤│                               │
    └─┬─┘      └┤                               │
      GND       └─────────────────────────────┘
```

### Pin Mapping

| GPIO | Component | Function | LEDC Channel |
|---|---|---|---|
| 2 | Red LED + 220R | Status: error/fail indicator | CH_R (channel 2) |
| 14 | Green LED + 220R | Status: pass/heartbeat indicator | CH_G (channel 3) |
| 13 | Blue LED + 220R | Status: blue indicator | CH_B (channel 4) |
| 4 | White LED + 220R | Simulates illumination LED | CH_WHITE (channel 1) |
| 1 (TX) | UART loopback | Connects TX→RX for IPC self-test | — |
| 3 (RX) | UART loopback | Receives loopback frames | — |
| 15 | Push button | Manual test trigger (active low) | — |

## What is Simulated

### 1. 5PL Calibration Mathematics (Test 2)
- Forward evaluation: concentration → signal using `FivePL_G::evaluate()`
- Inverse evaluation: signal → concentration using `FivePL_G::inverse()`
- Roundtrip verification at 8 concentration points (1, 5, 10, 25, 50, 100, 150, 200)
- Verified against real IgE assay 5PL coefficients from Dx365 MCP data
- Edge case handling: zero concentration, asymptote values, degenerate curves

### 2. Peak Detection on Synthetic Profiles (Test 3)
- Generates synthetic 1D line profiles (350 points, baseline ~165)
- Gaussian dips at control line (idx 47) and test lines (idx 138, 247)
- 7-point peak descriptor: leftExpect → leftLimit → leftPeak → center → rightPeak → rightLimit → rightExpect
- Peak metrics: height, value (area), integral, valid flag
- T/C ratio computation

### 3. IPC Protocol — UART Frame Encoding/Decoding (Test 4)
- CRC-16/CCITT computation against known test vectors
- Frame building: SYNC (0xAA55) + LEN + CMD + PAYLOAD + CRC
- Frame parsing with CRC validation
- Payload roundtrip: float, struct, multi-byte data
- Corruption detection: flipped CRC bit → parse rejection
- UART loopback wire enables hardware-level TX→RX frame testing

### 4. Full Measurement Pipeline (Test 5)
- End-to-end simulation: synthetic profile → peak detection → T/C ratio → 5PL inverse → concentration
- Four test cases: HIGH, MEDIUM, LOW, NEGATIVE concentration levels
- Uses verified IgE assay configuration with 3 lines (1 control + 2 test)

### 5. LED PWM Control (Test 6)
- LEDC PWM initialization (4 channels, 8-bit resolution, 5 kHz)
- Sequential color test: Red → Green → Blue → White → Off
- PWM duty cycle ramp on green LED (visible brightness change in Wokwi)
- Visual feedback: Yellow during testing, Green on pass, Red on fail

### 6. NVS Storage Persistence (Test 7)
- Open NVS partition (`phoenix_cal` namespace)
- Write 5PL calibration blob (20 bytes) and calibration date
- Read back and verify all parameters match (float precision check)
- Commit and close

### 7. Safety Checks / Heap Monitor (Test 8)
- Free heap measurement (must be > 32 kB)
- RAM alloc/free with pattern write/verify (simulates `SafetyManager::testRAM()`)
- Heap leak detection after alloc/free cycle
- Uptime reporting

### 8. Result<T> Error Handling Framework (Test 1)
- Ok and Err construction for `Result<int>` and `Result<void>`
- Error category and message propagation
- `value_or()` fallback behavior

## What is NOT Simulated

| Component | Reason | Where Tested Instead |
|---|---|---|
| OV2686 Camera | No Wokwi model for DVP cameras | Host unit tests (`test_peak_detection.cpp`) + real hardware |
| ST7701S Display / LVGL | Requires RGB LCD driver; separate `phoenix-ui` project | `phoenix-ui` on real hardware or ESP-IDF QEMU |
| GT911 Touch Controller | No Wokwi model for I2C touch | `phoenix-ui` on real hardware |
| WiFi / LIMS Connectivity | Network simulation not useful for algorithm verification | `phoenix-ui` integration test |
| Real Assay Measurements | Requires physical LFA cassette + camera | Field validation with reference cassettes |
| Dual-MCU UART IPC | Wokwi supports one ESP32; loopback approximates IPC | Real hardware with both MCUs |
| UV LED (365nm) | Same GPIO control as white LED; no spectral simulation | Real hardware |
| Color Chart Calibration | Requires camera image of physical plate | Real hardware + host tests |

## Expected Serial Monitor Output

A successful test run produces output similar to:

```
I (340) Phoenix: ══════════════════════════════════════════════════════════
I (345) Phoenix:  Phoenix v108.0 Chimera — Wokwi Integration Test
I (350) Phoenix:  Igloo Pro Medical Device Firmware
I (355) Phoenix:  ESP-IDF v4.4.7 on ESP32
I (360) Phoenix:  Free heap: 283.5 kB
I (365) Phoenix: ══════════════════════════════════════════════════════════

I (870) TEST: ═══ TEST 1: Result<T> Framework ═══
I (875) TEST:   PASS: Ok(42) is ok
I (880) TEST:   PASS: Ok(42) value == 42
...
I (900) TEST: ═══ TEST 2: 5PL Calibration Math ═══
I (905) TEST:   IgE 5PL forward: conc=[0,1,10,50,100,200]
I (910) TEST:   PASS: 5PL(0) near A (blank)
I (915) TEST:   PASS: 5PL roundtrip
...
I (1200) TEST: ═══ TEST 3: Peak Detection (Synthetic Profile) ═══
I (1205) TEST:   Profile: 350 points, baseline=165
I (1210) TEST:   PASS: detectPeaks succeeded
I (1215) TEST:   PASS: Found 3 peaks
...
I (1400) TEST: ═══ TEST 4: IPC Protocol (CRC16 + Frames) ═══
I (1405) TEST:   CRC16('123456789') = 0x29B1
I (1410) TEST:   PASS: CRC16 known vector '123456789' = 0x29B1
I (1415) TEST:   PASS: PING frame parse OK
...
I (1600) TEST: ═══ TEST 5: Measurement Pipeline (Profile → Concentration) ═══
I (1605) TEST:   --- HIGH conc (strong TL) (CL=130, TL=90) ---
I (1610) TEST:   PASS: Peak detection OK
...
I (1800) TEST: ═══ TEST 6: LED Control (watch the LEDs on Wokwi!) ═══
I (1805) TEST:   RED...
I (2305) TEST:   GREEN...
I (2805) TEST:   BLUE...
...
I (4000) TEST: ═══ TEST 7: NVS Storage (Calibration Persistence) ═══
I (4005) TEST:   PASS: NVS open 'phoenix_cal'
I (4010) TEST:   PASS: NVS write 5PL blob
...
I (4200) TEST: ═══ TEST 8: Safety Checks (Heap Monitor) ═══
I (4205) TEST:   Free heap: 271840 bytes (265.5 kB)
I (4210) TEST:   PASS: Heap > 32 kB (warning threshold)
...
I (4400) Phoenix: ══════════════════════════════════════════════════════════
I (4405) Phoenix:  TEST RESULTS: 68 PASSED, 0 FAILED (total: 68)
I (4410) Phoenix: ══════════════════════════════════════════════════════════
I (4415) Phoenix:  ALL TESTS PASSED — Phoenix core algorithms verified!
```

### LED Behavior During Test

| Phase | LED Color | Meaning |
|---|---|---|
| Boot / Testing | Yellow (R+G) | Tests running |
| RED flash | Red only | Individual red LED test |
| GREEN flash | Green only | Individual green LED test |
| BLUE flash | Blue only | Individual blue LED test |
| WHITE flash | White only | Illumination LED test |
| PWM ramp | Green fading | Duty cycle sweep 0→255→0 |
| Final: all pass | Green blinking (1 Hz) | All tests passed |
| Final: failures | Red blinking (2 Hz) | One or more tests failed |

## Running the Simulation

### VS Code + Wokwi Extension

1. Open the `wokwi-test/` directory in VS Code
2. Build the firmware:
   ```bash
   pio run
   ```
3. Press **F1** > **"Wokwi: Start Simulator"**
4. The simulator loads `diagram.json` and starts execution
5. Watch the serial monitor for test output
6. Watch the virtual LEDs for visual feedback

### Wokwi CLI

```bash
cd wokwi-test
pio run
wokwi-cli --firmware .pio/build/esp32dev/firmware.bin \
           --elf .pio/build/esp32dev/firmware.elf \
           --diagram diagram.json
```

### PlatformIO Only (No Simulation)

```bash
cd wokwi-test
pio run              # Build
pio run -t upload    # Flash to real ESP32 DevKit
pio device monitor   # Watch serial output
```

## Known Limitations

| Limitation | Impact | Workaround |
|---|---|---|
| Single ESP32 only | No real dual-MCU IPC testing | UART loopback + frame encode/decode tests |
| No DVP camera model | Cannot test image capture or ROI extraction | Synthetic profiles used; real camera tested on hardware |
| Arduino framework | Some ESP-IDF APIs may differ slightly | Core algorithms are framework-agnostic; tested identically on host (g++17) |
| No PSRAM in simulation | `BOARD_HAS_PSRAM=0` set in build flags | Tests use stack/heap allocation only; PSRAM tested on real hardware |
| Timing not cycle-accurate | FreeRTOS delays are approximate | Timing-critical tests use relative comparisons, not absolute timing |
| Wokwi LED model | No brightness proportional to PWM in all skins | PWM duty verified via serial log; LED on/off state is accurate |
| Push button not wired to test code | Button on GPIO 15 is present but no ISR registered | Future: add interrupt-driven test trigger |

## File Structure

```
wokwi-test/
├── platformio.ini          # PlatformIO build config (Arduino, ESP32, gnu++17)
├── wokwi.toml              # Wokwi firmware paths
├── diagram.json            # Virtual hardware: ESP32 + 4 LEDs + resistors + UART loopback + button
├── SIMULATION.md           # This file
├── shared/
│   └── phoenix_test_core.h # Single source of truth: all algorithms + 8 test suites
├── src/
│   └── main.cpp            # Arduino entry (5 lines, includes shared header)
└── main/
    └── main.cpp            # ESP-IDF entry (5 lines, includes shared header)
```
