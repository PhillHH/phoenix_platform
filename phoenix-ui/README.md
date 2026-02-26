# Phoenix v108.0 "Chimera" — UI/Connectivity MCU

## Target Hardware
- **MCU:** ESP32-WROOM-32D (Igloo Pro) — Chip #1
- **Display:** ST7701S (SPI/RGB)
- **Touch:** FT5x06 (I2C)
- **Connectivity:** WiFi 802.11 b/g/n, BLE 4.2
- **IPC:** UART1 921600 baud → ESP32 #2 (Measurement MCU)

## Architecture

```
ESP32 #1 (UI/Connectivity MCU)
┌──────────────────────────────────────────────┐
│  app_main()                                  │
│    └─ main_task (Core 0)                     │
│         ├─ NVS + UART init                   │
│         ├─ MeasurementProxy (IPC client)     │
│         ├─ LVGL init (ST7701S + FT5x06)     │
│         ├─ PhoenixUI                         │
│         │   ├─ StatusBar (WiFi/Battery/...)  │
│         │   ├─ CircularMenu (Home)           │
│         │   ├─ MeasurementScreen             │
│         │   ├─ ResultScreen                  │
│         │   ├─ CalibrationScreen             │
│         │   ├─ PatientScreen                 │
│         │   └─ SettingsScreen                │
│         └─ WiFiManager                       │
│                                              │
│    └─ ui_task (Core 0, 200 Hz)              │
│         ├─ lv_timer_handler()                │
│         └─ proxy.poll() — UART responses     │
└──────────────────┬───────────────────────────┘
                   │ UART1 (TX=17, RX=16)
                   │ 921600 baud
┌──────────────────┴───────────────────────────┐
│  ESP32 #2 (Measurement MCU)                  │
│  (phoenix-measurement project)               │
└──────────────────────────────────────────────┘
```

## Build

```bash
. $HOME/esp/esp-idf/export.sh
idf.py add-dependency "lvgl/lvgl^8.3"
idf.py set-target esp32
idf.py build
idf.py -p /dev/ttyUSB1 flash monitor
```

## UI Flow
```
[Home] ──→ [Measurement] ──→ [Result]
  │                              │
  ├──→ [Calibration]            └──→ [Home]
  ├──→ [Patients]
  ├──→ [Settings]
  └──→ [System Menu] (status bar tap)
```
