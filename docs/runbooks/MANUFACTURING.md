# Manufacturing Runbook — Igloo Pro Analyzer

## Scope

This runbook covers the complete production process for a single Igloo Pro Analyzer unit: flashing both ESP32 modules, verifying inter-processor communication, executing Power-On Self Test (POST), performing factory calibration with the reference Color Chart, and running the final QC measurement.

**Target audience:** Production technicians
**Estimated time per unit:** 25–35 minutes
**IEC 62304 reference:** REQ-SAFE-001 (POST), REQ-CAL-001 (Calibration)

---

## Required Tools and Materials

### Software

| Tool | Version | Purpose |
|---|---|---|
| `esptool.py` | 4.7+ | Flash firmware to ESP32 |
| ESP-IDF | v5.3 | Build framework (pre-built binaries provided) |
| Serial terminal | Any (minicom, PuTTY, idf.py monitor) | Verify boot output |

### Hardware

| Item | Quantity | Notes |
|---|---|---|
| Igloo Pro Analyzer (assembled) | 1 | Both ESP32 modules mounted on PCB |
| USB-C cable | 2 | One per ESP32 (simultaneous flash possible) |
| Reference Color Chart DXR.007.01 | 1 | 11-strip calibration plate, lot-verified |
| CRP Reference Cassette (25 mg/L) | 1 | Known concentration for QC check |
| Test PC | 1 | Linux/macOS/Windows with Python 3.8+ |
| Serial number label | 1 | Pre-printed for this unit |

### Pre-Built Firmware Binaries

Location: `//build-server/phoenix/v108.0/release/`

```
phoenix-measurement/
  ├── bootloader.bin          (0x0000)
  ├── partition-table.bin     (0x8000)
  ├── phoenix-measurement.bin (0x10000)
  └── ota_data_initial.bin    (0xD000)

phoenix-ui/
  ├── bootloader.bin          (0x0000)
  ├── partition-table.bin     (0x8000)
  ├── phoenix-ui.bin          (0x10000)
  └── ota_data_initial.bin    (0xD000)
```

---

## Step 1: Flash ESP32 #2 (Measurement MCU)

The Measurement MCU is the **lower** ESP32 module on the main PCB (labeled "MEAS" on the silkscreen). It connects via the **left** USB-C port.

### 1.1 Connect

1. Connect USB-C cable from PC to the **left** USB-C port (MEAS)
2. The device should appear as `/dev/ttyUSB0` (Linux), `/dev/cu.usbserial-*` (macOS), or `COM3` (Windows)
3. Verify connection:

```bash
esptool.py --port /dev/ttyUSB0 chip_id
```

**Expected output:**
```
Chip is ESP32-D0WD-V3 (revision v3.1)
Features: WiFi, BT, Dual Core, 240MHz, VRef calibration in efuse, Coding Scheme None
Crystal is 40MHz
```

### 1.2 Erase Flash (new units only)

```bash
esptool.py --port /dev/ttyUSB0 --baud 921600 erase_flash
```

### 1.3 Flash All Partitions

```bash
esptool.py --port /dev/ttyUSB0 \
    --baud 921600 \
    --chip esp32 \
    write_flash \
    --flash_mode qio \
    --flash_freq 80m \
    --flash_size 4MB \
    0x0000  phoenix-measurement/bootloader.bin \
    0x8000  phoenix-measurement/partition-table.bin \
    0xD000  phoenix-measurement/ota_data_initial.bin \
    0x10000 phoenix-measurement/phoenix-measurement.bin
```

**Expected output:**
```
Wrote 26480 bytes at 0x00000000  ... (100 %)
Wrote 3072 bytes at 0x00008000  ... (100 %)
Wrote 8192 bytes at 0x0000D000  ... (100 %)
Wrote 831472 bytes at 0x00010000 ... (100 %)
Hash of data verified.

Leaving...
Hard resetting via RTS pin...
```

### 1.4 Verify Flash

```bash
esptool.py --port /dev/ttyUSB0 verify_flash \
    0x10000 phoenix-measurement/phoenix-measurement.bin
```

**Expected:** `Verified (digest matched)`

> **FAIL?** See [Troubleshooting: Flash Failure](TROUBLESHOOTING.md#flash-failure)

---

## Step 2: Flash ESP32 #1 (UI MCU)

The UI MCU is the **upper** ESP32-S3 module (labeled "UI" on the silkscreen). It connects via the **right** USB-C port (USB-JTAG).

### 2.1 Connect

1. Connect USB-C cable from PC to the **right** USB-C port (UI)
2. The device appears as `/dev/ttyACM0` (Linux) or `COM4` (Windows) — note: USB-JTAG, not UART
3. Verify connection:

```bash
esptool.py --port /dev/ttyACM0 chip_id
```

**Expected output:**
```
Chip is ESP32-S3 (revision v0.2)
Features: WiFi, BLE, Embedded PSRAM 8MB (Octal)
Crystal is 40MHz
```

### 2.2 Erase Flash (new units only)

```bash
esptool.py --port /dev/ttyACM0 --baud 921600 erase_flash
```

### 2.3 Flash All Partitions

```bash
esptool.py --port /dev/ttyACM0 \
    --baud 921600 \
    --chip esp32s3 \
    write_flash \
    --flash_mode qio \
    --flash_freq 80m \
    --flash_size 16MB \
    0x0000  phoenix-ui/bootloader.bin \
    0x8000  phoenix-ui/partition-table.bin \
    0xD000  phoenix-ui/ota_data_initial.bin \
    0x10000 phoenix-ui/phoenix-ui.bin
```

### 2.4 Verify Flash

```bash
esptool.py --port /dev/ttyACM0 verify_flash \
    0x10000 phoenix-ui/phoenix-ui.bin
```

**Expected:** `Verified (digest matched)`

> **FAIL?** See [Troubleshooting: Flash Failure](TROUBLESHOOTING.md#flash-failure)

---

## Step 3: Verify UART Connection

After flashing both MCUs, verify the inter-processor UART link.

### 3.1 Open Serial Monitors

Open **two** terminal windows:

```bash
# Terminal 1: Measurement MCU (UART0 debug)
idf.py -p /dev/ttyUSB0 monitor
# or: minicom -D /dev/ttyUSB0 -b 115200

# Terminal 2: UI MCU (USB-JTAG)
idf.py -p /dev/ttyACM0 monitor
# or: minicom -D /dev/ttyACM0 -b 115200
```

### 3.2 Power Cycle

Disconnect and reconnect the USB cables (or press both RESET buttons simultaneously).

### 3.3 Expected Boot Output

**Measurement MCU (Terminal 1):**
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
I (460) IPC: Ping → UI MCU: OK (12ms)
```

**UI MCU (Terminal 2):**
```
I (280) cpu_start: Starting scheduler on PRO CPU.
I (280) cpu_start: Starting scheduler on APP CPU.
I (295) PHOENIX: Phoenix UI MCU v108.0 "Chimera"
I (300) LCD: ST7701S initialized (480x480, RGB)
I (310) TOUCH: GT911 detected (addr=0x5D, fw=0x1060)
I (315) IPC: UART1 initialized (921600 baud, TX=17, RX=16)
I (320) IPC: Pong ← Measurement MCU: OK
I (325) LVGL: Display driver registered
I (330) UI: Home screen loaded
```

### 3.4 Verification Checklist

| Check | Expected | Pass? |
|---|---|---|
| Measurement MCU boots without errors | No `E (...)` lines | [ ] |
| All 5 POST checks pass | `PASS` for RAM, Flash, Camera, LED, UART | [ ] |
| Overall health = 100% | `100.0%` | [ ] |
| UART Ping/Pong succeeds | `OK` with latency < 50ms | [ ] |
| UI MCU boots without errors | No `E (...)` lines | [ ] |
| LCD initialized | `ST7701S initialized` | [ ] |
| Touch controller detected | `GT911 detected` | [ ] |
| Home screen visible on LCD | Igloo Pro home screen displayed | [ ] |

> **FAIL?** See [Troubleshooting: UART Communication](TROUBLESHOOTING.md#uart-communication)

---

## Step 4: Power-On Self Test (POST)

POST is executed automatically at boot (Step 3). If all checks passed, proceed to Step 5.

If a retest is needed, send the diagnostics command from the UI MCU monitor:

```
CMD_RUN_DIAGNOSTICS (0x88)
```

Or trigger via the UI: **System Menu > Diagnostics > Run POST**

### POST Check Details

| # | Test | What it checks | Pass Criteria | Error Code |
|---|---|---|---|---|
| 1 | `testRAM()` | PSRAM write/read pattern (0xAA, 0x55) | All bytes match | `0x0502` |
| 2 | `testFlash()` | NVS partition read/write integrity | Read-back matches | `0x0140` |
| 3 | `testCamera()` | OV2686 chip ID via SCCB I2C | Chip ID = `0x2686` | `0x0100` |
| 4 | `testLED()` | White, UV, RGB channels self-test | All channels respond | `0x0110` |
| 5 | `testUART()` | Ping/Pong with UI MCU | Response within 500ms | `0x0402` |

### Diagnostics Report Fields

| Field | Expected Value | Alert Threshold |
|---|---|---|
| `heap_free_kb` | > 200 kB | < 32 kB |
| `cpu_temp_celsius` | 25–45 °C | > 85 °C |
| `battery_voltage_v` | 4.8–5.2 V (USB powered) | < 3.0 V |
| `overall_health` | 100.0% | < 80% |

> **FAIL?** See [Troubleshooting: POST Failure](TROUBLESHOOTING.md#post-failure)

---

## Step 5: Factory Calibration with Reference Color Chart

### 5.1 Prepare

1. Take a lot-verified Color Chart plate (DXR.007.01)
2. Verify the plate is undamaged — no scratches, no discoloration, no bent corners
3. Note the plate lot number on the production record

### 5.2 Insert Color Chart

1. Open the cassette drawer on the Igloo Pro
2. Insert the Color Chart plate with **Strip 0 (Black)** at the top
3. The plate registration hole should align with the locating pin
4. Close the drawer until it clicks

### 5.3 Trigger Calibration

**Via UI:**
1. Navigate to **Settings > Calibration > Color Chart**
2. Tap **"Start Calibration"**
3. The device reads all 11 strips (takes ~5 seconds)

**Via serial command (alternative):**
```
IPC: CMD_START_CALIBRATION (0x82) → "COLOR_CHART"
```

### 5.4 Expected Output

```
I (1200) CAL: Color Chart calibration started
I (1210) CAL: Strip  0 (Black):    raw=  12.3, valid=true
I (1220) CAL: Strip  1 (White):    raw= 243.7, valid=true
I (1230) CAL: Strip  2 (Dark Gold):raw=  94.5, valid=true
I (1240) CAL: Strip  3 (Magenta):  raw= 156.2, valid=true
I (1250) CAL: Strip  4 (Yellow):   raw= 221.8, valid=true
I (1260) CAL: Strip  5 (Black 2):  raw=  12.8, valid=true
I (1270) CAL: Strip  6 (White 2):  raw= 242.9, valid=true
I (1280) CAL: Strip  7 (Blue):     raw=  45.3, valid=true
I (1290) CAL: Strip  8 (Green):    raw= 178.4, valid=true
I (1300) CAL: Strip  9 (Red):      raw= 189.6, valid=true
I (1310) CAL: Strip 10 (White 3):  raw= 243.1, valid=true
I (1320) CAL: 11/11 strips read successfully
I (1325) CAL: Linearity R² = 0.9973
I (1326) CAL: Dark drift   = 0.4%
I (1327) CAL: White drift  = 0.3%
I (1330) CAL: Calibration PASSED — saved to NVS
```

### 5.5 Acceptance Criteria

| Check | Requirement | Production Limit |
|---|---|---|
| All 11 strips valid | `valid=true` for all | Mandatory |
| Linearity R² | > 0.95 | > 0.99 preferred |
| Dark drift | < 5% | < 2% preferred |
| White drift | < 5% | < 2% preferred |
| Calibration saved | `saved to NVS` | Mandatory |

### 5.6 Remove Color Chart

1. Open the drawer
2. Remove the Color Chart plate
3. Store the plate in its protective sleeve — reusable for up to 50 calibrations

> **FAIL?** See [Troubleshooting: Calibration Failure](TROUBLESHOOTING.md#calibration-failure)

---

## Step 6: Write Serial Number and Production Data

### 6.1 Write Serial Number to NVS

Use the `nvs_set` command via the Measurement MCU monitor. Format: `IGLOO-YYYY-NNNN`

```bash
# From Measurement MCU debug console:
nvs_set device_info serial_no str "IGLOO-2026-0042"
nvs_set device_info hw_version str "DA-R3.1"
nvs_set device_info fw_version str "v108.0"
nvs_set device_info mfg_date str "2026-02-27"
nvs_set device_info cal_lot str "CC-DXR007-LOT23"
nvs_commit device_info
```

### 6.2 Verify Written Data

```bash
nvs_get device_info serial_no str
# Expected: IGLOO-2026-0042
```

### 6.3 Affix Serial Number Label

1. Print and apply the serial number label to the bottom of the enclosure
2. The label must match the NVS serial number exactly

---

## Step 7: Final QC — Control Measurement

### 7.1 Insert Reference Cassette

1. Take a CRP reference cassette with known concentration (25 mg/L)
2. Apply 10 uL of reference sample to the cassette sample well
3. Wait for the specified incubation time (10 minutes for CRP)
4. Insert the cassette into the Igloo Pro drawer

### 7.2 Run Measurement

**Via UI:**
1. Navigate to **Home > New Measurement**
2. Select assay: **CRP**
3. Tap **"Start"**
4. Wait for the 10-stage pipeline to complete (~15 seconds after incubation)

### 7.3 Expected Results

| Parameter | Expected | Acceptable Range |
|---|---|---|
| Concentration | 25.0 mg/L | 21.25 – 28.75 mg/L (±15%) |
| QC Flags | `NONE` | No flags |
| Control Line SNR | > 5.0 | > 5.0 |
| Confidence | > 0.90 | > 0.85 |

### 7.4 Record Results

1. Record the measured concentration on the production sheet
2. Record the QC pass/fail status
3. Screenshot or export the result via **System Menu > Export Last Result**

### 7.5 Pass/Fail Decision

| Outcome | Action |
|---|---|
| Concentration within ±15% AND no QC flags | **PASS** — Unit approved for shipment |
| Concentration within ±20% OR minor QC flag | **CONDITIONAL** — Re-run measurement, recalibrate if needed |
| Concentration outside ±20% OR critical QC flag | **FAIL** — See Troubleshooting, escalate to engineering |

> **FAIL?** See [Troubleshooting: Measurement Failure](TROUBLESHOOTING.md#measurement-failure)

---

## Production Record Template

| Field | Value |
|---|---|
| Serial Number | IGLOO-2026-____ |
| Production Date | __________ |
| Firmware (Meas) | v108.0 |
| Firmware (UI) | v108.0 |
| POST Result | [ ] PASS / [ ] FAIL |
| POST Health % | ____% |
| Color Chart Lot | CC-DXR007-LOT__ |
| Cal R² | ______ |
| Cal Dark Drift | ____% |
| Cal White Drift | ____% |
| QC Assay | CRP |
| QC Reference | 25.0 mg/L |
| QC Measured | ____ mg/L |
| QC Deviation | ____% |
| QC Result | [ ] PASS / [ ] FAIL |
| Technician | __________ |
| Signature | __________ |

---

## Quick Reference: esptool.py Commands

```bash
# Check connection
esptool.py --port PORT chip_id

# Full erase (new unit only)
esptool.py --port PORT --baud 921600 erase_flash

# Flash measurement MCU
esptool.py --port /dev/ttyUSB0 --baud 921600 --chip esp32 write_flash \
    --flash_mode qio --flash_freq 80m --flash_size 4MB \
    0x0000 bootloader.bin 0x8000 partition-table.bin \
    0xD000 ota_data_initial.bin 0x10000 phoenix-measurement.bin

# Flash UI MCU
esptool.py --port /dev/ttyACM0 --baud 921600 --chip esp32s3 write_flash \
    --flash_mode qio --flash_freq 80m --flash_size 16MB \
    0x0000 bootloader.bin 0x8000 partition-table.bin \
    0xD000 ota_data_initial.bin 0x10000 phoenix-ui.bin

# Verify flash
esptool.py --port PORT verify_flash 0x10000 firmware.bin

# Read MAC address (unique device ID)
esptool.py --port PORT read_mac
```
