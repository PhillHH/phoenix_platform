# Field Service Runbook — Igloo Pro Analyzer

## Scope

This runbook covers all field-serviceable operations for deployed Igloo Pro Analyzers: firmware updates (OTA and USB), field recalibration, diagnostics export, component replacement, and preventive maintenance.

**Target audience:** Field service technicians
**Required training:** Basic electronics, serial terminal usage, WiFi configuration

---

## OTA Firmware Update (WiFi)

### Prerequisites

- Device connected to WiFi with internet access
- Firmware update package available on the update server
- Device battery > 50% or connected to USB power

### Procedure

#### 1. Verify Current Version

Navigate to **System Menu > About** on the Igloo Pro display.

```
Phoenix Platform v108.0 "Chimera"
Measurement MCU: v108.0
UI MCU: v108.0
Serial: IGLOO-2026-0042
```

Note the current versions before updating.

#### 2. Check WiFi Connectivity

Navigate to **Settings > WiFi**.
- Status should show **"Connected"** with signal strength indicator
- If not connected, enter WiFi credentials for the local network

#### 3. Start OTA Update

Navigate to **System Menu > Firmware Update > Check for Updates**.

```
Current:  v108.0
Available: v109.0
Size: 1.2 MB (Measurement) + 1.8 MB (UI)
Changelog: Bug fixes, new assay support
```

Tap **"Download and Install"**.

#### 4. Update Progress

The update proceeds in stages:

```
Downloading Measurement MCU firmware...     [===========         ]  55%
Downloading UI MCU firmware...               [=======             ]  35%
Verifying checksums...                       [====================] 100%
Writing Measurement MCU...                   [===============     ]  75%
Writing UI MCU...                            [====================] 100%
Rebooting...
```

**IMPORTANT:**
- Do **NOT** disconnect power during the update
- Do **NOT** remove USB cables during the update
- The device will reboot automatically when complete
- The update takes approximately 2–5 minutes

#### 5. Verify Update

After reboot, check **System Menu > About** to confirm the new version.

Run POST via **System Menu > Diagnostics > Run POST** to verify hardware integrity.

#### 6. Post-Update Calibration

After a firmware update, the existing calibration data in NVS is preserved. However:
- If the update changes the image processing pipeline, recalibration is recommended
- Check the release notes for "recalibration required" notices
- If in doubt, perform a Color Chart calibration (see [Field Recalibration](#field-recalibration))

### OTA Troubleshooting

| Issue | Fix |
|---|---|
| "No updates available" | Check server URL in **Settings > Update Server**. Verify internet connectivity. |
| Download stuck at X% | Check WiFi signal strength. Move closer to access point. Retry. |
| "Checksum mismatch" | Corrupted download. Retry. If persists, check network for packet corruption. |
| Device won't boot after update | Connect via USB and flash manually (see next section). |
| Update server unreachable | Use USB update as fallback. |

---

## USB Firmware Update (Fallback)

Use this method when WiFi is unavailable or OTA has failed.

### Required Tools

- USB-C cable
- PC with `esptool.py` installed (see [Development Runbook](DEVELOPMENT.md))
- Firmware binary files (on USB drive or downloaded from internal server)

### Procedure

#### 1. Download Firmware Binaries

Obtain the firmware package from the build server or USB distribution media:

```
phoenix-v109.0/
├── phoenix-measurement/
│   ├── bootloader.bin
│   ├── partition-table.bin
│   ├── ota_data_initial.bin
│   └── phoenix-measurement.bin
└── phoenix-ui/
    ├── bootloader.bin
    ├── partition-table.bin
    ├── ota_data_initial.bin
    └── phoenix-ui.bin
```

#### 2. Flash Measurement MCU

Connect USB-C to the **left** port (MEAS).

```bash
esptool.py --port /dev/ttyUSB0 \
    --baud 921600 \
    --chip esp32 \
    write_flash \
    --flash_mode qio \
    --flash_freq 80m \
    --flash_size 4MB \
    0x10000 phoenix-measurement/phoenix-measurement.bin
```

> Note: Only the application partition (0x10000) needs to be reflashed for updates.
> The bootloader and partition table only need updating for major releases.

#### 3. Flash UI MCU

Connect USB-C to the **right** port (UI).

```bash
esptool.py --port /dev/ttyACM0 \
    --baud 921600 \
    --chip esp32s3 \
    write_flash \
    --flash_mode qio \
    --flash_freq 80m \
    --flash_size 16MB \
    0x10000 phoenix-ui/phoenix-ui.bin
```

#### 4. Verify and Reboot

```bash
esptool.py --port /dev/ttyUSB0 verify_flash 0x10000 phoenix-measurement/phoenix-measurement.bin
esptool.py --port /dev/ttyACM0 verify_flash 0x10000 phoenix-ui/phoenix-ui.bin
```

Power cycle the device and verify versions via **System Menu > About**.

---

## Field Recalibration

<a id="field-recalibration"></a>

### When is Recalibration Needed?

| Trigger | Action |
|---|---|
| Calibration expired (>24 hours) | Recalibrate before next measurement |
| After firmware update (if release notes require it) | Recalibrate |
| QC measurement outside tolerance | Recalibrate, then re-run QC |
| Device moved to different environment | Recalibrate (temperature, humidity change) |
| After component replacement (camera, LED) | Mandatory recalibration |
| Scheduled preventive maintenance | Recalibrate as part of PM |

### Recalibration Procedure

#### 1. Prepare Color Chart

- Use a lot-verified Color Chart plate (DXR.007.01)
- Inspect for physical damage — no scratches, discoloration, or bent corners
- Record the plate lot number

#### 2. Insert and Calibrate

1. Open the cassette drawer
2. Insert the Color Chart with **Strip 0 (Black) at the top**
3. Align the registration hole with the locating pin
4. Close the drawer until it clicks
5. Navigate to **Settings > Calibration > Color Chart**
6. Tap **"Start Calibration"**

#### 3. Verify Results

Check the calibration summary on screen:

| Parameter | Acceptable | Optimal |
|---|---|---|
| All 11 strips valid | Yes (mandatory) | Yes |
| Linearity R² | > 0.95 | > 0.99 |
| Dark drift | < 5% | < 2% |
| White drift | < 5% | < 2% |

If calibration fails, see [Troubleshooting: Calibration](TROUBLESHOOTING.md#calibration-failure).

#### 4. Verification Measurement (Optional but Recommended)

Run a measurement with a known reference cassette to confirm:

| Assay | Reference Concentration | Acceptable Range |
|---|---|---|
| CRP | 25.0 mg/L | 21.25 – 28.75 mg/L (±15%) |
| HbA1c | 6.5% | 5.85 – 7.15% (±10%) |

#### 5. Document

Record in the service log:
- Date and time of recalibration
- Color Chart lot number
- R², drift values
- Verification measurement result (if performed)
- Technician name

---

## Diagnostics Export

### Method 1: Via WiFi (Preferred)

1. Navigate to **System Menu > Diagnostics > Export Full Report**
2. The report is uploaded to the configured LIMS server
3. Alternatively, select **"Save to Local"** to store on the device for USB retrieval

### Method 2: Via Serial Monitor (USB)

Connect a USB cable to the Measurement MCU (left port) and open a serial terminal:

```bash
idf.py -p /dev/ttyUSB0 monitor | tee diagnostics_IGLOO-2026-0042.log
```

Then trigger diagnostics from the UI: **System Menu > Diagnostics > Run Full Report**

The output includes:

```
═══════════════════════════════════════
  Phoenix Diagnostics Report
  Serial:  IGLOO-2026-0042
  FW:      v108.0
  Date:    2026-02-27 14:30:22
═══════════════════════════════════════

── Hardware Status ──
  Heap free:    245.3 kB
  Heap min:     198.7 kB
  CPU temp:     38.2°C
  Battery:      5.05 V
  PSRAM:        OK
  Flash:        OK
  Camera:       OK (chip_id=0x2686)
  LED:          OK
  UART IPC:     OK (latency 12ms)
  Overall:      100.0%
  Uptime:       3847s

── Calibration Status ──
  Active cal:   CC-DXR007-LOT23
  Cal R²:       0.9973
  Cal age:      2h 15m
  Cal expires:  in 21h 45m
  Factory cals: 8 loaded

── Error Registry (2 entries) ──
  [0] 0x0400 LOW  COMM  "UART CRC mismatch" x3 (last: 20min ago)
  [1] 0x0210 LOW  CAL   "Calibration expired" x1 (last: 60min ago)

── Logger (last 10 entries) ──
  14:30:01 INFO  PIPE  "Measurement completed: CRP = 24.8 mg/L"
  14:29:50 INFO  PIPE  "QC validation: PASS"
  14:29:48 INFO  PIPE  "Peak detection: 2 peaks found"
  14:29:45 INFO  CAM   "Image captured (640x480, exp=128)"
  14:29:44 INFO  LED   "White LED on (intensity=200)"
  ...

═══════════════════════════════════════
```

### Method 3: Error Log Only (Quick)

To export only the error registry without the full diagnostics:

**Via UI:** **System Menu > Diagnostics > Error Log**

**Via serial:**
```
# The error registry entries are displayed in the serial output
# Look for the "Error Registry" section
```

### Saving Diagnostics Files

Save the output to a file for support:

```bash
# Capture entire serial output to file
idf.py -p /dev/ttyUSB0 monitor | tee igloo_diag_$(date +%Y%m%d_%H%M%S).log

# Or capture just the last N lines
idf.py -p /dev/ttyUSB0 monitor > /tmp/diag.log &
# ... trigger diagnostics ...
kill %1
```

---

## Component Replacement

### Field-Replaceable Parts

| Component | Part Number | Replacement Difficulty | Recalibration Required? |
|---|---|---|---|
| Color Chart plate | DXR.007.01 | Easy (user-serviceable) | No (it IS the calibration) |
| LFA Cassettes | Assay-specific | Easy (user-serviceable) | No |
| USB-C cables | Standard | Easy | No |
| Camera FPC cable | DXR-FPC-CAM-01 | Medium (open enclosure) | **Yes** |
| LED board assembly | DXR-LED-ASM-01 | Medium (open enclosure) | **Yes** |
| Display FPC cable | DXR-FPC-LCD-01 | Medium (open enclosure) | No |

### Non-Field-Replaceable Parts

These require return to factory:

| Component | Reason |
|---|---|
| ESP32 Measurement MCU | Soldered on main PCB; requires re-flash + full recalibration |
| ESP32-S3 UI MCU | Soldered on main PCB; requires re-flash |
| OV2686 Camera module | Requires optical alignment after replacement |
| Display panel (ST7701S) | Requires mechanical alignment |
| Main PCB | Core assembly; not cost-effective to repair |

### Camera FPC Cable Replacement

1. Power off the device
2. Remove the 4 bottom screws (Torx T6)
3. Open the enclosure carefully — the FPC cables run between halves
4. Disconnect the old camera FPC from both the camera module and the main PCB ZIF connectors
5. Insert the new FPC: contacts facing down on both connectors
6. Close the ZIF locks
7. Reassemble enclosure
8. Power on and run POST
9. **Mandatory: Perform Color Chart recalibration**

### LED Board Replacement

1. Power off the device
2. Remove the 4 bottom screws (Torx T6)
3. Disconnect the LED board FPC cable from the main PCB
4. Remove the 2 LED board mounting screws (Phillips #0)
5. Install new LED board and connect FPC
6. Reassemble enclosure
7. Power on and run POST — verify `testLED()` passes
8. **Mandatory: Perform Color Chart recalibration**

---

## Preventive Maintenance Schedule

### Recommended Intervals

| Task | Interval | Description |
|---|---|---|
| Color Chart recalibration | Every 24 hours (automatic prompt) | System prompts when calibration expires |
| QC measurement | Weekly | Run CRP reference at 25 mg/L |
| Full diagnostics export | Monthly | Export and review error log |
| Camera lens cleaning | Quarterly | Inspect and clean with lens cloth |
| Color Chart plate replacement | After 50 uses or visible wear | Check for scratches and discoloration |
| Firmware update check | Monthly | Check for available updates |
| Full POST | After any service action | Verify all subsystems operational |

### Lens Cleaning Procedure

1. Power off the device
2. Open the cassette drawer fully
3. Use a dry lens cleaning cloth (microfiber)
4. Gently wipe the camera lens window (visible inside the reader bay)
5. Do **NOT** use liquids, compressed air, or abrasive cloths
6. Close the drawer
7. Power on and verify camera works (run a test capture)

### Environmental Requirements

| Parameter | Operating Range | Storage Range |
|---|---|---|
| Temperature | 15–35 °C | 5–45 °C |
| Humidity | 20–80% RH (non-condensing) | 10–90% RH (non-condensing) |
| Altitude | 0–3000 m | 0–12000 m |

Operating outside these ranges may trigger safety warnings:
- `SAFE_OVER_TEMPERATURE (0x0511)` if CPU > 85°C
- `SAFE_VOLTAGE_LOW (0x0510)` if supply < 3.0V

---

## Service Log Template

| Field | Value |
|---|---|
| Device Serial | IGLOO-2026-____ |
| Service Date | __________ |
| Service Type | [ ] OTA Update / [ ] USB Update / [ ] Recalibration / [ ] Component Replacement / [ ] PM |
| FW Before | v____.__ |
| FW After | v____.__ |
| Calibration Performed | [ ] Yes / [ ] No |
| Cal R² | ______ |
| Cal Chart Lot | CC-DXR007-LOT__ |
| QC Measurement | [ ] Performed / [ ] Skipped |
| QC Result | ____ mg/L (expected: ____ mg/L) |
| QC Pass/Fail | [ ] PASS / [ ] FAIL |
| POST Result | [ ] PASS / [ ] FAIL |
| Error Count (pre-service) | ____ |
| Error Count (post-service) | ____ |
| Parts Replaced | __________ |
| Technician | __________ |
| Notes | __________ |
