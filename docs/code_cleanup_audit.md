# Code Cleanup Audit

> **Updated:** 2026-06-23
> **Purpose:** Record code cleanup audit findings and repair status.

## Completed Cleanup

| # | Item | Status | Notes |
|---|------|--------|-------|
| 1 | Remove unused GPIO helpers | ✅ Completed | Deleted `reservedEthernetReason()`, `isPinAvailableForOutput()`, `pinsConflict()`, and `enumerateChannelGpios()` from `include/gpio_control.h`. |
| 2 | Remove legacy `CHAN_TYPE_ANALOG_RGB` macro | ✅ Completed | Replaced all call sites with `OutputDefs::TYPE_ANALOG_RGB` and deleted the macro from `include/output_control.h`. |
| 3 | Remove redundant `scoring_defs.h` | ✅ Completed | `include/scoring.h` now uses `ScoringLimits` directly; `include/scoring_defs.h` deleted. |
| 4 | Fix `SEG_DIGITS` linkage | ✅ Completed | Moved segment digit lookup table to `output_devices/seven_seg_digits.h` and removed the fragile `extern` declaration. |
| 5 | Remove redundant include | ✅ Completed | Removed unused `#include <Wire.h>` from `include/motion_control.h`. |
| 6 | Sync AGENTS.md project structure | ✅ Completed | Added missing headers and updated Source IDs `0..7`. |
| 7 | Sync GPIO documentation | ✅ Completed | `docs/domain_model.md` no longer documents deleted GPIO helper functions. |

## Verified Clean

| Area | Status |
|------|--------|
| All 19 `type_interfaces/type_N.h` files present and referenced | ✅ Clean |
| No stale `#include` directives to deleted wrapper files | ✅ Clean |
| No `#if 0` / disabled code blocks found in firmware headers/source | ✅ Clean |
| `output_devices/`, `i2c_devices/`, and `lighting_protocols/` paths are current | ✅ Clean |
| Build after cleanup | ✅ Clean (`platformio run`) |

## Follow-Up Audit Targets

These are broader design cleanups, not required for the current pass:

| # | Area | Notes |
|---|------|-------|
| F1 | Raw type IDs in validation | `src/main.cpp` still has many raw type comparisons like `type == 6`; replace gradually with `OutputDefs::TYPE_*`. |
| F2 | Web UI/C++ scoring parity maintenance | C++ and JS scoring still duplicate logic; future work could generate JS constants from firmware definitions or serve via API. |
| F3 | Large HTTP route lambdas | `src/main.cpp` route handlers are still large; extract named handlers when touching those endpoints. |
