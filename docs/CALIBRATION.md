# Calibration — Phoenix Platform v108.0

## 5-Parameter Logistic (5PL) Model

### Mathematical Model

Phoenix uses the 5-Parameter Logistic (5PL) function to model the sigmoidal dose–response relationship between analyte concentration and measured signal.

**Dx365 convention (with G):**

```
f(x) = D + (A − D) / (1 + (x/C)^B)^G
```

**CalibrationService convention (with E):**

```
f(x) = D + (A − D) / (1 + (x/C)^B)^E
```

| Parameter | Symbol | Meaning | Typical Range |
|---|---|---|---|
| Minimum asymptote | A | Background signal at zero concentration | 0.01 – 1.0 |
| Hill slope | B | Steepness of the curve at inflection | 0.5 – 3.0 (positive = sandwich; negative = competitive) |
| Inflection point | C | EC50 — concentration at 50% response | Assay-specific |
| Maximum asymptote | D | Saturated signal at high concentration | 5.0 – 50.0 |
| Asymmetry factor | G / E | Curve asymmetry (1.0 = symmetric 4PL) | 0.1 – 10.0 |

### Typical 5PL Curve Shape

```
Signal (y)
    │
  D ┤ · · · · · · · · · · · · · · · · ·─────────────────
    │                               ·─
    │                           ··
    │                         ·
    │                       ·          ← steepest slope (Hill slope B)
    │                     ·
    │                   ·
    │                 ·
    │               ·                  ← inflection point C (EC50)
    │             ·
    │           ·
    │         ·
    │        ·
    │      ·
    │    ··
  A ┤ ──·· · · · · · · · · · · · · · ·
    └──────────────────────────────────────────── Concentration (x)
          C/10          C          C*10
```

For competitive assays (e.g., Vitamin D), the curve is inverted: B < 0, meaning higher concentration produces lower signal.

### Inverse 5PL — Signal → Concentration

The analytical inverse computes concentration from a measured signal value:

```
Step 1:  inner  = (A − D) / (y − D)
Step 2:  powered = inner^(1/G) − 1
Step 3:  x = C · powered^(1/B)
```

**Boundary conditions:**
- If |A − D| < ε: degenerate curve, return C
- If |y − D| < ε: signal at asymptote, return large value (1e6)
- If `inner ≤ 0`: out of range, return 0
- If `powered ≤ 0`: below detection, return 0

**Implementation** (`Dx365Algorithm.h:55–63`):

```cpp
float FivePL_G::inverse(float y) const {
    if (fabsf(y - D) < 1e-8f) return 1e6f;   // at max asymptote
    if (fabsf(A - D) < 1e-8f) return C;        // degenerate
    float inner = (A - D) / (y - D);
    if (inner <= 0.0f) return 0.0f;
    float powered = powf(inner, 1.0f / G) - 1.0f;
    if (powered <= 0.0f) return 0.0f;
    return C * powf(powered, 1.0f / B);
}
```

## Levenberg-Marquardt Fitting Algorithm

### Overview

The CalibrationService uses an embedded Levenberg-Marquardt (LM) solver to fit 5PL parameters to measured calibration data. This implementation operates without external linear algebra libraries (no Eigen, no LAPACK).

### Algorithm Description

```
Input:  N calibration points (concentration_i, signal_i), initial guess p₀
Output: Optimal parameters p* = [A, B, C, D, E] minimizing Σ(residuals²)

1. Initialize:
   - p = initial_guess (heuristic from data endpoints)
   - λ = 0.001 (damping factor)
   - max_iter = 200

2. For each iteration:
   a. Compute residuals r_i = signal_i − f(concentration_i, p)
   b. Compute Jacobian J (5 × N) via finite differences
   c. Form normal equations: (J^T·J + λ·I) · δ = J^T · r
   d. Solve for parameter update δ (5 × 5 system, Gauss elimination)
   e. Candidate: p_new = p + δ
   f. If cost(p_new) < cost(p):
      - Accept: p = p_new
      - Decrease damping: λ = λ / 10
   g. Else:
      - Reject update
      - Increase damping: λ = λ * 10

3. Convergence criteria (any of):
   - Residual change < 1e-8 (relative)
   - Parameter change < 1e-8 (relative)
   - Maximum iterations reached (200)

4. Compute R² = 1 − SS_res / SS_tot
```

### Lambda Strategy

| Phase | λ Range | Behavior |
|---|---|---|
| Early (gradient descent) | 10⁻³ – 10³ | Large λ → conservative steps toward minimum |
| Mid (transition) | 10⁻⁶ – 10⁻³ | Moderate λ → balanced convergence |
| Late (Gauss-Newton) | < 10⁻⁶ | Small λ → fast quadratic convergence near minimum |

### Convergence Requirements

| Criterion | Threshold | IEC 62304 Ref |
|---|---|---|
| Minimum R² | 0.990 | REQ-CAL-001 |
| Minimum calibration points | 4 | REQ-CAL-002 |
| Maximum iterations | 200 | Performance bound |
| Residual tolerance | 1e-8 | Numerical precision |

## Factory Calibration

### Purpose

Factory calibration curves serve as:
1. **Initial calibration** before first Color Chart calibration
2. **Plausibility check** for field calibrations
3. **Demo mode** reference curves
4. **QC acceptance criteria** bounds

### Reference Curves (8 Assays)

Source: Auker Verification 2026-01-09 (Dx365 vs reference instrument, serial dilution, 5 replicates per concentration).

| # | Assay Code | Assay Name | Technology | EC50 (C) | R² | Range | LOD | LOQ |
|---|---|---|---|---|---|---|---|---|
| 1 | CRP-COL | hs-CRP | Colorimetric | 25.0 mg/L | 0.994 | 0–200 mg/L | 0.5 | 1.0 |
| 2 | PROCAL-COL | Procalcitonin | Colorimetric | 2.0 ng/mL | 0.991 | 0–100 ng/mL | 0.02 | 0.05 |
| 3 | TROPO-COL | Cardiac Troponin I | Colorimetric | 0.5 ng/mL | 0.992 | 0–50 ng/mL | 0.006 | 0.01 |
| 4 | VITD-FLU | 25-OH Vitamin D | Immunofluorescence | 30.0 ng/mL | 0.996 | 0–150 ng/mL | 3.0 | 5.0 |
| 5 | FER-FLU | Ferritin | Immunofluorescence | 150.0 ng/mL | 0.993 | 0–1000 ng/mL | 2.0 | 5.0 |
| 6 | HBA1C-DRY | HbA1c | Dry Chemistry | 8.0 % | 0.998 | 3–15 % | 3.0 | 3.5 |
| 7 | GLUC-DRY | Glucose | Dry Chemistry | 200.0 mg/dL | 0.997 | 10–600 mg/dL | 10.0 | 20.0 |
| 8 | COAG-MFL | PT/INR | Microfluidics | 2.5 INR | 0.997 | 0.8–8.0 INR | 0.5 | 0.8 |

### Verification Points

Each factory curve includes 6–8 verification points measured on the reference instrument. Field calibrations are compared against these points using the `FactoryCalibrationRegistry::verifyAgainstFactory()` method:

```cpp
bool verified = factory.verifyAgainstFactory("CRP-COL", field_params);
// Returns true if all verification points are within tolerance
```

**Tolerance criteria:** Each point has an individual tolerance percentage (typically 8–25%). Tighter tolerances at mid-range, wider at extremes.

### Assay Technologies

| Technology | Detection Method | LED | Signal Source |
|---|---|---|---|
| Colorimetric | Gold nanoparticle LFT | White LED | T/C ratio from OV2686 camera |
| Immunofluorescence | UV excitation → emission | UV 365nm LED | Fluorescence intensity ratio |
| Dry Chemistry | Reflectance photometry | White LED | Reflectance change (Kubelka-Munk) |
| Microfluidics | Optical clot detection | White LED | Clotting time ratio |

## Field Calibration — Color Chart System

### Overview

The Igloo Pro uses a proprietary Color Chart reference plate (DXR.007.01) to calibrate the camera response before each measurement. This 11-strip cardboard plate compensates for optical drift, LED aging, and unit-to-unit variation.

### Color Chart Plate Specification

**Physical dimensions:** 8.75 × 45 mm, thickness 0.4–0.6 mm

```
 ┌──────────┐
 │          │  ← 13 mm top margin
 │──────────│
 │ ████████ │  Strip 0:  Black    (#000000) — Dark reference
 │──────────│  0.2mm gap
 │ ████████ │  Strip 1:  White    (#FFFFFF) — Full-scale reference
 │──────────│
 │ ████████ │  Strip 2:  Dark Gold(#755E05) — Gold NP reference
 │──────────│
 │ ████████ │  Strip 3:  Magenta  (#FF00FF) — R+B cross-talk
 │──────────│
 │ ████████ │  Strip 4:  Yellow   (#FFFF00) — R+G linearity
 │──────────│
 │ ████████ │  Strip 5:  Black    (#000000) — Drift check
 │──────────│
 │ ████████ │  Strip 6:  White    (#FFFFFF) — Drift check
 │──────────│
 │ ████████ │  Strip 7:  Blue     (#0000FF) — Blue sensitivity
 │──────────│
 │ ████████ │  Strip 8:  Green    (#00FF00) — Green sensitivity
 │──────────│
 │ ████████ │  Strip 9:  Red      (#FF0000) — Red / hemoglobin ref
 │──────────│
 │ ████████ │  Strip 10: White    (#FFFFFF) — Stability check
 │──────────│
 └──────────┘
   8.75mm wide, strips 1.8mm × 7mm
```

### 11-Strip Color Reference Table

| Strip | Color | Hex | Purpose |
|---|---|---|---|
| 0 | Black | `#000000` | Dark reference / baseline zero |
| 1 | White | `#FFFFFF` | Full-scale reference / gain |
| 2 | Dark Gold | `#755E05` | Mid-tone / LFT gold nanoparticle reference |
| 3 | Magenta | `#FF00FF` | Red+Blue channel cross-talk |
| 4 | Yellow | `#FFFF00` | Red+Green linearity / LFT background |
| 5 | Black | `#000000` | Repeated dark reference (drift check) |
| 6 | White | `#FFFFFF` | Repeated white reference (drift check) |
| 7 | Blue | `#0000FF` | Blue channel sensitivity |
| 8 | Green | `#00FF00` | Green channel sensitivity |
| 9 | Red | `#FF0000` | Red channel sensitivity / hemoglobin reference |
| 10 | White | `#FFFFFF` | Final white reference (stability check) |

### Calibration Pipeline

```
Step 1: Acquire              Step 2: Validate           Step 3: Compute
┌─────────────────┐         ┌─────────────────┐        ┌──────────────────┐
│ Read 11 strips  │────────►│ All strips      │───────►│ Dark ref (0,5)   │
│ from camera     │         │ present + valid  │        │ White ref (1,6,10)│
│ image           │         │                 │        │ Gain/offset       │
└─────────────────┘         └─────────────────┘        │ Gamma (strip 2)  │
                                                        │ RGB gains (7,8,9)│
                                                        │ Cross-talk (3,4) │
Step 4: Save                 Step 5: Verify             │ Drift check      │
┌─────────────────┐         ┌─────────────────┐        │ Linearity R²     │
│ Persist to NVS  │◄────────│ R² > 0.95       │◄───────┤                  │
│ (24h expiry)    │         │ Drift < 5%      │        └──────────────────┘
└─────────────────┘         │ Uniformity OK   │
                            └─────────────────┘
```

### Acceptance Criteria

| Criterion | Threshold | Action on Failure |
|---|---|---|
| All 11 strips readable | 100% | Reject plate, prompt re-insertion |
| Linearity R² | > 0.95 | Reject calibration |
| Dark drift (Black 1 vs Black 2) | < 5% | Warning; log drift event |
| White drift (max of W1/W2/W3) | < 5% | Warning; log drift event |
| Strip uniformity score | > 0.80 | Warning; may indicate damaged plate |
| Calibration age | < 24 hours | Require recalibration |

### NVS Storage

Calibration data is persisted to NVS with magic number `0x43434C31` ("CCL1"):

| Field | Size | Description |
|---|---|---|
| `magic` | 4 B | `0x43434C31` validation marker |
| `version` | 2 B | Serialization version (currently 1) |
| `intensity` | 20 B | Gain, offset, gamma, non-linearity coefficients |
| `white_balance` | 12 B | Per-channel (R/G/B) gain factors |
| `gold_np_reference` | 4 B | Gold nanoparticle reference intensity |
| `timestamp` | 4 B | Calibration time (Unix) |
| `expires_at` | 4 B | 24-hour expiry timestamp |

## Verified Assay Registry — 30 Assays

All 5PL coefficients extracted from real Dx365 MCP software projects. Data: 43 projects, 1594 sessions, 1266+ camera images.

### Clinical Inflammation (4 assays)

| ID | Name | LOINC | Signal | Incubation | Source |
|---|---|---|---|---|---|
| CRP | C-Reactive Protein | 1988-5 | T/C ratio | 600s | Untitled-2, 53 meas. |
| CRP-DE | CRP (German Cal.) | 16503-5 | TL only | 600s | DE market, 62 meas. |
| MxA | MxA Protein | MxA | T/C ratio | 600s | Untitled-2, viral marker |
| PCT | Procalcitonin | 75241-0 | TL only | 600s | Bacterial sepsis, 18 meas. |

### Cardiac (3 assays)

| ID | Name | LOINC | Signal | Incubation | Source |
|---|---|---|---|---|---|
| CTNI | Cardiac Troponin I | 14723-1 | TL only | 600s | Acute MI, 30 meas. |
| NT-proBNP | NT-proBNP | 27100-7 | TL only | 600s | Getein, 32 meas. |
| D-DIMER | D-Dimer (DDU) | 91556-1 | TL only | 600s | Coagulation, 31 meas. |

### Hematology / Iron (3 assays)

| ID | Name | LOINC | Signal | Incubation | Source |
|---|---|---|---|---|---|
| FERR | Ferritin | FERRITIN | TL only | 600s | FERRDENEME2, 48 meas. |
| FER-DE | Ferritin (German Cal.) | 24373-3 | TL only | 600s | DE market, 27 meas. |
| HBA1C | HbA1c | 4548-4 | TL only | 600s | Diabetes, 30 meas. |

### Sepsis (1 assay)

| ID | Name | LOINC | Signal | Incubation | Source |
|---|---|---|---|---|---|
| IL6 | Interleukin-6 | 49919-4 | TL only | 600s | Sepsis/inflammation, 23 meas. |

### Infectious Disease (2 assays)

| ID | Name | LOINC | Signal | Incubation | Source |
|---|---|---|---|---|---|
| DENGUE | Dengue NS1 Ag | 91064-6 | T/C ratio | 600s | Tropical, 148 meas. |
| SAA | Serum Amyloid A | SAA | T/C ratio | 600s | Orient, 42 meas. R²=0.994 |

### Thyroid (2 assays)

| ID | Name | LOINC | Signal | Incubation | Source |
|---|---|---|---|---|---|
| TSH | Thyroid Stimulating Hormone | 3014-8 | T/C ratio | 900s | Getein, 25 meas. |
| T4 | Thyroxine (T4) | 83120-6 | TL only | 900s | Getein, 33 meas. |

### Immunology (2 assays)

| ID | Name | LOINC | Signal | Incubation | Source |
|---|---|---|---|---|---|
| IgE | IgE Antibody | 51651-8 | T/C ratio | 600s | MCP verified, 44 meas. |
| S-Ab | S Antibody | 1317-7 | T/C ratio | 600s | 31 meas. |

### Gastroenterology (1 assay)

| ID | Name | LOINC | Signal | Incubation | Source |
|---|---|---|---|---|---|
| PEP | Pepsinogen | 2739-1 | T/C ratio | 600s | GI/gastric, 64 meas. |

### Veterinary (5 assays)

| ID | Name | LOINC | Signal | Incubation | Source |
|---|---|---|---|---|---|
| cPL | Canine Pancreas Lipase | 48497-2 | TL only | 600s | Getein, 36 meas. |
| fPL | Feline Pancreas Lipase | 23726-3 | T/C ratio | 600s | Getein, 32 meas. |
| fSAA | Feline Serum Amyloid A | 25585-1 | T/C ratio | 300s | Vet inflammation, 25 meas. |
| CPV | Canine Parvovirus Ag | 23793-3 | TL only | 600s | Vet diagnostic, 32 meas. |
| SDMA | SDMA (Dimethylarginine) | 80981-4 | T/C ratio | 600s | Kidney biomarker, 44 meas. |

### Drug Panel (7 assays)

| ID | Name | LOINC | Signal | Incubation | Source |
|---|---|---|---|---|---|
| COC | Cocaine | 3398-5 | TL only | — | GC704 multi-drug |
| OPI | Opiates | 48961-7 | TL only | — | GC704 multi-drug |
| METH | Methamphetamine | 3780-4 | TL only | — | GC704 multi-drug |
| AMP | Amphetamine | 19346-6 | TL only | — | GC704 multi-drug |
| THC | THC (Cannabis) | 3530-3 | T/C ratio | — | GC704 multi-drug |
| BZO | Benzodiazepines | 9428-4 | T/C ratio | — | GC704 multi-drug |
| BUP | Buprenorphine | 3415-7 | TL only | — | GC704 multi-drug, 47 meas. |

### Cassette Manufacturers

| Code | Manufacturer | Markets |
|---|---|---|
| GETEIN | Getein Biotech | Asia, Vet |
| HIGHTOP | Hightop Biotech | EU |
| MAXHEALTH | Maxhealth Biotech | Asia |
| ISIA | iSIA | EU |
| AUKER | Auker | EU, DE |
| ORIENT | Orient Gene | Asia, Clinical |
| CHINA_GENERIC | Generic (China) | OEM |

### Signal Types

| Type | Formula | Usage |
|---|---|---|
| `TL_DIV_CL` | Test Line / Control Line | Most common — compensates for sample volume variation |
| `TL_ONLY` | Test Line intensity | Used when control line is not applicable |
| `CL_ONLY` | Control Line only | Quality control only |
| `TL_MINUS_CL` | Test Line − Control Line | Differential measurement |

Each assay in the registry carries three 5PL curves:
- **`test_5pl`** — Maps concentration → test line peak value
- **`control_5pl`** — Maps concentration → control line peak value
- **`div_5pl`** — Maps concentration → T/C ratio (primary calibration curve)
