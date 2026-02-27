# Troubleshooting Guide — Igloo Pro Analyzer

## How to Use This Guide

1. Find the **symptom** in the tables below
2. Follow the **Diagnosis** steps
3. Apply the **Fix**
4. If the fix does not resolve the issue, follow the **Escalation** path at the bottom of this document

---

## Hardware Issues

### LED / Illumination

| Symptom | Possible Cause | Diagnosis | Fix |
|---|---|---|---|
| White LED does not turn on | LED driver fault, GPIO misconfiguration | Check serial log for `HW_LED_FAILURE (0x0110)`. Measure voltage on GPIO 35. | Re-flash firmware. If voltage at GPIO 35 is 0V during LED-on, replace LED board. |
| White LED flickers | Loose FPC cable, PWM frequency issue | Inspect FPC connector. Check `LEDC Ch0` log output. | Reseat FPC cable. If persists, replace LED board. |
| UV LED does not turn on | UV LED driver fault, wrong LED mode | Check serial log for `HW_LED_FAILURE`. Verify `LEDMode::UV_365NM` is sent. | Re-flash firmware. Check UV LED power supply. |
| LED overcurrent warning | Short circuit or degraded LED | Check for `HW_LED_OVERCURRENT (0x0111)` in error log. | Replace LED board. Do not operate with overcurrent. |

### Camera

| Symptom | Possible Cause | Diagnosis | Fix |
|---|---|---|---|
| No image captured | Camera not initialized, DVP bus fault | Check for `HW_CAMERA_INIT_FAIL (0x0100)`. Run POST. | Re-flash firmware. Reseat camera FPC cable. Check I2C pull-ups on SDA/SCL. |
| Image is all black | Lens cap left on, LED not firing | Verify LED fires during capture. Check `captureImage()` log. | Remove obstruction. If LED works but image is black, replace camera module. |
| Image is all white / saturated | Exposure too high, LED too bright | Check `exposure` and `gain` values in log. | Recalibrate. Reduce exposure via `CMD_SET_EXPOSURE (0x85)`. |
| Image corrupt / garbled | DVP timing error, PSRAM fault | Check for `MEAS_IMAGE_CORRUPT (0x0320)`. Run RAM self-test. | Power cycle. If persists, check DVP data line connections. Replace PSRAM module. |
| Camera chip ID mismatch | Wrong camera module, I2C failure | Run POST — `testCamera()` should return chip ID `0x2686`. | Verify OV2686 module installed (not OV2640). Check SCCB wiring. |

### Display

| Symptom | Possible Cause | Diagnosis | Fix |
|---|---|---|---|
| Screen is black | LCD not initialized, backlight off | Check for `LCD: ST7701S initialized` in boot log. Measure GPIO 1 (backlight). | Check LCD FPC cable. Verify backlight PWM. Re-flash UI firmware. |
| Screen shows noise / wrong colors | RGB timing mismatch, wrong display driver | Check `sdkconfig` for correct `LCD_RGB` settings. | Verify ST7701S display module. Check PCLK frequency. |
| Partial display / shifted image | HSYNC/VSYNC misconfigured | Check GPIO 47 (HSYNC) and GPIO 48 (VSYNC) signals. | Re-flash with correct sdkconfig. Check soldering on display connector. |

### Touch

| Symptom | Possible Cause | Diagnosis | Fix |
|---|---|---|---|
| Touch not responding | GT911 not detected, I2C failure | Check for `GT911 detected` in boot log. Scan I2C bus at addr `0x5D`. | Check I2C SDA (GPIO 8) / SCL (GPIO 9) wiring. Check pull-ups. |
| Touch coordinates wrong | Calibration mismatch, wrong orientation | Log touch events and compare to screen position. | Recalibrate touch matrix. Check GT911 rotation setting. |
| Touch ghost events | EMI interference, faulty touch panel | Observe touch events without touching screen. | Shield FPC cable. Replace touch panel. |

---

## Communication Issues

### UART IPC

<a id="uart-communication"></a>

| Symptom | Possible Cause | Diagnosis | Fix |
|---|---|---|---|
| UART CRC errors | Noise on UART lines, baud mismatch | Check `COMM_UART_CRC_ERROR (0x0400)` count. Verify both MCUs use 921600 baud. | Reseat UART connectors. Shorten cable length. Add ferrite bead. |
| UART frame timeout | Peer MCU not responding, crashed | Check `COMM_UART_FRAME_TIMEOUT (0x0401)`. Verify peer MCU is running. | Power cycle. Re-flash peer MCU. Check TX/RX pin connections (TX=17, RX=16). |
| UART sync lost | Corrupted stream, buffer overflow | Check `COMM_UART_SYNC_LOST (0x0402)`. May follow a crash on peer side. | Power cycle both MCUs. If persists, reduce baud to 460800 for testing. |
| UART buffer overflow | Too many messages, slow consumer | Check `COMM_UART_OVERFLOW (0x0403)`. Monitor RX buffer usage. | Reduce message rate. Increase `rx_buf_size` in `UartConfig`. |
| Ping fails | UART hardware fault, TX/RX swapped | Send `PING (0xF0)` and check for `PONG (0xF1)`. | Verify TX→RX and RX→TX cross-connection between MCUs. |
| IPC NACK | Measurement MCU rejected command | Check `COMM_IPC_NACK (0x0430)` with error details. | Review command payload. Ensure MCU is in correct state (not mid-measurement). |

### WiFi

| Symptom | Possible Cause | Diagnosis | Fix |
|---|---|---|---|
| WiFi won't connect | Wrong SSID/password, signal too weak | Check `COMM_WIFI_DISCONNECT (0x0410)`. Verify WiFi credentials in NVS. | Re-enter WiFi credentials via **Settings > WiFi**. Move closer to AP. |
| WiFi connects but LIMS upload fails | Server unreachable, wrong endpoint | Check HTTP response code in log. Test endpoint with `curl`. | Verify LIMS server URL and API key in settings. |
| WiFi intermittent | Interference, memory pressure | Monitor `heap_free_kb`. Check for WiFi log warnings. | Reduce WiFi TX power. Free up memory. |

---

## Calibration Issues

<a id="calibration-failure"></a>

| Symptom | Possible Cause | Diagnosis | Fix |
|---|---|---|---|
| "Color Chart not detected" | Plate not inserted correctly, wrong orientation | Check log for strip readings. Verify plate orientation (Black strip at top). | Re-insert plate. Ensure registration hole aligned with pin. |
| Linearity R² too low (< 0.95) | Damaged Color Chart, contamination, camera issue | Check individual strip readings for outliers. Compare to expected values. | Replace Color Chart plate. Clean camera lens. Recalibrate. |
| Dark drift > 5% | Color Chart degradation, LED warm-up issue | Compare Black strip 0 vs strip 5 readings. | Wait 30s for LED warm-up. Replace Color Chart. |
| White drift > 5% | Color Chart degradation, LED instability | Compare White strips 1, 6, 10 readings. | Replace Color Chart. Check LED stability. |
| One strip "valid=false" | Damaged strip, misalignment | Inspect the specific strip for physical damage. | Replace Color Chart. If repeated, check camera alignment. |
| LM fit diverged | Insufficient calibration points, bad data | Check `CAL_FIT_DIVERGED (0x0200)`. Review data points. | Add more calibration points. Remove outliers. Check initial parameter guess. |
| R² too low after LM fit | Poor curve fit, wrong assay model | Check `CAL_R2_TOO_LOW (0x0201)`. Plot data vs fitted curve. | Verify correct assay is selected. Check for sample preparation errors. |
| Calibration expired | Time elapsed since last calibration | Check `CAL_EXPIRED (0x0210)` and `expires_at` timestamp. | Perform field recalibration with Color Chart. See [Field Service](FIELD_SERVICE.md#field-recalibration). |
| NVS save/load failure | NVS partition corrupt | Check `CAL_NVS_SAVE_FAIL (0x0220)` or `CAL_NVS_LOAD_FAIL (0x0221)`. | Erase NVS partition: `nvs_erase_all`. Re-flash and recalibrate. |

---

## Measurement Issues

<a id="measurement-failure"></a>

| Symptom | Possible Cause | Diagnosis | Fix |
|---|---|---|---|
| "No peak found" | Cassette not inserted, wrong assay, low sample | Check `MEAS_PEAK_NOT_FOUND (0x0301)`. View profile data in diagnostics. | Verify cassette inserted correctly. Ensure sufficient sample applied. |
| Control line weak | Expired cassette, insufficient sample flow | Check `MEAS_CONTROL_LINE_WEAK (0x0302)`. Check control line SNR value. | Use fresh cassette. Ensure correct sample volume (10 uL). |
| QC flag: HIGH_BACKGROUND | Dirty camera lens, light leak | Check background noise level in profile. | Clean camera lens. Ensure cassette drawer fully closed. |
| QC flag: LOW_SNR | Weak signal, low analyte concentration | Check signal-to-noise ratio in result. | Normal at very low concentrations. Report "below LOD" if SNR < 3. |
| QC flag: OUT_OF_RANGE | Concentration above or below reportable range | Check `MEAS_OUT_OF_RANGE (0x0311)`. Check assay range limits. | Dilute sample and re-measure. Report as "> upper limit" or "< LOD". |
| Result = 0 or unreasonable value | 5PL inverse boundary hit, degenerate curve | Check concentration value and 5PL parameters in log. | Recalibrate. If factory curves also fail, escalate to engineering. |
| Invalid profile | Image extraction failed | Check `MEAS_PROFILE_INVALID (0x0300)`. Check ROI detection. | Re-insert cassette. Ensure strip is centered in the reader. |
| ROI invalid | Strip not detected in image | Check `MEAS_ROI_INVALID (0x0321)`. View captured image thumbnail. | Re-insert cassette. Check camera focus. Check LED illumination. |

---

## System Issues

<a id="post-failure"></a>

### POST Failure

| POST Test | Error Code | Possible Cause | Fix |
|---|---|---|---|
| RAM test | `0x0502` | PSRAM hardware failure | Power cycle. If persists, replace ESP32 module. |
| Flash test | `0x0140` | NVS partition corrupted | Erase NVS: `esptool.py erase_region 0x9000 0x6000`. Re-flash. |
| Camera test | `0x0100` | Camera not connected, wrong module | Check FPC cable. Verify OV2686 module. |
| LED test | `0x0110` | LED board disconnected | Check LED FPC cable. Test with multimeter. |
| UART test | `0x0402` | Peer MCU not running, wiring fault | Verify peer MCU is flashed and booting. Check UART cross-wiring. |

### Watchdog / Reset

| Symptom | Possible Cause | Diagnosis | Fix |
|---|---|---|---|
| Device resets randomly | Watchdog timeout, stack overflow | Check `SAFE_WATCHDOG_TIMEOUT (0x0501)` in error log. Check reset reason in boot log. | Check for infinite loops. Increase watchdog timeout. Check stack sizes. |
| "Stack overflow detected" | Task stack too small | Check `SAFE_STACK_OVERFLOW (0x0504)`. Check which task crashed. | Increase task stack size in FreeRTOS config. |
| "Heap low" warning | Memory leak or excessive allocation | Check `SAFE_HEAP_LOW (0x0503)`. Monitor `heap_free_kb` over time. | Power cycle. If heap decreases over time, report as firmware bug. |
| Memory corruption | Buffer overrun, hardware fault | Check `SAFE_MEMORY_CORRUPTION (0x0502)`. Run full POST. | Power cycle. Re-flash firmware. If persists, replace ESP32 module. |
| Over-temperature shutdown | Ambient temperature too high, ventilation blocked | Check `SAFE_OVER_TEMPERATURE (0x0511)`. Check `cpu_temp_celsius`. | Move device to cooler location. Ensure ventilation not blocked. Wait for cool-down. |

### NVS Corruption

| Symptom | Possible Cause | Diagnosis | Fix |
|---|---|---|---|
| Settings lost after reboot | NVS write failure, power interruption during write | Check NVS error codes in log. | Re-enter settings. Verify with `nvs_get`. |
| Calibration data missing | NVS corruption | Check `CAL_NVS_LOAD_FAIL (0x0221)`. Read NVS magic number. | Recalibrate with Color Chart. |
| Error registry corrupt | Incomplete NVS write | Check `ErrorRegistry::loadFromNvs()` return value. | Clear error registry: `ErrorRegistry::clearErrors()`. Persist fresh copy. |
| Complete NVS corruption | Flash failure, power loss | Multiple NVS errors on boot. | Erase NVS partition and re-flash. Full recalibration required. |

<a id="flash-failure"></a>

### Flash Failure

| Symptom | Possible Cause | Diagnosis | Fix |
|---|---|---|---|
| esptool.py: "Failed to connect" | Wrong port, device not in download mode | Check USB cable. Hold BOOT button while pressing RESET. | Try different USB cable. Hold BOOT, press RESET, release BOOT. |
| esptool.py: "Invalid head of packet" | Baud rate too high, noise on USB | Try lower baud rate. | Use `--baud 115200` for initial test, then increase. |
| "Verify failed" after flash | Corrupted flash, bad firmware image | Re-flash. Verify binary checksum. | Download fresh firmware binaries. Try different flash chip speed. |
| Flash size detection wrong | Wrong `--flash_size` argument | Compare detected size with specification. | Use correct `--flash_size`: 4MB for Meas MCU, 16MB for UI MCU. |

---

## Diagnostics Export

### Via Serial Monitor

Connect to the Measurement MCU debug console and request the error log:

```bash
# Open serial monitor
idf.py -p /dev/ttyUSB0 monitor
```

Then issue diagnostics command via UI: **System Menu > Diagnostics > Export Error Log**

The Measurement MCU sends `DIAGNOSTICS_REPORT (0x13)` containing all error entries.

### Via UART IPC Command

From the UI MCU, send:

```
CMD_RUN_DIAGNOSTICS (0x88)
```

The Measurement MCU responds with:

```
DIAGNOSTICS_REPORT (0x13):
  heap_free_kb: 245.3
  cpu_temp:     38.2°C
  battery:      5.05V
  psram:        OK
  flash:        OK
  camera:       OK
  led:          OK
  uart:         OK
  health:       100.0%
  uptime:       3847s
  errors:       2 registered
    [0] 0x0400 LOW  "UART CRC mismatch" x3 (last: 1200ms ago)
    [1] 0x0210 LOW  "Calibration expired" x1 (last: 3600000ms ago)
```

### Error Log Interpretation

Each error entry contains:

| Field | Meaning |
|---|---|
| Code | Error code (see [API Reference](../API_REFERENCE.md#error-codes-reference)) |
| Severity | LOW / MEDIUM / HIGH / CRITICAL |
| Message | Human-readable description |
| Count | Number of occurrences (deduplicated) |
| First seen | Timestamp of first occurrence |
| Last seen | Timestamp of most recent occurrence |

### Error Code Quick Reference

| Range | Category | Examples |
|---|---|---|
| `0x0100-0x01FF` | Hardware | Camera, LED, temperature, ADC, flash |
| `0x0200-0x02FF` | Calibration | Fit diverged, R² low, expired, NVS |
| `0x0300-0x03FF` | Measurement | No peak, weak control line, QC fail |
| `0x0400-0x04FF` | Communication | UART CRC, timeout, sync lost, WiFi |
| `0x0500-0x05FF` | Safety | POST fail, watchdog, memory, temp |

---

## Escalation Path

### Level 1 — Field Technician

- Follow this troubleshooting guide
- Power cycle the device
- Re-flash firmware
- Recalibrate with Color Chart
- Export error log

### Level 2 — Technical Support

Contact if:
- Same error persists after re-flash and recalibration
- CRITICAL severity errors (0x05xx)
- Hardware fault suspected (camera, LED, display)
- QC measurements consistently out of tolerance

Provide:
- Device serial number
- Firmware version
- Exported error log
- Description of symptoms and steps already attempted

### Level 3 — Engineering

Contact if:
- Firmware bug suspected (reproducible crash, incorrect calculation)
- New error pattern not covered in this guide
- Hardware design issue affecting multiple units

Provide:
- Full serial monitor output from both MCUs
- Error log export
- Production batch number
- Steps to reproduce
