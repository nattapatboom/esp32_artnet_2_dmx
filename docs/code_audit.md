# Code Audit Report

Generated: 2026-06-23
Scope: Full firmware logic audit — validation, scoring, routing, protocol, config, GPIO

## Priority Legend

| Level | Meaning |
|-------|---------|
| ★★★ | Crash / data-loss / silent misbehavior |
| ★★☆ | Functional bug in well-defined scenario |
| ★☆☆ | Edge case / defense-in-depth gap |
| ℹ️ | Maintainability / style concern |

---

## 1. Protocol Handling

### ~~1.1 sACN: All packet offsets shifted by +16 bytes~~

**FALSE POSITIVE** — After verification against the E1.31 standard, all SACN_* offset constants are correct:

| Constant | Code | E1.31 Spec | Match? |
|----------|------|------------|--------|
| SACN_ROOT_VECTOR | 18 | 18 (after ACN transport header) | ✅ |
| SACN_CID | 22 | 22 | ✅ |
| SACN_FRAMING_VECTOR | 40 | 40 | ✅ |
| SACN_PRIORITY | 108 | 108 | ✅ |
| SACN_UNIVERSE | 113 | 113 | ✅ |
| SACN_DMP_VECTOR | 117 | 117 | ✅ |
| SACN_DMP_ADDR_TYPE | 118 | 118 | ✅ |
| SACN_DMP_PROP_COUNT | 123 | 123 | ✅ |
| SACN_DMP_DATA | 125 | 125 | ✅ |

The E1.31 packet has a 16-byte ACN transport header (Preamble + Post-AM + ACN ID) before the Root PDU. The Root Vector is at offset 18, not 2. The audit agent incorrectly calculated offsets from the absolute start of the packet instead of accounting for the ACN transport layer.

---

### ~~1.2 Art-Net: ArtPollReply sent to wrong UDP port~~

**FALSE POSITIVE** — Art-Net specification requires ArtPollReply to be sent to the controller on the **standard Art-Net port** (6454), not to the controller's ephemeral source port. Current code `udp.beginPacket(udp.remoteIP(), sysCfg.artnet_port)` is correct per the Art-Net protocol standard. `udp.remotePort()` would be wrong.

---

### ~~1.3 ★★☆ `swapBuffers()` cross-core data race~~

**RESOLVED** — `swapBuffers()` now uses `swapMutex` (`xSemaphoreTake`/`xSemaphoreGive`) to protect the pointer swap across cores.

---

### 1.4 ★★☆ Cross-core atomics missing

**Files:** `main.cpp:54-55`, `output_control.h:33-34`, `sacn_control.h:239`, `artnet_control.h:111`

`lastDmxUpdateTime` and `systemActive` are plain `unsigned long`/`bool` accessed from both cores without `std::atomic`. On 32-bit Xtensa, aligned reads/writes are likely atomic, but compiler may reorder.

---

### 1.5 ★★☆ DMX frame timeout not enforced

**Contradiction:** `docs/domain_model.md` says "DMX Frame Cycle must not exceed 50ms" but code never checks `lastDmxUpdateTime`. No protective action is taken when frames stop arriving (Hold Last State is unimplemented).

---

### ~~1.6 RMT DMX: `malloc(20600)` failure silently ignored~~

**RESOLVED** — `RmtDmxDriver::begin()` now refuses to start without a buffer, `ready()` reports only successfully initialized drivers, and DMX RMT fallback deletes the driver without consuming an RMT slot when allocation/init fails.

Previous finding retained for audit history:

**File:** `include/output_devices/rmt_dmx.h:21`

`rmt_buffer` allocation is never null-checked in constructor. `send()` does check but returns silently. RMT channel is wasted on a dead DMX output.

---

### 1.7 ★☆☆ ESP-NOW callback races with foreground

**File:** `include/espnow_control.h:264-313`

`onDataRecv` callback writes `lastPacketRecvTime` and `channelLocked`. Foreground `loop()` reads them. No atomic or barrier. Slave could prematurely restart channel scanning.

---

### 1.8 ℹ️ Art-Net `lastSequence[]` declared but never used

**File:** `include/lighting_protocols/artnet_control.h:20`

Remove or implement sequence validation.

---

## 2. Output Routing & Setup

### ~~2.1 ★★★ 7-segment LEDC index corruption~~

**RESOLVED** — `sevenSegSetup()` now tracks `usedLedc` as the number of LEDC channels actually allocated, then sets `ledcIdx = baseChan + usedLedc` instead of blindly writing `baseChan + numSeg`.

---

### ~~2.2 ★★★ Stepper array bounds overflow~~

**RESOLVED** — `stepperSetup()` now checks `if (stepperCount >= 8) return;` before accessing the array.

---

### ~~2.3 ★★☆ No GPIO pin validation in setup functions~~

**RESOLVED** — All GPIO-routed setup functions now guard with `if (ch.pin == 255) return;` at the top (buzzer, dmx, funcgen, led_strip, motor, pwm_dac, relay, servo, single_led, solenoid, stepper, smoke_shooter). `seven_seg.h` relies on per-segment pin validation in `segmentGpio()`/`setupSegmentOutput()` which already check for pin 255.

---

### ~~2.4 ★★☆ Analog RGB ignores `mc_resolution`~~

**RESOLVED** — `analog_rgb.h` now uses `ledcResolution(ch)` for `ledcSetup()` and scales output values by `getMaxValue(ch.mc_resolution) / 255`.

---

### ~~2.5 ★★☆ Motor update division by zero~~

**RESOLVED** — `motorUpdate()`, `servoUpdate()`, `singleLedUpdate()`, and `stepperUpdate()` all check `if (max_val == 0) return;` before division.

---

### ~~2.6 ★★☆ PCA9685 frequency override conflict~~

**RESOLVED** — Per-type setups no longer call `setFrequency()` for `ch.pca_addr` (handled by `setupChannels()`). For secondary PCA addresses (`pin2_addr`, `pin3_addr`, `seg_addrs[]`), the shared frequency is obtained via `outputCtrl.sharedPcaFrequency()` instead of `ch.mc_freq`.

---

### ~~2.7 ★★☆ `segmentGpio()` returns invalid pin~~

**RESOLVED** — Added `return (gpio > 39) ? 255 : gpio;` guard after computing the pin value.

---

### ~~2.8 DFPlayer / FuncGen `new` allocation never checked~~

**RESOLVED** — DFPlayer and Function Generator setup now use nothrow allocation and skip setup with a Serial error if allocation fails.

Previous finding retained for audit history:

**File:** `include/output_control.h:965`

`ch.dfPlayer = new DFPlayerController();` — returns nullptr on OOM. Same for `FuncGenController`.

---

### ~~2.9 `RmtDmxDriver::begin()` ignores return value~~

**RESOLVED** — `RmtDmxDriver` now tracks successful `rmt_driver_install()` and `dmxSetup()` only keeps/increments RMT fallback allocation when `ready()` is true.

Previous finding retained for audit history:

**File:** `include/output_devices/rmt_dmx.h:41`

`rmt_driver_install(channel, 0, 0)` return value ignored. If channel is already in use, subsequent `send()` operates on uninstalled channel.

---

### ~~2.10 ★☆☆ `dimmer.h` ISR accesses non-volatile state~~

**RESOLVED** — `DimmerCh::dmxVal` changed from `uint8_t**` to `uint8_t* volatile*` to prevent compiler optimization of repeated reads in the ISR.

---

### 2.11 ★☆☆ `dmxSetup()` double-free risk

**File:** `include/output_control.h:960`

`dmx_driver_install` return value ignored after `dmx_driver_delete`. If install fails, UART state is undefined.

---

## 3. Configuration Save/Load

### ~~3.1 ★★★ NVS save return values all ignored~~

**RESOLVED** — `saveConfig()` now checks `prefs.begin()` return value, reports `false` on failure, and all callers (settings POST, import, factory-reset) check the return and send 500.

---

### ~~3.2 ★★★ LittleFS write failure on migration silently discarded~~

**RESOLVED** — `saveChannels()` now returns `bool`, and the migration/default-creation callers check the return value with a `CRITICAL` log message on failure.

---

### ~~3.3 `mc_resolution` has no default initializer~~

**RESOLVED** — `OutputChannel` now initializes `mc_resolution` and the neighboring motion/pin fields (`pin2`, `pin3`, `mc_freq`, `mc_mode`, deadband, invert/brake, pulse limits, steps per rev). Default-constructed fallback channels no longer carry uninitialized motion state.

---

Previous finding retained for audit history:

**File:** `include/output_control.h:151`

All other struct fields have `= value` defaults. `mc_resolution` is uninitialized on default construction.

---

### ~~3.4 `pin` JSON default (4) != struct default (255)~~

**RESOLVED** — `loadChannels()` now defaults missing `pin` to `255`, matching the struct default and preventing accidental GPIO4 activation.

Previous finding retained for audit history:

**File:** `include/output_control.h:597`

```cpp
ch.pin = item["pin"] | 4;  // JSON: fallback to GPIO4
```

Struct default is 255. If JSON is missing `pin`, channel activates on unexpected GPIO.

---

### ~~3.5 ★★☆ `ip4Valid()` accepts empty string~~

**RESOLVED** — `ip4Valid()` now returns `false` for empty strings.

---

### ~~3.6 ★★☆ Factory reset reports success on write failure~~

**RESOLVED** — Factory reset handler now checks `prefs.begin()` return, validates LittleFS file open, and returns 500 on failure instead of 200.

---

### ~~3.7 ★☆☆ `espnow_channel` range not validated on load~~

**RESOLVED** — `loadConfig()` now clamps `espnow_channel` to `0..13` after reading from NVS.

---

### ~~3.8 ℹ️ Duplicate inline validation in settings handler~~

**RESOLVED** — Removed the duplicate validation block (pin overlaps, display address, protocol enable/disable, Master auto-channel) from the settings POST handler. All checks now live in `validateSettingsAndOutputs()` only.

---

## 4. Validation Gaps

### ~~4.1 DMX start_address not validated in C++~~

**RESOLVED** — `validateOutputJson()` now validates `start_address` is `1..512` and rejects channels whose required DMX byte range would exceed universe channel 512, except modes whose `OutputModeDef` metadata declares `startAtFirstUniverse=true`. LED strips and DMX outputs set that capability in `OUTPUT_MODES[]`.

---

Previous finding retained for audit history:

**Missing server-side: `validateOutputJson()` never checks `start_address`**

JS checks at `outputs.js:826-833`: `start_address + byteCount - 1 <= 512`. C++ has NO check. Direct HTTP POST to `/api/outputs` with `start_address=999` passes. Runtime DMX buffer access is out-of-bounds.

---

### 4.2 ★☆☆ I2C speed validation defined but never called

Both C++ (`network_protocol.h:170-172`) and JS (`network_protocol.js:123-125`) define `i2cSpeedValid()` but neither calls it.

---

### 4.3 ★☆☆ LED count, universe range not validated server-side

HTML has `min/max` attributes but C++ `validateOutputJson()` never checks `led_count` or `start_universe`.

---

### 4.4 ★☆☆ mDNS name length not validated on either side

`MDNS_NAME_MAX_LEN = 31` but no code enforces it.

---

## 5. Scoring Parity

### ~~5.1 ★★☆ Type 6 Mode 0: JS overcounts LEDC~~

**RESOLVED** — JS `channelHardware()` corrected: DIR pin LEDC count changed from `(mc_mode||0)===0` to `(mc_mode||0)!==0`.

---

### 5.2 ★★☆ I2C write count differs (multiple types)

| Type | C++ | JS | Impact |
|------|-----|----|--------|
| 5 (Analog RGB, mode<4) | Counts pin4 | Skips pin4 | JS undercounts by 1 |
| 6 (Motor, main-I2C) | Per-pin source | Blind n writes | JS overcounts |
| 7 (Stepper) | Counts pin4 (HOME) | Skips pin4 | JS undercounts by 1 |
| 12/13 (7-seg) | Per-pin iteration | Multiply-by-n heuristic | Structure mismatch |

---

### 5.3 ℹ️ RAM limit: C++ = 65535, JS = 48000

**Known intentional difference**: C++ uses dynamic `freeHeap - 150KB`, JS uses conservative 48KB pre-check. Documented in `resource_calculator.md`.

---

## 6. GPIO/Pin Validation

### ~~6.1 ★★☆ JS `outputGpios()` skips `seg_pins`~~

**RESOLVED** — `outputGpios()` in `_gpio.js` now iterates `seg_pins`/`seg_sources` for types 12/13 and includes GPIO-routed segment pins.

---

### 6.2 ★☆☆ System pin options hardcoded in HTML

**File:** `web/parts/pane-network.html`

Status LED, ZC, I2C SDA/SCL dropdown options are hardcoded, not generated from `gpio_control.h`. If `OUTPUT_GPIO_PINS` changes, system pin lists drift.

---

### ~~6.3 ★☆☆ Reserved-pin list duplicated in `main.cpp`~~

**RESOLVED** — `outputsUseForbiddenGpio()` now uses `GpioControl::isReservedEthernetPin()` and `GpioControl::isInputOnlyPin()` instead of hardcoded pin lists.

---

### 6.4 ✅ Verified correct

| Check | C++ | JS |
|-------|-----|----|
| Pin lists match (auto-generated) | ✅ | ✅ |
| Ethernet reserved pins blocked | ✅ | ✅ |
| Input-only pins (34-39) blocked | ✅ | ✅ |
| GPIO12 strapping warning-only | ✅ | ✅ |
| I2C overlap checks | ✅ | ✅ |
| Duplicate GPIO detection | ✅ | ✅ |
| Duplicate expander channel detection | ✅ | ✅ |

---

## 7. Summary by Priority

All audit items are now **RESOLVED**.

### ★☆☆ Medium priority

| # | Issue | File | Status |
|---|-------|------|--------|
| 1.4 | Missing atomics (lastDmxUpdateTime, systemActive) | `main.cpp` | ✅ Resolved (already `std::atomic`) |
| 1.5 | DMX frame timeout not enforced | `src/main.cpp` | ✅ Resolved (timeout check in `outputTask`) |
| 1.7 | ESP-NOW callback races with foreground | `espnow_control.h` | ✅ Resolved (`channelLocked`/`lastPacketRecvTime` now `std::atomic`) |
| 2.11 | dmx_driver_install return ignored | `output_devices/dmx.h` | ✅ Resolved (already checks `err != ESP_OK` at lines 27, 42) |
| 4.2 | I2C speed validation defined but never called | `src/main.cpp` | ✅ Resolved (called in `validateSettingsAndOutputs()`) |
| 4.3 | LED count, universe range not validated server-side | `src/main.cpp` | ✅ Resolved (added in `validateOutputJson()`) |
| 4.4 | mDNS name length not validated on either side | `src/main.cpp` | ✅ Resolved (added in `validateSettingsAndOutputs()`) |
| 5.2 | I2C write count differences (4 types) | `scoring.js` / `scoring.h` | ✅ Resolved (metadata refactor made both per-pin; parity confirmed) |
| 6.2 | System pin options hardcoded | `_gpio.js` / `pane-network.html` | ✅ Resolved (`populateSystemPins()` generates from GPIO arrays) |

### ℹ️ Maintainability

| # | Issue | File | Status |
|---|-------|------|--------|
| 1.8 | Art-Net dead sequence array | `artnet_control.h:20` | ✅ Resolved (`lastSequence[4]` removed) |

---

## 8. What Was Audited

| Area | Files | Lines examined |
|------|-------|---------------|
| Validation logic | `main.cpp`, `web/index.html`, `source_rules.h` | ~800 |
| Scoring parity | `scoring.h`, `scoring_limits.h`, `web/js/scoring.js`, `resource_calculator.md` | ~900 |
| Output routing | `output_control.h`, `motion_control.h`, 18 files in `output_devices/` | ~2,500 |
| Protocol handling | `artnet_control.h`, `sacn_control.h`, `espnow_control.h`, `rmt_dmx.h` | ~1,200 |
| Config save/load | `config.h`, `output_control.h`, `network_protocol.h` | ~600 |
| GPIO validation | `gpio_control.h`, `_gpio.js`, `outputs.js`, `pane-network.html` | ~500 |
| **Total** | **~6,500 lines** | |
