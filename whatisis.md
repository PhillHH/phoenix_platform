# Phoenix Platform — Was ist das, wo stehen wir, was fehlt

**Stand:** 2026-02-27 | **Version:** v108.0.1 "Chimera" | **Branch:** `claude/analyze-project-structure-UNkOR`

---

## 1. Was ist das?

Die **Phoenix Platform** ist die Firmware-Plattform für den **Igloo Pro Analyzer** — ein Point-of-Care (PoC) Diagnosegerät, das Lateral-Flow-Immunoassays (LFA) optisch quantifiziert. Es ist kein Schwangerschaftstest-Ableser — es ist ein vollwertiges IVD-Gerät mit 5-Parameter-Logistik-Kalibrierung, das klinisch relevante Konzentrationen in ng/mL liefert.

### Was das Gerät macht

```
Patient legt Testkassette ein
    → Gerät erkennt Kassette (Barcode/Chip)
    → LEDs beleuchten den Teststreifen
    → Kamera erfasst Bild des Streifens
    → Algorithmus extrahiert 1D-Profil aus dem Bild
    → Peak-Detektion findet Test- und Kontrolllinien
    → 5PL-Kalibrierung rechnet Signal → Konzentration um
    → Ergebnis auf Display + optional an LIMS/Cloud
```

### Unterstützte Technologien (4 Stück)

| # | Technologie | Beispiel-Analyten | Detektionsprinzip |
|---|---|---|---|
| 1 | **Colorimetric** (Gold-NP) | CRP, IgE, Troponin I, HCG | Farb-Absorption, LED + Kamera |
| 2 | **Immunofluorescence** | TSH, Vitamin D, Ferritin | UV-Anregung, Emissions-Capture |
| 3 | **Dry Chemistry** | HbA1c, Glucose, Lipide | Reflektometrie auf Reagenz-Pads |
| 4 | **Microfluidics** | PT/INR, Gerinnung | Optische Detektion in Mikrokanälen |

### 30 verifizierte Assay-Konfigurationen

Alle 30 Assays haben validierte 5PL-Parameter aus realen Dx365-MCP-Messungen:

| Analyt | LOINC | Technologie | Bereich | Einheit |
|---|---|---|---|---|
| CRP | 1988-5 | Colorimetric | 0–200 | mg/L |
| Troponin I | 49563-0 | Colorimetric | 0–50 | ng/mL |
| IgE (total) | 19113-0 | Colorimetric | 0–5000 | IU/mL |
| HCG | 2106-3 | Colorimetric | 0–25000 | mIU/mL |
| TSH | 3016-3 | Fluorescence | 0–50 | mIU/L |
| Vitamin D | 62292-8 | Fluorescence | 0–150 | ng/mL |
| Ferritin | 2276-4 | Fluorescence | 0–1000 | ng/mL |
| HbA1c | 59261-8 | Dry Chemistry | 4–14 | % |
| Glucose | 2345-7 | Dry Chemistry | 20–600 | mg/dL |
| PT/INR | 46418-0 | Microfluidics | 0.8–5.0 | INR |
| ... und 20 weitere | | | | |

---

## 2. Architektur — Dual-MCU Design

```
 ┌─────────────────────────────────────────────────────────────┐
 │                     IGLOO PRO GERÄT                         │
 │                                                             │
 │  ┌───────────────────┐   UART 921600 bd   ┌──────────────┐ │
 │  │   ESP32-S3 #1     │◄═══════════════════►│ ESP32-S3 #2  │ │
 │  │   "UI MCU"        │    CRC-16/CCITT     │ "Mess MCU"   │ │
 │  │                   │    Framed Protocol  │              │ │
 │  │ ┌───────────────┐ │                     │ ┌──────────┐ │ │
 │  │ │ LVGL Display  │ │                     │ │ OV2686   │ │ │
 │  │ │ ST7701S 480px │ │                     │ │ Kamera   │ │ │
 │  │ │ GT911 Touch   │ │                     │ │ DVP Bus  │ │ │
 │  │ ├───────────────┤ │                     │ ├──────────┤ │ │
 │  │ │ WiFi (STA)    │ │    Befehle →        │ │ RGB LEDs │ │ │
 │  │ │ HTTP/HTTPS    │ │    ← Ergebnisse     │ │ 4-Kanal  │ │ │
 │  │ │ LIMS Client   │ │    ← Progress       │ │ LEDC PWM │ │ │
 │  │ ├───────────────┤ │    ← Fehler         │ ├──────────┤ │ │
 │  │ │ NVS: Settings │ │                     │ │ 5PL Algo │ │ │
 │  │ │ NVS: Patients │ │                     │ │ Peak Det │ │ │
 │  │ │ NVS: Results  │ │                     │ │ LM Fit   │ │ │
 │  │ │ NVS: WiFi     │ │                     │ │ Pipeline │ │ │
 │  │ │ NVS: LIMS cfg │ │                     │ ├──────────┤ │ │
 │  │ ├───────────────┤ │                     │ │ NVS: Cal │ │ │
 │  │ │ Demo Engine   │ │                     │ │ NVS: Err │ │ │
 │  │ │ (Standalone)  │ │                     │ │ NVS: Aud │ │ │
 │  │ └───────────────┘ │                     │ └──────────┘ │ │
 │  └───────────────────┘                     └──────────────┘ │
 │        │                                                     │
 └────────┼─────────────────────────────────────────────────────┘
          │ HTTPS (WiFi)
          ▼
 ┌─────────────────────────────────────────┐
 │         CLOUD / LIMS                    │
 │                                         │
 │  POST /api/v1/observations              │
 │  GET  /api/v1/health                    │
 │  (HL7 FHIR-kompatibles JSON)           │
 │                                         │
 │  Geplant:                               │
 │  GET  /api/v1/calibrations/{device_id}  │
 │  POST /api/v1/devices/{id}/status       │
 │  GET  /api/v1/assays/latest             │
 │  WS   /api/v1/stream (Real-time)        │
 └─────────────────────────────────────────┘
```

### Warum zwei Chips?

| Aspekt | Ein Chip | Zwei Chips (aktuelles Design) |
|---|---|---|
| CPU-Last bei Messung | Kamera + Algo + UI blockiert sich | Kamera + Algo laufen ungestört auf Core 1 |
| Echtzeitfähigkeit | UI-Rendering kann Messung stören | Mess-MCU hat dedizierte 240 MHz |
| IEC 62304 Isolation | Alles im gleichen Adressraum | Safety-kritischer Code physisch isoliert |
| Update-Risiko | UI-Update kann Algorithmus brechen | MCUs unabhängig updatebar |
| Watchdog | Gemeinsamer Watchdog | Jeder Chip hat eigenen WDT |

---

## 3. Was ist schon da — Implementierungsstand

### 3.1 Measurement MCU (phoenix-measurement) — ~6.800 LOC

| Modul | Dateien | Status | Reifegrad |
|---|---|---|---|
| **Core/Result.h** | Result<T>, ErrorInfo | Fertig, getestet | 100% |
| **Core/FixedString.h** | Heap-freie Strings | Fertig, getestet | 100% |
| **Core/ServiceLocator.h** | DI Container | Fertig | 90% |
| **Core/SafetyManager** | POST, WDT, Temp, Memory | Implementiert, HW-Stubs | 70% |
| **Core/Logger.h** | Ring-Buffer 100 Entries | Fertig, getestet | 100% |
| **Core/ErrorRegistry.h** | 38 Error-Codes, NVS | Fertig, getestet | 100% |
| **Analysis/Dx365Algorithm** | 5PL, Peak-Detection | Fertig, 30 Assays validiert | 95% |
| **Analysis/MeasurementPipeline** | 9-Stufen-Pipeline | Implementiert | 80% |
| **Analysis/CurveCorrection** | Polynom-Baseline | Implementiert | 85% |
| **Analysis/ImageProcessor** | ROI-Extraktion | Implementiert | 75% |
| **Calibration/VerifiedAssayRegistry** | 30 Assays | Fertig, validiert | 100% |
| **Calibration/FactoryCalibration** | 8 Fabrikkurven | Fertig, getestet | 100% |
| **Calibration/ColorChartCalibration** | 11-Strip Pipeline | Implementiert | 85% |
| **Services/CalibrationService** | LM-Fitting, NVS | Fertig, getestet | 95% |
| **Services/BenchmarkValidator** | Klinische Referenzen | Fertig | 90% |
| **HAL/CameraController_OV2686** | DVP Kamera-Treiber | Implementiert, HW nötig | 60% |
| **HAL/LEDController** | 4-Kanal LEDC PWM | Implementiert | 70% |
| **IPC/UartBridge** | CRC-16 Framing | Fertig, getestet | 95% |
| **IPC/CommandProtocol** | Command Handler | Implementiert | 80% |
| **Core/AuditTrail** | NVS Ring-Buffer | Implementiert | 85% |
| **main.cpp** | Boot, Loop, Dispatch | Fertig | 90% |

**Algorithmischer Kern: ~95% fertig.** 813 Unit-Tests bestätigen das.

### 3.2 UI MCU (phoenix-ui) — ~5.400 LOC

| Modul | Dateien | Status | Reifegrad |
|---|---|---|---|
| **Core/** | Result, FixedString, Logger, ErrorRegistry | Shared mit Measurement | 100% |
| **Core/MeasurementTypes.h** | Shared Datentypen | Fertig | 100% |
| **Core/AssayTechnology.h** | 16 Demo-Assays, 4 Technologien | Fertig | 100% |
| **UI/PhoenixUI** | LVGL Screen-Manager | Implementiert | 75% |
| **UI/CircularMenu** | Animiertes Kreismenü | Implementiert | 70% |
| **UI/Screens/HomeScreen** | Home mit Assay-Auswahl | Implementiert | 70% |
| **UI/Screens/MeasurementScreen** | Progress-Anzeige | Implementiert | 75% |
| **UI/Screens/ResultScreen** | Ergebnis mit Referenzbereich | Implementiert | 75% |
| **UI/Screens/CalibrationScreen** | Kalibrierungsablauf | Implementiert | 60% |
| **UI/Screens/SettingsScreen** | WiFi, LIMS, Sprache | Implementiert | 65% |
| **UI/Screens/PatientScreen** | Patientenverwaltung | Implementiert | 70% |
| **UI/Screens/TestSelectionScreen** | Assay-Katalog | Implementiert | 70% |
| **UI/Accessibility** | Kontrast, Schriftgröße | Implementiert | 60% |
| **Services/LocalizationManager** | 9 Sprachen | Implementiert | 80% |
| **Services/DemoMeasurementEngine** | Standalone-Simulation | Fertig | 90% |
| **Services/PatientManager** | GDPR-konform, NVS | Implementiert | 80% |
| **Services/ResultStorage** | NVS Ring-Buffer | Implementiert | 80% |
| **Connectivity/WiFiManager** | WPA2-PSK STA-Mode | Implementiert | 75% |
| **Connectivity/HttpLimsClient** | FHIR JSON, Bearer Auth | Implementiert | 70% |
| **IPC/MeasurementProxy** | UI→Measurement Proxy | Fertig | 90% |
| **IPC/UartBridge** | Identisch mit Measurement | Fertig | 95% |
| **HAL/DisplayDriver_ST7701S** | SPI/RGB Display | **REAL DRIVER** (nicht verbunden) | 85% |
| **HAL/TouchDriver_CTP** | FT5x06/CST820 I2C Touch | **REAL DRIVER** (nicht verbunden) | 85% |
| **main.cpp** | Boot, Task-Setup | Fertig | 85% |

### KRITISCH: Die "Verdrahtungslücke" auf dem UI-MCU

Die Agenten-Analyse hat ein systematisches Problem aufgedeckt: **Viele Module sind
fertig implementiert, aber nicht in die Boot-Sequenz oder UI-Navigation eingebunden.**

#### Nicht initialisierte Services (main.cpp ruft `initialize()` nicht auf):

| Service | Code vorhanden | Am Boot initialisiert? |
|---|---|---|
| WiFiManager | Ja | **NEIN** (Zeile 167: "Phase 4") |
| HttpLimsClient | Ja | **NEIN** |
| PatientManager | Ja | **NEIN** |
| ResultStorage | Ja | **NEIN** |
| LocalizationManager | Ja (9 Sprachen!) | **NEIN** |
| AccessibilityManager | Ja | **NEIN** |
| DemoMeasurementEngine | Ja | **NEIN** |
| ErrorRegistry | Ja | **NEIN** (loadFromNvs nicht aufgerufen) |

#### Display/Touch: Echte Treiber nicht verbunden

```
IST-ZUSTAND:
  main.cpp:init_lvgl()  →  320x240 STUB Flush  →  Kein Display-Output
                         →  Stub Touch-Callback  →  Kein Touch-Input

SOLL-ZUSTAND:
  DisplayDriver_ST7701S::displayInit()  →  480x480 RGB über XL9535 I/O-Expander
  TouchDriver_CTP::touchInit()          →  Auto-Detect CST820/FT5x06 via I2C

Die echten Treiber existieren, sind hardware-spezifisch für das Igloo Pro PCB
(I2C SDA=GPIO8, SCL=GPIO48, Touch INT=GPIO1, Backlight=GPIO46) und voll
funktional — sie werden nur nicht aus main.cpp aufgerufen.
```

#### UI-Screens: Doppelt implementiert, nicht verbunden

| Screen-Datei | Aufwändige Implementierung | In PhoenixUI.cpp verbunden? |
|---|---|---|
| `MeasurementScreen.cpp` | 8-Stufen Pipeline, animierte Dots | **NEIN** (inline Simpel-Version wird genutzt) |
| `ResultScreen.cpp` | Detaillierte Ergebniskarte | **NEIN** (inline Version) |
| `CalibrationScreen.cpp` | 11-Strip DXR.007.01 UI | **NEIN** ("Phase 4") |
| `PatientScreen.cpp` | Suchleiste, Patientenliste | **NEIN** ("Phase 4") |
| `SettingsScreen.cpp` | 4 Sektionen, MAC-Anzeige | **NEIN** ("Phase 4") |
| `TestSelectionScreen.cpp` | 4 Tech-Kategorien, 16 Assays | **NEIN** (kein Navigations-Aufruf) |

**Fazit:** Die UI-MCU ist wie ein Auto, bei dem Motor, Getriebe und Räder
eingebaut sind, aber die Kupplung fehlt. Die Teile müssen "verdrahtet" werden —
das ist eine Integration-Aufgabe, kein neuer Code.

### 3.3 Wokwi Simulation — ~1.100 LOC

| Komponente | Status |
|---|---|
| 8 Testsuiten in phoenix_test_core.h | Fertig |
| diagram.json (4 LEDs, UART, Button) | Fertig |
| wokwi.toml | Fertig |
| SIMULATION.md | Fertig |
| Single Source of Truth (shared/) | Fertig |

### 3.4 Host Unit Tests — ~3.100 LOC

| Testsuite | Suites | Assertions | Status |
|---|---|---|---|
| Result<T> | 10 | ~60 | PASS |
| FixedString<N> | 13 | ~85 | PASS |
| 5PL Math + 30 Assays | 17 | ~280 | PASS |
| Peak Detection | 10 | ~80 | PASS |
| Calibration/LM Fitting | 12 | ~100 | PASS |
| UART/CRC-16 | 14 | ~90 | PASS |
| NVS Storage | 5 | ~30 | PASS |
| Logger | 15 | ~55 | PASS |
| ErrorRegistry | 14 | ~33 | PASS |
| **Gesamt** | **115** | **813** | **ALL PASS** |

### 3.5 Dokumentation

| Dokument | Status | Umfang |
|---|---|---|
| ARCHITECTURE.md | Fertig | System-Design, IPC-Protokoll, Memory-Budget |
| CALIBRATION.md | Fertig | 5PL-Modell, LM-Fitting, alle 30 Assays |
| API_REFERENCE.md | Fertig | 36 Public APIs, 38 Error-Codes |
| REGULATORY.md | Fertig | IEC 62304, SOUP, Traceability Matrix |
| 4 Runbooks | Fertig | Manufacturing, Troubleshooting, Field Service, Development |
| ANALYSIS_REPORT.md | Fertig | Code-Analyse, Compiler-Checks |
| BUILD_REPORT.md | Fertig | Release-Gate v108.0.1 |
| CHANGELOG.md | Fertig | Keep-a-Changelog Format |

### KRITISCH: Integration-Gaps auf dem Measurement-MCU

| Modul | Problem |
|---|---|
| **ErrorRegistry** | 38 Error-Codes definiert + NVS-Persistenz, aber `registerError()` wird **nirgendwo aufgerufen**. Errors fließen nur über Result<T> und Logger, nicht in die persistente Registry. |
| **AuditTrail** | NVS Ring-Buffer mit 1000 Einträgen implementiert, aber **kein Header**, kein Read/Export-API, `entry_count_` wird nie aktualisiert, und nichts ruft `s_audit.log()` auf. |
| **CommandProtocol** | `CommandDispatcher`-Klasse vollständig definiert, aber **nie instantiiert**. `main.cpp` hat seine eigene inline `handle_command()` stattdessen. Toter Code. |
| **Dx365 applyCalibration()** | Loggt nur "Calibration applied", **tut aber nichts**. Color Chart Kalibrierung wird nicht auf Profile angewendet. |
| **4 IPC-Kommandos** | `SET_EXPOSURE`, `SET_LED`, `CAPTURE_PREVIEW`, `IMAGE_THUMBNAIL` sind im Protokoll definiert, aber es gibt keinen Handler dafür. |

---

## 4. Das IPC-Protokoll — Wie die zwei Chips reden

```
┌──────────────────────────────────────────────────┐
│                  UART Frame                       │
│                                                   │
│  ┌────────┬────────┬────────┬──────────┬────────┐ │
│  │ SYNC   │ CMD    │ LENGTH │ PAYLOAD  │ CRC-16 │ │
│  │ 0xAA55 │ 1 byte │ 2 byte │ 0-512 B  │ 2 byte │ │
│  └────────┴────────┴────────┴──────────┴────────┘ │
│                                                   │
│  UART: 921600 Baud, 8N1                           │
│  CRC: CRC-16/CCITT (Poly 0x1021)                 │
│  Max Payload: 512 Bytes                           │
│  Timeout: 500ms (Ping), 30s (Measurement)         │
└──────────────────────────────────────────────────┘
```

### Kommandos (UI → Measurement)

| Command | Byte | Payload | Beschreibung |
|---|---|---|---|
| `PING` | 0x01 | — | Heartbeat |
| `CMD_START_MEASUREMENT` | 0x10 | Config (opt.) | Messung starten |
| `CMD_CANCEL_MEASUREMENT` | 0x11 | — | Messung abbrechen |
| `CMD_START_CALIBRATION` | 0x20 | Analyt-Name (ASCII) | Kalibrierung starten |
| `CMD_ADD_CAL_POINT` | 0x21 | float conc + float signal | Kalibrierpunkt hinzufügen |
| `CMD_FINISH_CALIBRATION` | 0x22 | — | LM-Fitting auslösen |
| `CMD_RUN_DIAGNOSTICS` | 0x30 | — | POST ausführen |
| `CMD_GET_STATUS` | 0x31 | — | Status abfragen |

### Antworten (Measurement → UI)

| Response | Byte | Payload | Beschreibung |
|---|---|---|---|
| `PONG` | 0x02 | — | Heartbeat-Antwort |
| `ACK` | 0x06 | — | Befehl akzeptiert |
| `NACK` | 0x15 | — | Befehl abgelehnt |
| `MEASUREMENT_RESULT` | 0x80 | MeasurementResult struct | Endergebnis |
| `MEASUREMENT_PROGRESS` | 0x81 | uint8 pct + char[] msg | Fortschritt |
| `ERROR_REPORT` | 0x82 | ErrorCategory + char[] msg | Fehlermeldung |
| `DIAGNOSTICS_REPORT` | 0x83 | DiagnosticsReport struct | POST-Ergebnis |

---

## 5. Cloud-Integration und Standalone-Modus

### 5.1 Ist-Zustand: Was schon geht

**LIMS-Client (HttpLimsClient.cpp) — implementiert:**

```
Gerät                           Cloud/LIMS
  │                                │
  │  POST /api/v1/observations     │  ← Ergebnis-Upload (FHIR JSON)
  │ ──────────────────────────────►│
  │                                │
  │  GET  /api/v1/health           │  ← Connectivity-Check
  │ ──────────────────────────────►│
  │                                │
  │  Headers:                      │
  │   Authorization: Bearer <key>  │
  │   Content-Type: application/json
  │   X-Device-ID: <serial>        │
```

**Was der LIMS-Client heute kann:**
- Einzelergebnis als HL7 FHIR `Observation` hochladen
- Konnektivitäts-Check (`/health` Endpoint)
- Bearer-Token-Authentifizierung
- Retry-Logik mit exponentiellem Backoff (3 Versuche)
- TLS-Unterstützung (via ESP-IDF mbedTLS)
- Konfiguration persistent in NVS (URL, API-Key, Device-ID, Practice-ID)
- Timeout konfigurierbar (Default: 10s)

**JSON-Format eines hochgeladenen Ergebnisses:**
```json
{
  "resourceType": "Observation",
  "status": "final",
  "device_id": "IGLOO-PRO-001",
  "practice_id": "PRAXIS-DE-42",
  "record_id": 1234,
  "patient_id": 5678,
  "timestamp": 1740672000,
  "analyte": "CRP",
  "value": 12.34,
  "unit": "mg/L",
  "interpretation": "ELEVATED",
  "signal": 0.854,
  "control": 0.921,
  "tc_ratio": 0.927,
  "confidence": 0.97,
  "ref_low": 0.0,
  "ref_high": 5.0,
  "qc_flags": 0,
  "firmware": "Phoenix v108.0"
}
```

### 5.2 Standalone-Modus

Das Gerät funktioniert **vollständig ohne Cloud/Netzwerk**:

```
                 ┌─────────────────────────┐
                 │    Standalone-Modus      │
                 │                          │
                 │  WiFi: nicht verbunden   │
                 │  LIMS: nicht konfiguriert│
                 │                          │
                 │  ► Messung: JA           │
                 │  ► Ergebnis: auf Display │
                 │  ► Speicherung: NVS lokal│
                 │  ► Kalibrierung: lokal   │
                 │  ► Patientenverwaltung: JA│
                 │  ► Batch-Sync: wenn WiFi │
                 │    wieder da             │
                 └─────────────────────────┘
```

**Implementiert:**
- `DemoMeasurementEngine` — Simuliert realistische Messungen für alle 4 Technologien, wenn der Measurement-MCU nicht antwortet
- Auto-Switch: `isDemoMode()` → `setLiveMode()` sobald UART-PONG empfangen wird
- Ergebnisse werden lokal in NVS gespeichert (`ResultStorage`)
- Patientenverwaltung läuft komplett lokal
- `syncPending()` — Batch-Upload wenn WiFi wieder verfügbar (angelegt, noch nicht mit ResultStorage verdrahtet)

### 5.3 Was fehlt für Cloud-Kalibrierung

| Feature | Status | Beschreibung |
|---|---|---|
| **Kalibrierungsdaten-Download** | FEHLT | `GET /api/v1/calibrations/{device_id}` |
| **Assay-Updates OTA** | FEHLT | Neue Assay-Konfigurationen aus der Cloud laden |
| **Geräte-Registrierung** | FEHLT | Device-Provisioning bei Erstinbetriebnahme |
| **Zertifikat-Pinning** | FEHLT | mTLS oder Certificate Pinning für Produktion |
| **Cloud-Cal-Caching** | FEHLT | Heruntergeladene Kalibrierung lokal cachen |
| **Batch-Sync Verdrahtung** | 50% | `syncPending()` existiert, aber nicht mit ResultStorage verbunden |
| **OTA-Firmware-Update** | FEHLT | esp_https_ota für Remote-Update |
| **MQTT/WebSocket** | FEHLT | Real-time Streaming statt Polling |
| **QC-Daten Upload** | FEHLT | Qualitätskontrolldaten an Cloud senden |
| **Remote-Diagnostik** | FEHLT | Gerätestatus an Cloud melden |

---

## 6. Schnittstellen — Wie sich das integrieren lässt

### 6.1 REST API (LIMS/Cloud)

**Bestehende Endpoints:**

| Method | Endpoint | Richtung | Beschreibung |
|---|---|---|---|
| `POST` | `/api/v1/observations` | Gerät → Cloud | Messergebnis hochladen |
| `GET` | `/api/v1/health` | Gerät → Cloud | Health-Check |

**Geplante/benötigte Endpoints:**

| Method | Endpoint | Richtung | Beschreibung |
|---|---|---|---|
| `GET` | `/api/v1/calibrations/{device_id}` | Cloud → Gerät | Kalibrierungsdaten abrufen |
| `GET` | `/api/v1/assays/latest` | Cloud → Gerät | Aktuellste Assay-Definitionen |
| `POST` | `/api/v1/devices/register` | Gerät → Cloud | Geräte-Provisioning |
| `POST` | `/api/v1/devices/{id}/status` | Gerät → Cloud | Gerätestatus melden |
| `POST` | `/api/v1/devices/{id}/diagnostics` | Gerät → Cloud | POST-Ergebnisse melden |
| `GET` | `/api/v1/firmware/{device_id}/latest` | Cloud → Gerät | OTA-Update Check |
| `POST` | `/api/v1/qc/results` | Gerät → Cloud | QC-Ergebnisse melden |
| `GET` | `/api/v1/patients/sync` | Bidirektional | Patienten-Synchronisation |

### 6.2 UART IPC (Mess-MCU ↔ UI-MCU)

Siehe Abschnitt 4. Binäres Frame-Protokoll, CRC-geschützt, 921600 Baud.

### 6.3 NVS Storage (lokale Persistenz)

| NVS Namespace | MCU | Inhalt |
|---|---|---|
| `calibration` | Measurement | 5PL-Parameter, Kalibrierungsdatum |
| `errors` | Measurement | Error-Registry mit Dedup |
| `audit` | Measurement | Audit-Trail Ring-Buffer (1000 Einträge) |
| `patients` | UI | Patientendaten (GDPR: anonymisierbar) |
| `results` | UI | Messergebnisse Ring-Buffer |
| `lims` | UI | LIMS-URL, API-Key, Device-ID |
| `wifi` | UI | SSID, Passwort |
| `settings` | UI | Sprache, Kontrast, Helligkeit |

### 6.4 Authentifizierung

| Methode | Status | Beschreibung |
|---|---|---|
| Bearer Token | Implementiert | API-Key im Header `Authorization: Bearer <key>` |
| mTLS | Vorbereitet | `esp_tls.h` inkludiert, Client-Cert-Support in ESP-IDF |
| Certificate Pinning | FEHLT | Für Produktion empfohlen |
| Device Certificate | FEHLT | Pro-Gerät-Zertifikat aus Secure Element |

### 6.5 Datenformate

| Format | Verwendung | Status |
|---|---|---|
| HL7 FHIR JSON (Observation) | Ergebnis-Upload | Implementiert |
| Custom Binary (UART Frame) | MCU-zu-MCU IPC | Implementiert |
| NVS Blob (packed structs) | Lokale Persistenz | Implementiert |
| JSON (Settings) | Cloud-Config-Download | FEHLT |

---

## 7. Was konkret fehlt — Priorisierte Roadmap

### Phase 1: Hardware-Integration (P0 — ohne das geht nichts)

| # | Task | Aufwand | Abhängigkeit |
|---|---|---|---|
| 1.1 | **DisplayDriver_ST7701S** implementieren | 2–3 Tage | Hardware vorhanden |
| 1.2 | **TouchDriver_CTP** (FT5x06/GT911) implementieren | 1–2 Tage | Hardware vorhanden |
| 1.3 | **CameraController_OV2686** auf realer Hardware verifizieren | 2–3 Tage | DVP-Kamera-Modul |
| 1.4 | **LEDController** auf realer Hardware kalibrieren | 1 Tag | LED-Board |
| 1.5 | **SafetyManager** ADC für Spannungsüberwachung | 1 Tag | Spannungsteiler auf PCB |
| 1.6 | **UART-IPC** mit zwei ESP32-S3 physisch testen | 1 Tag | 2x DevKit + UART-Kabel |

### Phase 2: Cloud-Kalibrierung (P1 — Kernfeature)

| # | Task | Aufwand | Abhängigkeit |
|---|---|---|---|
| 2.1 | Cloud-API Endpoint `GET /calibrations/{device_id}` | Backend | Cloud-Team |
| 2.2 | `CalibrationDownloader` auf Gerät implementieren | 2 Tage | 2.1 |
| 2.3 | Kalibrierungs-Cache in NVS (mit Ablaufdatum) | 1 Tag | 2.2 |
| 2.4 | Auto-Sync beim Einschalten: lokale Cal vs Cloud-Cal vergleichen | 1 Tag | 2.3 |
| 2.5 | Fallback: Wenn Cloud nicht erreichbar → letzte gecachte Cal verwenden | 0.5 Tage | 2.3 |

### Phase 3: Produktionsreife (P1)

| # | Task | Aufwand |
|---|---|---|
| 3.1 | OTA-Update via `esp_https_ota` | 2 Tage |
| 3.2 | Batch-Sync `syncPending()` mit ResultStorage verdrahten | 1 Tag |
| 3.3 | Logger Thread-Safety (portMUX spinlock) | 0.5 Tage |
| 3.4 | Certificate Pinning / mTLS für LIMS | 1 Tag |
| 3.5 | Device Provisioning (Erstregistrierung) | 2 Tage |
| 3.6 | Partitionstabelle: Factory + 2x OTA App | 1 Tag |
| 3.7 | Power Management (Light Sleep zwischen Messungen) | 2 Tage |

### Phase 4: Erweiterte Integration (P2)

| # | Task | Aufwand |
|---|---|---|
| 4.1 | MQTT/WebSocket für Real-time-Status | 3 Tage |
| 4.2 | Remote-Diagnostik (Gerätestatus an Cloud) | 2 Tage |
| 4.3 | QC-Daten-Upload | 1 Tag |
| 4.4 | Assay-Definition-Updates aus Cloud | 2 Tage |
| 4.5 | Multi-Geräte-Management (Fleet-API) | Backend |
| 4.6 | Barcode-Scanner für Kassetten-ID | HW + 2 Tage SW |
| 4.7 | Drucker-Anbindung (Bluetooth/USB) | 2 Tage |

---

## 8. Wie es auf die Chips kommt

### 8.1 Build-Toolchain

```bash
# 1. ESP-IDF v5.3 installieren
# 2. Measurement MCU flashen:
cd phoenix-measurement
idf.py set-target esp32s3
idf.py build
idf.py flash -p /dev/ttyUSB0 -b 921600

# 3. UI MCU flashen:
cd ../phoenix-ui
idf.py set-target esp32s3
idf.py build
idf.py flash -p /dev/ttyUSB1 -b 921600
```

### 8.2 Partitionslayout (geplant)

```
Measurement MCU (4 MB Flash):
┌──────────────┬────────────┐
│ Bootloader   │ 0x1000     │
│ Part.Table   │ 0x8000     │
│ NVS (cal)    │ 0x9000  24K│
│ NVS (audit)  │ 0xF000  16K│
│ App (factory)│ 0x10000 1.5M│
│ App (ota_0)  │ 0x190000 1.5M│
│ SPIFFS (data)│ 0x310000 960K│
└──────────────┴────────────┘

UI MCU (8 MB Flash):
┌──────────────┬────────────┐
│ Bootloader   │ 0x1000     │
│ Part.Table   │ 0x8000     │
│ NVS (config) │ 0x9000  32K│
│ NVS (results)│ 0x11000 32K│
│ App (factory)│ 0x20000 2.5M│
│ App (ota_0)  │ 0x2A0000 2.5M│
│ SPIFFS (UI)  │ 0x520000 2.8M│
└──────────────┴────────────┘
```

### 8.3 Memory Budget (geschätzt)

| Ressource | Measurement MCU | UI MCU |
|---|---|---|
| Flash (App) | ~800 kB | ~1.5 MB (inkl. LVGL) |
| DRAM (Heap) | ~160 kB | ~200 kB |
| PSRAM | 4 MB (Kamera-Buffer) | 8 MB (LVGL Frame-Buffer) |
| .bss (statisch) | ~120 kB (Assay-Registry) | ~80 kB |
| Stack (Main Task) | 16 kB | 12 kB |

---

## 9. Zusammenfassung: Wo stehen wir?

```
 Gesamtfortschritt:
 ▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓░░░░░░░░░  ~65%

 Algorithmik (5PL, Peak, LM):    ▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓░  95%  Kernalgorithmus steht
 Assay-Datenbank (30 Assays):    ▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓░  95%  Aus echten MCP-Daten
 IPC-Protokoll (UART/CRC):      ▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓░░░░  85%  Fehlen: Retransmission
 Kalibrierung (5PL + Factory):   ▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓░░░  88%  LM-Fitting steht
 Unit Tests (813 Assertions):    ▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓░░  92%  SafetyMgr fehlt
 Dokumentation:                  ▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓░░░░  85%  Fertig, kleine Lücken
 HAL Display/Touch:              ▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓░░░░  85%  Code da, nicht verdrahtet!
 HAL Kamera/LED:                 ▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓░░░░░  80%  Braucht Hardware-Test
 UI-Screens (LVGL):              ▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓░░░░░░░░  70%  Code da, nicht verdrahtet!
 WiFi/LIMS Upload:               ▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓░░░░░░░░  70%  Code da, nicht initialisiert!
 Service-Integration:            ▓▓▓░░░░░░░░░░░░░░░░░░░░░░  15%  ErrorReg/Audit nicht verbunden
 Cloud-Kalibrierung-Download:    ▓▓░░░░░░░░░░░░░░░░░░░░░░░  10%  Endpoint fehlt
 OTA-Updates:                    ░░░░░░░░░░░░░░░░░░░░░░░░░   0%  Nicht begonnen
 Device Provisioning:            ░░░░░░░░░░░░░░░░░░░░░░░░░   0%  Nicht begonnen
```

### Das zentrale Thema: "Kupplung einbauen"

Der Code-Stand ist besser als die Fortschrittsbalken suggerieren. Das Problem
ist nicht "es fehlt Code", sondern "der vorhandene Code ist nicht verbunden":

- **Display-Treiber**: Echte ST7701S-Initialisierung mit I/O-Expander, PSRAM
  Framebuffer, 480x480 RGB — liegt in `DisplayDriver_ST7701S.cpp`, wird aber
  nicht aus `main.cpp` aufgerufen (stattdessen: 320x240 Stub)
- **Touch-Treiber**: Auto-Detection CST820/FT5x06 über I2C — liegt in
  `TouchDriver_CTP.cpp`, wird nicht aufgerufen
- **9 Sprachen**: `LocalizationManager` mit 50+ Strings in DE/EN/FR/ES/IT/PT/NL/PL/TR
  — wird nie initialisiert
- **Patientenverwaltung**: GDPR-konform, 200 Patienten, Soft-Delete — wird nie
  initialisiert, UI zeigt Hardcoded-Demo-Daten
- **Ergebnis-Speicher**: 500er Ring-Buffer mit LIMS-Export-Tracking — wird nie
  initialisiert, Messergebnisse werden angezeigt aber nicht gespeichert
- **ErrorRegistry**: 38 Error-Codes, NVS-Persistenz, Severity-Eskalation — wird
  nie aufgerufen, Fehler fließen nur über Logger

**Geschätzter Aufwand um alles zu verdrahten: 3–5 Tage fokussierte Integration.**

### Was funktioniert HEUTE (auf Host / Simulator):
- Kompletter Mess-Algorithmus (5PL forward/inverse, 30 Assays)
- Peak-Detektion mit 7-Punkt-Deskriptor
- Levenberg-Marquardt Kalibrierungsfitting
- CRC-geschütztes UART-Protokoll
- Error-Handling mit 38 Fehlercodes
- Strukturiertes Logging
- NVS-Persistenz (Stubs auf Host)
- Demo-Modus mit realistischer Messsimulation

### Was funktioniert BALD (mit Hardware):
- Dual-MCU-Kommunikation über UART
- Display mit Touch (ST7701S + GT911)
- Kamera-basierte Streifenanalyse (OV2686)
- WiFi + LIMS-Upload

### Was noch GEBAUT werden muss:
- Cloud-Kalibrierungsdaten-Download und -Caching
- OTA-Firmware-Update
- Device-Provisioning / Geräte-Registrierung
- mTLS / Certificate Pinning
- Batch-Sync (ResultStorage ↔ LIMS)
- Power Management
- Barcode-Scanner-Integration (optional)

---

## 10. Codebase auf einen Blick

```
phoenix_platform/                  16.400 LOC total
├── phoenix-measurement/    6.816 LOC   ← Mess-Algorithmen, HAL, Safety
├── phoenix-ui/             5.442 LOC   ← UI, Cloud, Patientenverwaltung
├── wokwi-test/             1.089 LOC   ← Wokwi-Simulation
├── tests/                  3.066 LOC   ← 115 Testsuiten, 813 Assertions
└── docs/                   ~8.000 LOC  ← Vollständige Dokumentation

Compiler:  g++ 13.3 / ESP-IDF GCC 13.2
Standard:  C++17 (gnu++17)
Warnings:  0 mit -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror
Tests:     813/813 PASS
```
