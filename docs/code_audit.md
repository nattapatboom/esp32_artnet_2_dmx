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

### 1.3 ★★☆ `swapBuffers()` cross-core data race

**Files:** `include/output_control.h:478`, `artnet_control.h:105`, `sacn_control.h:237`, `main.cpp:149,2167`

`swapBuffers()` iterates all channels swapping `dmxBuffer`/`shadowBuffer` pointers. Called from both Core 0 (network) and Core 1 (output test). Channel-by-channel swap is not atomic — Core 1 could observe mixed pre/post-swap pointers.

Fix: Call exclusively from one core or add mutex. The `networkFramePending` flag mitigates the normal path, but `clearOutputTest()` at `main.cpp:149` bypasses it.

---

### 1.4 ★★☆ Cross-core atomics missing

**Files:** `main.cpp:54-55`, `output_control.h:33-34`, `sacn_control.h:239`, `artnet_control.h:111`

`lastDmxUpdateTime` and `systemActive` are plain `unsigned long`/`bool` accessed from both cores without `std::atomic`. On 32-bit Xtensa, aligned reads/writes are likely atomic, but compiler may reorder.

---

### 1.5 ★★☆ DMX frame timeout not enforced

**Contradiction:** `docs/domain_model.md` says "DMX Frame Cycle must not exceed 50ms" but code never checks `lastDmxUpdateTime`. No protective action is taken when frames stop arriving (Hold Last State is unimplemented).

---

### 1.6 ★★☆ RMT DMX: `malloc(20600)` failure silently ignored

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

### 2.1 ★★★ 7-segment LEDC index corruption

**File:** `include/output_devices/seven_seg.h:61-64`

```cpp
uint8_t baseChan = allocateLedc(ledcIdx);
if (baseChan != 255) {
    ledcIdx = baseChan + numSeg;  // OVERWRITES caller's ledcIdx
    ...
}
```

If `baseChan=14` and `numSeg=7`, `ledcIdx` becomes 21 (clamped to 16). **All subsequent LEDC allocations return 255.** Every output type after a direct-dim 7-segment channel loses PWM capability.

Fix: Restore `ledcIdx` properly: `ledcIdx = savedIndex + numSeg` or allocate in a loop.

---

### 2.2 ★★★ Stepper array bounds overflow

**File:** `include/output_devices/stepper.h:28`

```cpp
FastAccelStepper* steppers[8] = {nullptr};
steppers[stepperCount] = stepper;  // no bounds check
```

9+ steppers corrupt adjacent memory in MotionControl (likely `ledcChannelIndex`).

Fix: Add `if (stepperCount >= 8) return;` guard.

---

### 2.3 ★★☆ No GPIO pin validation in setup functions

**Files:** All `include/output_devices/*.h`

Every setup function calls `pinMode(ch.pin, ...)` or `ledcAttachPin(ch.pin, ...)` without checking `ch.pin != 255`. If pin=255 (bypassed in JSON), undefined behavior on ESP32 GPIO register access.

Fix: Add `if (ch.pin == 255) return;` at the top of every GPIO-routed setup function.

---

### 2.4 ★★☆ Analog RGB ignores `mc_resolution`

**File:** `include/output_devices/analog_rgb.h:17,26,34,44`

`ledcSetup(rChan, ch.mc_freq, 8)` — hardcoded 8-bit. Web UI allows 10/12/16 bit. Contract (`docs/domain_model.md`) lists mc_resolution support.

---

### 2.5 ★★☆ Motor update division by zero

**File:** `include/output_devices/motor.h:99,161`

```cpp
uint32_t duty = (abs_offset * 4095) / center;  // center = max_val / 2
```

If `mc_resolution == 0`, `max_val = 0`, `center = 0`, integer division by zero.

Same pattern: `servo.h:21`, `single_led.h:22`.

---

### 2.6 ★★☆ PCA9685 frequency override conflict

**File:** `include/output_control.h:934-937` vs `motor.h:27,53,61`, `seven_seg.h:15`, `ledc_helpers.h:58`

`setupChannels()` computes shared PCA frequency and sets it. Then per-type `setup()` overrides with `mc_freq`. Loses shared-frequency contract.

---

### 2.7 ★★☆ `segmentGpio()` returns invalid pin

**File:** `include/output_devices/ledc_helpers.h:49`

```cpp
return (ch.seg_pins[idx] != 255) ? ch.seg_pins[idx] : (ch.pin + idx);
```

If both `seg_pins[idx]==255` and `ch.pin==255`, returns `262`. Undefined behavior at `pinMode(262,...)`.

---

### 2.8 ★★☆ DFPlayer `new` allocation never checked

**File:** `include/output_control.h:965`

`ch.dfPlayer = new DFPlayerController();` — returns nullptr on OOM. Same for `FuncGenController`.

---

### 2.9 ★★☆ `RmtDmxDriver::begin()` ignores return value

**File:** `include/output_devices/rmt_dmx.h:41`

`rmt_driver_install(channel, 0, 0)` return value ignored. If channel is already in use, subsequent `send()` operates on uninstalled channel.

---

### 2.10 ★☆☆ `dimmer.h` ISR accesses non-volatile state

**File:** `include/output_devices/dimmer.h:27-43`

`dimmerChannels[i].dmxVal` not declared `volatile`. Compiler may optimize reads.

---

### 2.11 ★☆☆ `dmxSetup()` double-free risk

**File:** `include/output_control.h:960`

`dmx_driver_install` return value ignored after `dmx_driver_delete`. If install fails, UART state is undefined.

---

## 3. Configuration Save/Load

### 3.1 ★★★ NVS save return values all ignored

**File:** `include/config.h:198-238`

`Preferences::begin()` returns bool — ignored. All 30+ `put*()` calls return `size_t` — ignored. **Silent data loss:** HTTP 200 returned, device reboots with old config.

---

### 3.2 ★★★ LittleFS write failure on migration silently discarded

**File:** `include/output_control.h:712-829` (called at `524,707`)

Internal `saveChannels()` calls (migration path, default-creation) log `Serial.println` but never return error. Migrated config silently lost.

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

### 3.5 ★★☆ `ip4Valid()` accepts empty string

**File:** `include/network_protocol.h:178-200`

```cpp
if (s == nullptr || s[0] == '\0') return true;  // "" === valid!
```

Empty IP fields stored as empty string → interface starts with zero IP.

---

### 3.6 ★★☆ Factory reset reports success on write failure

**File:** `src/main.cpp:1687-1716`

`prefs.clear()` return ignored. LittleFS write failure returns 200. User believes config is reset.

---

### 3.7 ★☆☆ `espnow_channel` range not validated on load

**File:** `include/config.h:141`

`getUChar("now_chan", 0)` — no range check. Corrupted NVS (e.g., 255) passed to `esp_wifi_set_channel()`.

---

### 3.8 ℹ️ Duplicate inline validation in settings handler

**File:** `src/main.cpp:1290-1342`, `591-658`

Settings POST handler repeats checks that `validateSettingsAndOutputs()` already does. Maintenance hazard.

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

### 5.1 ★★☆ Type 6 Mode 0: JS overcounts LEDC

JS `channelHardware()` counts DIR pin as LEDC when GPIO. C++ correctly skips DIR (digital). **Web UI may falsely block configs.**

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

### 6.1 ★★☆ JS `outputGpios()` skips `seg_pins`

**File:** `web/js/_gpio.js:86-97`

`outputGpios()` enumerates `pin`/`pin2`/`pin3`/`pin4` only. 7-segment `seg_pins[8]` are skipped. All JS-side checks using `outputGpios()` are blind to segment GPIOs:

- Forbidden GPIO check
- Input-only pin check
- System pin overlap (Status LED, ZC, I2C)
- Duplicate GPIO check
- GPIO12 strapping warning

C++ `forEachOutputGpioPin()` correctly includes `seg_pins`. **JS validation is incomplete for 7-segment direct-drive outputs.**

---

### 6.2 ★☆☆ System pin options hardcoded in HTML

**File:** `web/parts/pane-network.html`

Status LED, ZC, I2C SDA/SCL dropdown options are hardcoded, not generated from `gpio_control.h`. If `OUTPUT_GPIO_PINS` changes, system pin lists drift.

---

### 6.3 ★☆☆ Reserved-pin list duplicated in `main.cpp`

**File:** `src/main.cpp:355-358`

`outputsUseForbiddenGpio()` hardcodes 10 Ethernet pins instead of using `GpioControl::isReservedEthernetPin()`. `gpio_control.h` is the source of truth; `main.cpp` is a copy.

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

### ★★★ Must fix (crash/data-loss)

| # | Issue | File |
|---|-------|------|
| 1.1 | sACN offsets +16, dead code | `sacn_control.h` |
| 2.1 | 7-seg LEDC index corruption | `seven_seg.h:61` |
| 2.2 | Stepper array bounds overflow | `stepper.h:28` |
| 3.1 | NVS save return values ignored | `config.h:198` |
| 3.2 | LittleFS migration write failure silent | `output_control.h:712` |

### ★★☆ High priority

| # | Issue | File |
|---|-------|------|
| 1.2 | ArtPollReply wrong UDP port | `artnet_control.h:185` |
| 1.3 | `swapBuffers()` data race | `output_control.h:478` |
| 2.3 | No pin validation in setup functions | All `output_devices/*.h` |
| 2.4 | Analog RGB ignores mc_resolution | `analog_rgb.h` |
| 2.5 | Division by zero (motor/servo) | `motor.h:99` |
| 2.6 | PCA frequency override conflict | `motor.h` / `setupChannels` |
| 2.7 | `segmentGpio()` returns invalid pin | `ledc_helpers.h:49` |
| 3.5 | ip4Valid accepts empty string | `network_protocol.h:178` |
| 3.6 | Factory reset false success | `main.cpp:1687` |
| 5.1 | Type 6 Mode 0 LEDC overcount (JS) | `scoring.js` |
| 6.1 | JS outputGpios skips seg_pins | `_gpio.js:86` |

### ★☆☆ Medium priority

| # | Issue | File |
|---|-------|------|
| 1.4 | Missing atomics (lastDmxUpdateTime, systemActive) | `main.cpp` |
| 1.5 | DMX frame timeout not enforced | `output_control.h` |
| 1.6 | RMT DMX malloc failure silent | `rmt_dmx.h:21` |
| 2.8 | DFPlayer new not null-checked | `output_control.h:965` |
| 2.9 | rmt_driver_install return ignored | `rmt_dmx.h:41` |
| 2.10 | ISR volatile missing | `dimmer.h:27` |
| 2.11 | dmx_driver_install return ignored | `output_control.h:960` |
| 3.7 | espnow_channel no range check | `config.h:141` |
| 5.2 | I2C write count differences (4 types) | `scoring.js` / `scoring.h` |
| 6.2 | System pin options hardcoded | `pane-network.html` |

### ℹ️ Maintainability

| # | Issue | File |
|---|-------|------|
| 1.8 | Art-Net dead sequence array | `artnet_control.h:20` |
| 3.8 | Duplicate inline validation | `main.cpp:1290` |
| 6.3 | Reserved-pin list duplicated | `main.cpp:355` |

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
