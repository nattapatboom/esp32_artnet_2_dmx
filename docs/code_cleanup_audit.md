# Code Cleanup Audit — June 2026

> **Purpose:** Document all code quality issues, dead code, duplication, and
> inconsistencies found during a thorough audit. **Do NOT fix anything yet.**
> This doc is the backlog — each item will be fixed incrementally in its own
> commit after discussion.

---

## Legend

| Category | Meaning |
|----------|---------|
| HARDWARE | Can cause incorrect physical output or hardware damage |
| CORRECTNESS | Logic bug that can cause wrong runtime behaviour |
| DUPLICATION | Same logic copy-pasted across C++ and JS (drift risk) |
| DEAD | Unused variables, functions, fields (confusion + bloat) |
| MAGIC | Raw numbers instead of named constants (readability + drift) |
| DESIGN | Structural problem (redundant struct, wrong abstraction) |
| STALE | Comment/code that no longer matches reality |

---

## A. Hardware-Safety / Correctness Issues

### A1. `dimmer_tick` uint16 wrap without modulo
- **File:** `include/output_devices/dimmer.h`
- `dimmer_tick` increments freely; when it wraps past 65535, all dimmer timing
  comparisons break until the counter wraps back through zero.
- **Fix:** Add `&= (DIMMER_TICK_MAX-1)` modulo on increment, or use
  `uint32_t` and accept the small overhead.
- **Status:** ✅ Fixed in `dec8ef2`. Added `DIMMER_TICK_MAX=512` safety cap:
  tick clamps and ISR returns early if ZC signal is lost longer than a half-cycle.

### A2. Dimmer timer re-init leaks hardware timer
- **File:** `src/main.cpp` — route that calls `dimmerCtrl.begin()` on hot config
  reload (when implemented) or restart
- If `timerBegin()` is called without `timerEnd()` first, the ESP32 timer
  resource is leaked. Currently not triggered because outputs always reboot,
  but a future hot-reload path would hit this.
- **Fix:** Call `timerEnd()` before re-initializing dimmer timer.
- **Status:** ✅ Fixed in `dec8ef2`. Added `if (dimmerTimer) { timerEnd(dimmerTimer); dimmerTimer = nullptr; }` guard in `DimmerControl::begin()`.

### A3. ArtPollReply byte-order (big-endian fields)
- **File:** `include/artnet_control.h`
- The struct `ArtPollReplyPacket` is filled field-by-field; fields like IP
  address, port, universe are stored in network byte order per Art-Net spec,
  but some assignments write host-order values directly into the struct without
  `htons()`/`htonl()`.
- **Fix:** Audit all numeric fields >1 byte and add `htons()`/`htonl()`.
- **Status:** ❌ **False positive.** Art-Net protocol uses **little-endian**
  for all multi-byte fields (confirmed in spec). ESP32 is also little-endian.
  No conversion needed. No code change required.

### A4. `reloadPeersPending` is plain `bool`, not `std::atomic<bool>`
- **File:** `src/main.cpp` (and referenced in `espnow_control.h`)
- Set from HTTP handler (Core 0) and read in `networkTask` (Core 0), so
  currently safe; but the intent is cross-core signalling. If ever read from
  Core 1, it would race.
- **Fix:** Change to `std::atomic<bool>` for documentation and future safety.
- **Status:** ✅ Fixed in `ebbe98d`.

### A5. ESP-NOW queue-send return code not checked
- **File:** `include/espnow_control.h`
- `esp_now_send()` return value is ignored in some paths. If ESP-NOW radio is
  busy, the packet is silently dropped.
- **Fix:** Check return, optionally increment a dropped-counter.
- **Status:** ❌ **False positive.** `sendDmx()` at line 250 already checks
  `result != ESP_OK` with rate-limited logging.

---

## B. Dead / Unused Code

### B1. `validateIp4()` in C++
- **File:** `include/config_rules.h:29-51`
- Defined but never called from any C++ file. IP validation is done in JS.
- **Fix:** Remove dead function.
- **Status:** ❌ **False positive.** Called at `main.cpp:671` (settings POST handler). Keep.

### B2. `getMaxValue()` in Web UI
- **File:** `web/index.html` — search for `getMaxValue`
- Unused helper.
- **Fix:** Remove.

### B3. `showAlert()` / `hideAlert()` in Web UI
- **File:** `web/index.html` — search for `showAlert`
- Appears to be dead; all save alerts use their own inline `setBackupStatus()`.
- **Fix:** Remove or mark.

### B4. `newRxData` atomic flag
- **File:** `src/main.cpp` — `std::atomic<bool> newRxData(false);`
- Set but never consumed anywhere in `outputTask` or `networkTask`.
- **Fix:** Remove flag.
- **Status:** ✅ `ArtNetControl::newRxData` removed in `ebbe98d`.
  `EspNowControl::newRxData` is actively used (kept).

### B5. `channelLocked` / `currentScanChannel` in Web UI
- **File:** `web/index.html` — search for these variables
- Appear to be leftovers from an earlier ESP-NOW channel scan implementation.
- **Fix:** Remove dead variables.

### B6. `ScoringDefs::CHANNEL_CPU_US` is now redundant
- **File:** `include/scoring_defs.h` — already replaced by `OutputDefs::baseCpuUs()`
  but old array is removed. The `channelCpuUs()` inline proxy remains.
- **Fix:** Remove proxy `channelCpuUs()` and call `OutputDefs::baseCpuUs()` directly.

### B7. `displayAddressValid()` in JS
- **File:** `web/index.html` — defined but may be dead if validation moved to
  C++ only.
- **Fix:** Check usage and remove if unused.
- **Status:** ❌ **False positive.** Used at `index.html:1117` in setting validation. Keep.

---

## C. Duplication (C++ ↔ JS Drift Risk)

### C1. Hardware counting (`estimateHardware` / `channelHardware`)
- `include/scoring.h` (pin-loop based, no switch) and `web/js/scoring.js` (still switch-based)
- C++ side is now table-driven via `OUTPUT_MODES` pin loop; JS side still has parallel switch statements.
- **Future fix:** Generate JS from C++ definition, or serve via API.

### C2. I2C write counting (`i2cWritesForChannel` / `i2cWriteCount`)
- `include/scoring.h` (pin-loop based) and `web/js/scoring.js` (switch-based)
- Same pattern as C1 — C++ table-driven, JS still has parallel switch.

### C3. RAM buffer calculation (`dmxBufferRamForChannel` / `dmxBufferRam`)
- `include/scoring.h` (pin-loop based) and `web/js/scoring.js` (switch-based)
- Same pattern as C1/C2.

### C4. GPIO lists in three places
- `web/js/_gpio.js`: `OUTPUT_GPIOS`, `INPUT_GPIOS`, `INPUT_ONLY_GPIOS`, `PIN_GPIOS`
- `src/main.cpp` (validation): hardcoded RMII forbidden list, input-only check
- If the board changes or a pin is reallocated, these drift.
- **Future fix:** Single source of truth (target header) exposed via API.

### C5. `dmxValueByteCount` / `getValueByteCount`
- `include/output_control.h` defines `dmxValueByteCount(resolution)`.
- `web/js/_gpio.js` defines `valueByteCount(resolution)` — same logic.
- **Fix:** One JS function is fine (no C++ call needed from JS), but verify
  they produce identical results.

---

## D. Magic Numbers (Raw Type IDs, Hardcoded Constants)

### D1. Raw `0..18` type IDs everywhere
- `include/scoring.h`, `include/output_control.h`, `include/output_devices/` (all 17 per-type files),
  `web/index.html` — dozens of `case 3:`, `if(t===7)`, `type == 14`.
- **Fix:** Add `constexpr` type constants:
  ```cpp
  constexpr uint8_t TYPE_DIMMER   = 0;
  constexpr uint8_t TYPE_DMX      = 1;
  // ... etc.
  ```
  Then replace every raw integer in C++.
- **Status:** ✅ Constants added in `output_defs.h` in `ebbe98d`. Used in
  `OUTPUT_MODES[]`. Remaining raw type IDs across codebase still need
  replacement (future P2 work).

### D2. Hardcoded `8` for RMT max, `16` for LEDC max, `2` for UART
- `web/index.html:1891`: `hwMax:{ledc:16,rmt:8,uart:2,dac:2,timer:4}`
- `include/scoring_defs.h:34-40`: `HARDWARE_LIMITS` struct
- **Fix:** Drive JS from C++ limits via API.

### D3. Hardcoded universe count in ArtPollReply
- `include/artnet_control.h`: ArtPollReply assumes 4 universes max. This
  aligns with current domain but is not configurable.
- **Fix:** Derive from channel configuration at ArtPoll time.

---

## E. Design & Structural Issues

### E1. `OutputDefs::HardwareCost` vs `ScoringDefs::HardwareLimits`
- `include/output_defs.h` has `HardwareCost` (per-mode shape).
- `include/scoring_defs.h` has `HardwareLimits` (chip maximums).
- They share the same field names (`ledc`, `rmt`, `uart`, `dac`, `timer`).
- **Fix:** Consider unifying or clearly naming to avoid confusion.

### E2. `modeCost()` helper is nice but unused for `extraRamBytes` / `hardware` in C++ scoring
- `include/scoring_defs.h:channelCpuUs()` only reads `cpuUs`.
- `include/scoring.h:estimateChannelCost()` adds `extraRamBytes` via
  hardcoded type checks rather than reading from `OutputDefs::modeDef()`.
- **Fix:** Migrate `estimateChannelCost()` to read
  `OutputDefs::modeDef(ch.type, ch.mc_mode)->cost` instead of separate
  type checks.

### E3. Web UI `OUTPUT_DEFS` is hardcoded JS object
- `web/index.html:1266+`
- If C++ definitions change, JS must be manually updated.
- **Fix:** Serve `/api/output-defs` from firmware and fetch on page load.

### E4. `include/output_control.h` `OutputChannel` struct mixes persisted + runtime fields
- Persisted fields (type, source, pin, etc.) are interleaved with runtime-only
  fields (dmxBuffer, pixelStrip, etc.) in the same struct.
- **Fix:** Split into `OutputChannelConfig` (persisted) and
  `OutputChannelRuntime` (pointers/handles).

### E5. `inline` functions in `.h` files — no `.cpp` separation
- All functions in `scoring.h`, `config_rules.h`, `output_defs.h` are
  `inline` or `constexpr` in headers. This works but increases compile time
  and makes it harder to find definitions.
- **Fix:** For larger functions (e.g., `estimateChannelCost`), move to a
  `.cpp` file and keep only declaration in `.h`.

### E6. Web UI `renderPinRows()` is >180 lines
- The function handles all 19 types with nested conditionals.
- **Fix:** Break into per-type or per-mode rendering strategies.

### E7. `src/main.cpp` HTTP route lambdas are huge
- `/api/outputs` POST, `/api/settings` POST — each is a ~50-line lambda.
- **Fix:** Extract into named functions.

---

## F. Stale / Inconsistent Comments & Names

### F1. `CHAN_TYPE_*` macros vs `TYPE_*` consistency
- `include/output_control.h` has some `CHAN_TYPE_ANALOG_RGB` macros.
- `include/output_defs.h` uses raw numbers.
- **Fix:** Align naming; prefer `OutputDefs::TYPE_ANALOG_RGB`.

### F2. Comment references to old scoring system
- `include/scoring.h` still references "Resource Score" / "Compute Score" in
  some comments; these were replaced by 3 independent budgets.
- **Fix:** Update comments to match current system.

### F3. `docs/domain_model.md` — stale ADR011 description
- ADR011 says "known drift" but it was resolved.
- **Fix:** Mark ADR011 as resolved in the doc.
- **Status:** ✅ Already resolved. `domain_model.md` ADR011 title says "Resolved" and the summary list confirms it.

---

## G. Configuration & Build

### G1. `web_pages.h` generation not auto-triggered
- `include/web_pages.h` is committed. If someone edits `web/index.html` and
  forgets to run `build_web.py`, the embedded UI is stale.
- **Fix:** Consider a git pre-commit hook.

### G2. No version check between Web UI and firmware
- If user uploads old `web_pages.h` with new firmware (or vice versa), API
  calls may fail silently.
- **Fix:** Add API version field and show warning in UI.

---

## Priority Index

| Priority | Items |
|----------|-------|
| P0 — correctness | A1 ✅, A2 ✅, A3 ❌ (false positive) |
| P1 — high | A4 ✅, A5 ❌ (false positive), B4 ✅ (partial), D1 ✅ (constants + aggregate usage), B6 ✅ (removed proxy) |
| P2 — medium | B1 ❌ (false positive), B2 ❌ (already gone), B3 ❌ (used 17x), B5 ❌ (already gone), B7 ❌ (false positive), C4 🔲 (needs API), E2 ✅ (all cost fields now from OutputDefs), E3 🔲 (future) |
| P3 — low | C1 ✅ (C++ switch → pin loop, JS still parallel), C2 ✅ (same), C3 ✅ (same), C5 🔲, D2 🔲, D3 🔲, E1 🔲, E4 🔲, E5 🔲, E6 🔲, E7 🔲, F1 🔲, F2 🔲, F3 🔲, G1 🔲, G2 🔲, **Web UI split ✅** (multi-file shell + parts + JS modules, build_web.py updated) |

---

> **Process:** Pick an item, open a commit, fix it, test, commit, repeat.
> Do NOT batch unrelated fixes in a single commit.
