# Code Cleanup Audit

> **Generated:** 2026-06-22
> **Purpose:** Record findings from a systematic code audit to prepare for repairs and cleanup.

## Priority Legend

| Priority | Meaning |
|----------|---------|
| 🔴 **Important** | Should fix — dead code, linkage issues, incomplete refactoring |
| 🟡 Cosmetic | Nice to fix, no runtime impact |
| ✅ Clean | Verified OK |

---

## 1. Dead Code

### 1A. Unused Functions in `gpio_control.h`

| # | Function | Line | Severity | Notes |
|---|----------|------|----------|-------|
| 1.1 | `reservedEthernetReason()` | 74 | 🔴 **Important** | Returns human-readable reason string for Ethernet-reserved pins, but never called anywhere |
| 1.2 | `isPinAvailableForOutput()` | 82 | 🔴 **Important** | Combines `isInputOnlyPin` + `isReservedEthernetPin` — never called; callers use the primitives directly |
| 1.3 | `pinsConflict()` | 90 | 🔴 **Important** | Simple equality check, but `main.cpp` uses its own inline logic in `outputsHaveDuplicateGpio()` |
| 1.4 | `enumerateChannelGpios()` template | 98 | 🔴 **Important** | Complex template walking routing options to collect GPIO pins — never called |

**Fix:** Remove all 4 functions (~55 lines).

### 1B. Redundant Macro `CHAN_TYPE_ANALOG_RGB`

| # | File | Line | Severity |
|---|------|------|----------|
| 1.5 | `include/output_control.h` | 24 | 🔴 **Important** |

```cpp
#define CHAN_TYPE_ANALOG_RGB OutputDefs::TYPE_ANALOG_RGB
```

Legacy backward-compat macro for only one type. No other type has a `CHAN_TYPE_` equivalent. Used in **10 places** across 4 files:

| File | Lines |
|------|-------|
| `src/main.cpp` | 526, 528, 530, 796, 954 |
| `include/config_rules.h` | 39, 45, 50 |
| `include/motion_control.h` | 50, 78 |
| `include/output_control.h` | 406, 24 (definition) |

**Fix:** Replace all `CHAN_TYPE_ANALOG_RGB` with `OutputDefs::TYPE_ANALOG_RGB`, remove the `#define`.

### 1C. Redundant `scoring_defs.h`

| # | File | Severity |
|---|------|----------|
| 1.6 | `include/scoring_defs.h` | 🔴 **Important** |

The file is a 30-line passthrough wrapper that re-exports every constant from `ScoringLimits` into a `ScoringDefs` namespace with zero added value:

```
scoring.h → scoring_defs.h → scoring_limits.h
```

Only consumed by `scoring.h`. Eliminate and inline directly.

**Fix:** Remove `scoring_defs.h`, move its constants directly into `scoring.h` or use `ScoringLimits` directly.

---

## 2. Linkage Issues

### 2A. `SEG_DIGITS` Internal vs External Linkage Mismatch

| # | File | Line | Severity |
|---|------|------|----------|
| 2.1 | `include/output_control.h` | 37 | 🔴 **Important** |
| 2.2 | `include/output_devices/tm1637.h` | 6 | 🔴 **Important** |

`output_control.h` defines:
```cpp
const uint8_t SEG_DIGITS[10] = { ... };
```

In C++, `const` at namespace scope gives **internal linkage** — each TU gets its own copy. But `tm1637.h` declares:
```cpp
extern const uint8_t SEG_DIGITS[10];
```

expecting **external linkage**. This works only because `tm1637.h` (via `seven_seg.h`) is always included together with `output_control.h`, so the `extern` binds to the internal copy in the same TU. Violates ODR — fragile.

**Fix:** Add `extern` to the definition in `output_control.h`, or move `SEG_DIGITS` to its own header.

---

## 3. Documentation Drift

### 3A. AGENTS.md Missing Files

AGENTS.md project structure section lists `include/` contents but omits:

| # | File | Purpose |
|---|------|---------|
| 3.1 | `include/rmt_dmx.h` | RMT DMX fallback driver |
| 3.2 | `include/scoring.h` | Main scoring logic |
| 3.3 | `include/scoring_defs.h` | Scoring constant re-exports |
| 3.4 | `include/motion_control.h` | Thin coordinator for output devices |
| 3.5 | `include/dfplayer_control.h` | DFPlayer MP3 controller |
| 3.6 | `include/funcgen_control.h` | Function generator controller |
| 3.7 | `include/config_rules.h` | Config/IP validation helpers |
| 3.8 | `include/espnow_control.h` | ESP-NOW master/slave |
| 3.9 | `include/ota_control.h` | OTA update handling |
| 3.10 | `include/recovery_control.h` | Recovery mode control |

### 3B. AGENTS.md Source IDs Stale

Line 120 lists sources `0=GPIO, 1=PCA9685, 2=MCP23017, 3=TCA9555, 4=PCF857x, 5=MCP4725 DAC` — missing sources `6=DAC7571` and `7=DAC7573`.

---

## 4. Redundant Includes

| # | File | Line | Severity | Notes |
|---|------|------|----------|-------|
| 4.1 | `include/motion_control.h` | 5 | 🟡 Cosmetic | `#include <Wire.h>` — already pulled in transitively via `output_control.h` → `i2c_devices/pca9685.h` |

---

## 5. Verified Clean

| Area | Status |
|------|--------|
| All 19 `type_interfaces/type_N.h` files present and properly referenced | ✅ Clean |
| No stale `#include` directives to deleted files | ✅ Clean |
| No `#if 0` / commented-out code blocks | ✅ Clean |
| All `output_devices/` files properly include needed headers | ✅ Clean |
| All I2C mutex operations properly protected (`i2cMutex`) | ✅ Clean |
| No stale references to old file paths (`dimmer_control.h`, etc.) | ✅ Clean |
