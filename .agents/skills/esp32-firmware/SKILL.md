---
name: esp32-firmware
description: Full-stack ESP32 Art-Net firmware project (WT32-ETH01) — PlatformIO, 19 output types (v3 0-18), DMX/Art-Net control, Web UI, OTA deploy
metadata:
  board: wt32-eth01
  framework: arduino
  platform: espressif32
  build_cmd: pio run
  ota_url: http://192.168.1.93/update
  ota_cmd: curl.exe -F "update=@.pio\build\wt32-eth01\firmware.bin" http://192.168.1.93/update
  language: en
---

## Project Overview

ESP32-based Art-Net to DMX/Pixel/Relay/Motion controller for stage lighting.
Receives Art-Net via Ethernet (LAN) → controls multiple output types via GPIO, PCA9685, I2C expander

### Context Map
- `AGENTS.md` — concise project instructions loaded for agents
- `docs/domain_model.md` — domain vocabulary, bounded contexts, invariants, device modes, thread safety, ADRs, and grilling results
- `docs/resource_calculator.md` — current scoring formula, hardware limits, and validation rules
- `docs/hardware_guidelines.md` — ESD protection, flyback diode, datasheet checklist, verification guidelines
- `docs/user_manual/` — Typst user manual source (English-first, modular chapters, CeTZ/Fletcher diagrams)
- `web/index.html` — Web UI source of truth; regenerate `include/web_pages.h` after edits
- `docs/domain_model.md` -> `Configuration Contract` — source/routing/scoring contract for validation and implementation work

### Current State
- **Board:** 192.168.1.93 (WT32-ETH01) — ONLINE
- **Build:** Flash ~63.6%, RAM ~17.3%
- **Layout:** v3 (0-18) — config version 3
- **Protocols:** Art-Net, sACN E1.31, ESP-NOW bridge
- **Last Version:** 1.30.00 + header refactor (cleanup duplicates, output_common.h, output_impl.h → *.cpp split)

## Board: WT32-ETH01
- **CPU:** ESP32 dual-core (Core 0 = network/display, Core 1 = application)
- **Flash:** 4MB (partition: app 1.9MB, LittleFS 190KB) — Currently ~65.7%
- **RAM:** 320KB — Currently ~17.5%
- **Usable GPIO:** 4, 12, 14, 15, 2, 17, 32, 33 (+ input-only 36, 39, 34, 35)
- **Built-in:** Ethernet (LAN8720), no WiFi needed in Ethernet mode

### Hardware Notes
- Avoid GPIO16 (LAN8720A Ethernet power)
- Default status LED: GPIO 5
- Common output GPIOs: 4, 12, 14, 15, 2, 17, 32, 33
- Input-only: 36, 39, 34, 35
- GPIO12 is boot strap pin — careful with pull-up/down (warning shown in Web UI)
- **ALL GPIO outputs MUST have pull-down resistors** when connecting to interface boards
- DAC (Type 14) GPIO25/26 are used by LAN8720 RMII on WT32-ETH01 — unavailable

### Runtime Architecture
- **Core 0 — networkTask:** Art-Net, sACN, ESP-NOW, AsyncWebServer, I2C Display update, I2C Scan (queue-based)
- **Core 1 — outputTask:** DMX TX, LED output, motion control, output test
- **Storage:** LittleFS (`/outputs.json`, `/espnow_peers.json`), NVS Preferences (settings)

### ESP-NOW Bridge Contract
- **Modes:** Master receives Art-Net/sACN and forwards DMX to configured ESP-NOW peers; Slave receives ESP-NOW DMX and drives local outputs.
- **Peer storage:** `/espnow_peers.json` stores route objects with `mac`, `start_universe`, `start_address`, `end_universe`, `end_address`.
- **Fallback:** If the master peer list is empty, it broadcasts to `FF:FF:FF:FF:FF:FF`.
- **Range limits:** Universe `0..32767`, DMX address `1..512`; keep per-peer ranges narrow to reduce wireless airtime.
- **Payload:** Current `ESPNOW_DMX_CHUNK_SIZE = 200`; custom packet header is 12 bytes, so max firmware packet is about 212 bytes and a full 512-channel universe uses up to 3 packets.
- **Chunk protocol:** If a peer range exceeds chunk size, master sends multiple packets for the same universe with increasing `offset` and per-packet `length`; slave maps each packet by `universe`/`offset`, no packet sequence number required.
- **Planned setting:** Make ESP-NOW DMX chunk size user-configurable in Web UI/system settings. Keep a compile-time maximum receive buffer separate from the configured send chunk size; packet `length` lets slaves accept variable chunk sizes within max buffer.
- **Runtime safety:** Receive callback copies packet fields into aligned storage and queues packets; Core 1 `outputTask` maps by universe/offset. Queue depth is 16 packets.
- **Wi-Fi radio:** ESP-NOW requires Wi-Fi/AP/STA mode. Slave keeps setup AP active for field configuration, so avoid unnecessary AP clients during critical wireless operation.

## Output Types (v3 — 0-18)

| Type | Name | DMX Bytes | Source Support | Resource |
|------|------|-----------|----------------|----------|
| 0 | AC Dimmer (TRIAC) | 1-2 | GPIO only | GPIO 1 + ZC Timer |
| 1 | DMX Serial Output | 512 | GPIO only; DFPlayer-priority UART/RMT runtime allocation | UART or RMT fallback |
| 2 | Relay (On/Off) | 1 | GPIO, PCA, Expander | GPIO or PCA/Exp 1 |
| 3 | RGB LED (NeoPixel) | 3-4/led | GPIO only (RMT) | RMT 1ch |
| 4 | Single Color LED (PWM) | 1-2 | GPIO, PCA | GPIO+LEDC or PCA 1 |
| 5 | Analog RGB/RGBW | 3-4 | GPIO, PCA | LEDC 3-4ch |
| 6 | DC Motor (H-Bridge) | 1-2 | GPIO/PCA primary; IN2/DIR hybrid | LEDC/PCA 1-3ch |
| 7 | Stepper Motor | 3-5 | STEP GPIO-only; DIR/ENABLE/HOME hybrid | FastAccelStepper (2 RMT) |
| 8 | RC Servo | 1 | GPIO, PCA | LEDC 1ch (50Hz) |
| 9 | Passive Buzzer | 2 | GPIO only | LEDC 1ch |
| 10 | DFPlayer MP3 Module | 3 | GPIO only (UART) | UART 1 |
| 11 | 7-Segment 2-Pin (TM1637) | 2-4 | GPIO only | GPIO 2 |
| 12 | 7-Segment DD 7-Pin PWM | 1-2 (Direct) | Direct Dim GPIO/PCA only; No Dim/Common Dim GPIO/PCA/Expander routing | Routing-dependent segment resources |
| 13 | 7-Segment DD 8-Pin PWM | 1-2 (Direct) | Direct Dim GPIO/PCA only; No Dim/Common Dim GPIO/PCA/Expander routing | Routing-dependent segment resources |
| 14 | DAC (Analog Out) | 1 | MCP4725 preferred; GPIO DAC legacy/unsafe on WT32-ETH01 | I2C MCP4725 (Source 5) |
| 15 | PWM DAC (RC Filter) | 1-2 | GPIO, PCA | LEDC 1ch |
| 16 | Function Generator | 5 | GPIO only | LEDC 1ch + esp_timer |
| 17 | Solenoid Trigger | 1 | GPIO, PCA, Expander | GPIO 1 / PCA 1 / Exp 1 |
| 18 | Sequential Smoke Shooter | 1 | GPIO, PCA, Expander | GPIO 2 / PCA 2 / Exp 2 |

## Architecture

### Key Files
- `include/output_common.h` — Shared primitives: SENTINEL_NONE, DMX_BUFFER_SIZE, getValueByteCount, getMaxValue
- `include/output_control.h` — Core: OutputChannel struct, PixelStripWrapper abstract base, DMX processing, save/load JSON. Declares setupChannels/loop/updateLeds (bodies in output_control.cpp)
- `include/motion_control.h` — thin coordinator delegating to `output_devices/` files
- `include/output_devices/` — one file per output type (17 files total: 0-18)
- `include/scoring.h` — Resource + Compute scoring (limit ~109pts)
- `include/output_devices/funcgen_control.h` — Function Generator (Type 16) waveform engine using esp_timer + LEDC
- `include/output_devices/dfplayer_control.h` — DFPlayer MP3 protocol driver
- `include/output_devices/seven_seg_digits.h` — SEG_DIGITS[] lookup table + asciiToSegment()
- `include/output_devices/ledc_helpers.h` — LEDC allocation, DMX value, segment, PWM DAC calibration helpers
- `include/output_devices/stepper.h` — Stepper setup/update + setStepperDirection/setStepperEnable helpers
- `include/output_devices/led_strip.h` — PixelStripRmt/PixelStripRmtRgbw concrete classes + ledStripSetup/Update
- `include/config.h` — System configuration struct
- `src/main.cpp` — Entry point, HTTP API routes, validation, network tasks
- `src/output_control.cpp` — OutputControl loop/load/save/begin/swapBuffers/clearChannels + setupChannels + free functions loadChannelPins/saveChannelPins/outputDmxByteCount/writeOutputPin/readOutputPin
- `src/motion_control.cpp` — MotionControl begin/update
- `src/espnow_control.cpp` — EspNowControl begin/loop/loadPeers/sendDmx + static data definitions
- `src/artnet_control.cpp` — ArtNetControl begin/loop/parseArtDmx/sendArtPollReply + static packetCount
- `src/sacn_control.cpp` — SACNControl begin/process/validation + helper methods
- `include/web_pages.h` — Embedded HTML/CSS/JS for Web UI (auto-generated from `web/index.html`)
- `web/index.html` — Edit this file, run `tools/build_web.py` to regenerate `web_pages.h`

### OutputChannel Struct (output_control.h:98-192)
- `type` (0-18 v3), `source` (0=GPIO, 1=PCA9685, 2=MCP23017, 3=TCA9555, 4=PCF857x, 5=MCP4725, 6=DAC7571, 7=DAC7573)
- `pin`-`pin4` with `_source`/`_addr`/`_channel` for expander hybrid
- `dmxBuffer`, `bufferSize` (512 bytes for DMX, smaller for others)
- `pixelStrip` (LED), `dmxPort`/`rmtDmx` (DMX), `dfPlayer` (MP3)
- `ledc_chan2`/`ledc_chan3`/`ledc_chan4` — extra LEDC channels for Motor/RGB
- `seg_pins`, `seg_sources`, `seg_addrs`, `seg_channels` — 8-element arrays for individual segment routing of 7-segment displays (Type 12 / 13) to GPIO or expanders
- Source compatibility (v3): PCA (2,4,5,6,8,12,13,15,17,18), Digital (2,6,17,18), I2C DAC sources 5-7 (14)

### Hardware Resource Limits
- **UART:** 3 total. UART0=console. UART1 and UART2 are dynamically allocated. **DFPlayer (Type 10) has priority on UART** (assigns to UART2, then UART1) because DMX can dynamically fall back to RMT.
- **LEDC:** 16 channels max (Type 4, 5, 6, 8, 9, 12, 13, 15, 16)
- **RMT:** 8 channels max (Type 3 LED strips + extra DMX fallback). Total RMT channels cannot exceed 8.
- **Timer:** 4 total (AC dimmer uses 1, Function Generator uses 1 per channel)
- **I2C:** 1 bus (Wire), shared by all expanders + display
- **Dynamic UART Allocation:** DFPlayer is allocated first. DMX uses any remaining UART. If none are free, DMX falls back to RMT driver automatically.

### System Security & Stability
- **HTTP DoS Protection:** Rejects any incoming HTTP requests with bodies exceeding 64KB to prevent OOM attacks.
- **Settings Sanitization:** Validates `output_fps` to be between 1 and 44 on loading and saving.
- **Memory Allocation Safety:** Performs NULL checks on all `calloc()` calls allocating DMX buffers.
- **Interrupt Storm Rate Limiting:** Enforces a 5ms rate limit on zero-cross interrupts.
- **Core-Safe Deferral:** ESP-NOW callback pushes packets into a FreeRTOS queue, deferring processing to Core 1 `outputTask`.
- **Atomic Flags:** `std::atomic<bool>` for `networkFramePending`, `newRxData`.
- **I2C Hang Protection:** 100ms timeout on `i2cMutex` takes (`xSemaphoreTake(i2cMutex, pdMS_TO_TICKS(100)) == pdTRUE`).
- **Bootstrapping Pin Warning:** Web UI warns if GPIO 12 (MTDI) is configured in any output/LED/ZC/I2C slot.
- **Configuration Layout Versioning:** Config version 3 with v1→v3 and v2→v3 migration paths in `loadChannels()`.

## Resource + Compute Scoring System (v3)

Total score is calculated from 2 parts:
1. **Resource Score** — hardware resources each channel uses; contract is in `docs/domain_model.md`, implementation is in `scoring.h:estimateResources()`
2. **Compute Score** — CPU load each type generates + based on FPS (`scoring.h:channelComputeScore()` + `fpsComputeFactor()`)

Resource Score is firmware-authoritative and should be routing-accurate based on actual source/mode/per-pin routing, including hybrid pins, segment-level routing, and DMX fallback from UART to RMT after DFPlayer reserves UARTs. The Web UI may display estimates, but must not block saves; hard validation and blocking belong in C++ firmware/API responses.

Known drift to audit next: `include/scoring.h::totalOutputScoreFromJson()` must copy all routing fields from JSON before calling `estimateResources()`.

### Resource Weights

| Resource | Weight | Description |
|----------|--------|-------------|
| GPIO | 0.5 | Per normal output pin |
| LEDC | 2.5 | Per PWM channel (16 ch. total) |
| RMT | 3.0 | Per channel (8 ch. total) |
| UART | 8.0 | Per port (only 2 ports) |
| DAC | 2.0 | Per channel (GPIO25/26 reserved by LAN8720) |
| PCA9685 | 0.25 | Per channel (cheap because I2C, 16 ch./chip) |
| I2C Expander | 0.125 | Per pin |

### Scoring principle per channel (source=GPIO)

| Type | GPIO | LEDC | RMT | UART | DAC | PCA | Exp |
|------|------|------|-----|------|-----|-----|-----|
| 0 AC Dimmer | 1 | - | - | - | - | - | - |
| 1 DMX | 1 | - | - | 1 | - | - | - |
| 2 Relay | 1* | - | - | - | - | 1* | 1* |
| 3 RGB LED | 1 | - | 1 | - | - | - | - |
| 4 Single LED | 1* | 1* | - | - | - | 1* | - |
| 5 Analog RGB | 3-4* | 3-4* | - | - | - | 3-4* | - |
| 6 Motor | 2-3* | 1-2* | - | - | - | 2-3* | - |
| 7 Stepper | 2-3* | - | 2 | - | - | hybrid* | hybrid* |
| 8 Servo | 1* | 1* | - | - | - | 1* | - |
| 9 Buzzer | 1 | 1 | - | - | - | - | - |
| 10 DFPlayer | 2 | - | - | 1 | - | - | - |
| 11 7-Seg 2-Pin | 2 | - | - | - | - | - | - |
| 12 7-Seg 7-Pin | per segment* | routing-dependent* | - | - | - | per segment* | per segment* |
| 13 7-Seg 8-Pin | per segment* | routing-dependent* | - | - | - | per segment* | per segment* |
| 14 DAC | 1* | - | - | - | 1* | - | 1* |
| 15 PWM DAC | 1* | 1* | - | - | - | 1* | - |
| 16 Func Gen | 1 | 1 | - | - | - | - | - |
| 17 Solenoid | 1* | - | - | - | - | 1* | 1* |
| 18 Smoke | 2* | - | - | - | - | 2* | 2* |
| **I2C DAC (src 5-7)** | - | - | - | - | - | - | 1 (I2C) |

*Numbers change based on selected source (0=GPIO+LEDC, 1=PCA, 2-4=Expander, 5=MCP4725, 6=DAC7571, 7=DAC7573)
- 7-Segment Type 12/13 in direct drive mode must score based on actual route of each segment via `seg_sources`/`seg_pins` or base routing, not always counting full GPIO/LEDC
- DAC Type 14 when selecting I2C DAC source (5-7) does not consume ESP32 GPIO/internal DAC pins; consumes only I2C bus time (reflected in CPU budget)

### Formula
```
resourceScore = GPIO×0.5 + LEDC×2.5 + RMT×3.0 + UART×8.0 + DAC×2.0 + PCA×0.25 + EXP×0.125
computeScore  = Σ channelCompute(type) + (fps/60)×5
totalScore    = resourceScore + computeScore
```

All channels combined must not exceed **SCORE_LIMIT ≈ 109** (84 resource + 25 compute)

### Resource Max (per WT32-ETH01 hardware)
| Resource | Available | Weight | Max Score |
|----------|-----------|--------|-----------|
| GPIO (output) | 8 usable | ×0.5 | 4.0 |
| LEDC | 16 | ×2.5 | 40.0 |
| RMT | 8 | ×3.0 | 24.0 |
| UART | 2 | ×8.0 | 16.0 |
| DAC | 0 (blocked by ETH) | ×2.0 | 0 |
| **Total resource max** | | | **84.0** |
| **Compute max** | | | **25.0** |
| **Total max** | | | **≈ 109** |

## Computational Scoring Factor

| Type | CPU Points | Reason |
|------|-----------|--------|
| 3 RGB LED | `led_count × 0.005` | per-pixel data processing |
| 6 Motor | 0.5 | speed/PWM calculations |
| 7 Stepper | 2.0 | position tracking + RMT management |
| 10 DFPlayer | 0.5 | UART protocol handling |
| 11 7-Seg TM1637 | 0.5 | I2C/GPIO update timing |
| 12 7-Seg DD 7-Pin | 1.0 | 7× PWM refresh |
| 13 7-Seg DD 8-Pin | 1.2 | 8× PWM refresh |
| 16 Func Gen | 2.0 | continuous waveform calculation |
| 18 Smoke Shooter | 0.3 | state machine timing |

FPS Factor: `(fps/60) × 5` — at 40fps (default) = 3.33

### Where Weight/Compute are located
- **C++:** `scoring.h` — `W_GPIO` etc., `resourceScore()`, `channelComputeScore()`, `fpsComputeFactor()`
- **Web UI:** generated/display-only resource monitor; do not use JS scoring to block saves

## Web UI Hardware Monitor

Web UI has a **Hardware Resource Monitor** bar (4 cards) below the Resource Score bar:
- **RMT Channels** (max 8): LED Strip count + DMX channels exceeding UART → using RMT
- **UART Ports** (max 2): DFPlayer count + DMX that gets UART
- **LEDC Channels** (max 16): PWM, Motor, Servo, RGB/RGBW, Buzzer, 7-Seg DD, PWM DAC, Func Gen
- **Total Score** (max ≈109): resource + compute score

Colors: 🟢 Normal, 🟡 At limit, 🔴 Over limit → display only; firmware decides save success/failure

### UART/RMT Counting Logic (JS: `updateScoreBar()`)
```js
freeUarts = max(0, 2 - dfPlayerCount)
dmxUartUse = min(dmxCount, freeUarts)
dmxRmtUse = max(0, dmxCount - freeUarts)
uartUsed = dfPlayerCount + dmxUartUse
rmtUsed = ledCount + dmxRmtUse
```

## Interlocking System

### 1. GPIO Interlock (Pin Conflict Prevention)

| Rule | Authoritative implementation |
|------|-----|
| No duplicate GPIO between channels | C++ API validation in `src/main.cpp` |
| No pin reuse with Status LED | C++ API validation in `src/main.cpp` |
| No pin reuse with ZC pin | C++ API validation in `src/main.cpp` |

Warning if AC Dimmer exists but ZC pin = 255 (Disabled) — Web UI `#zc-warning`

### 2. PCA9685 Frequency Interlock

Each PCA9685 chip shares the same frequency across the whole chip:
1. **Servo (Type 8) — Priority HIGHEST:** forces 50 Hz
2. **Motor/PWM types:** use `mc_freq` of the first channel on that chip
3. **Default:** 1000 Hz

Validation/warning source: C++ firmware/API; Web UI displays firmware responses

### 3. UART Interlock

- DFPlayer (Type 10) has highest priority → UART2 (Serial2) → UART1 (Serial1)
- DMX (Type 1) uses remaining UARTs, if none → fallback RMT
- Maximum 2 DFPlayer channels (UART limit)

### 4. Smoke Shooter State Machine

IDLE → SMOKE → SETTLE → SHOOT → COOLDOWN → IDLE
- `smoke_lockout_ms` (default 2000ms) prevents re-fire
- See `output_control.h:updateSmokeShooters()`

---

## Critical Code Patterns — MUST KNOW

1. **i2cMutex** — Every `Wire` operation: `xSemaphoreTake(i2cMutex, pdMS_TO_TICKS(100)) == pdTRUE`
2. **Scoring** — C++ `scoring.h` is authoritative; Web UI scoring is informational only and must not block saves
3. **Layout versioning** — `/outputs.json` has `version: 3`, v1→v3 and v2→v3 migration in `loadChannels()`
4. **PCA frequency interlock** — `getPcaSharedFrequency()` — Servo on chip → forces 50 Hz
5. **GPIO interlock** — validate in C++ (`main.cpp`); Web UI submits values and displays API errors
6. **JS falsy trap** — use `o.pin4!==undefined?o.pin4:255` instead of `o.pin4||255` because `0` is falsy
7. **web/index.html** — edit here, then run `tools/build_web.py` → `include/web_pages.h`
8. **DFPlayer UART priority** — `setupChannels()` loop finds DFPlayer first, reserves UART before DMX

## P0 Bugs (all fixed in v1.16.00)

All 8 P0 bugs resolved: dimmer ISR safety, static globals, ESP-NOW queuing, unaligned pointer, calloc NULL check, div-by-zero, HTTP OOM, UART2 conflict (DFPlayer priority + DMX RMT fallback).

## File Map

```
include/
  config.h              SystemConfig, NVS load/save
  output_control.h      Output mapping, DMX/LED rendering, save/load JSON
  scoring.h             Resource + Compute scoring (limit ~109pts)
  web_pages.h           Embedded web UI (HTML+CSS+JS, auto-generated)
  motion_control.h      Thin coordinator delegating to output_devices/ files
  output_devices/       One file per output type (17 files: 0-18)
  i2c_devices/          I2C DAC, GPIO expander, PCA9685 drivers

  output_devices/dfplayer_control.h    DFPlayer MP3 protocol driver (Type 10)
  output_devices/funcgen_control.h     Function Generator (Type 16, esp_timer + LEDC waveform engine)
  i2c_devices/i2c_gpio_expander.h   MCP23017, TCA9555, PCF857x drivers
  i2c_devices/pca9685.h             PCA9685 PWM driver
  lighting_protocols/artnet_control.h      Art-Net UDP listener
  lighting_protocols/sacn_control.h        sACN E1.31 listener
  espnow_control.h      ESP-NOW master/slave bridge
  output_devices/rmt_dmx.h             RMT-based DMX TX fallback
  i2c_devices/display_driver.h      SSD1306/SH1106 OLED + PCF8574 LCD driver
src/
  main.cpp              Setup, network, web APIs, RTOS tasks, OTA, validation
web/
  index.html            Web UI source — EDIT THIS, then run build_web.py
docs/
  domain_model.md       Domain vocabulary, bounded contexts, contract, device modes, ADRs
  resource_calculator.md Hardware resource scoring, limits, and validation rules
  hardware_guidelines.md ESD protection, flyback diode, datasheet checklist
  user_manual/          Typst user manual source (main.typ + chapters)
handover/
  README.md             Handover system rules
  1.23.00.md            Latest — v3 layout migration (0-18)
  archive/              Drafts and backups
tools/
  build_web.py          Minify web/index.html → web_pages.h (auto-run in build_firmware_bin.bat)
  extract_web.py        Extract web_pages.h → web/index.html
  web_mock_server.py    Mock REST API for web UI dev (http://localhost:8000, auto-reload)
  web_ui_smoke_test.mjs Headless Web UI smoke test (Node + Chrome/Edge, no npm deps)
  load_calculator.py    Python resource calculator (offline analysis)
```

## Hardware Module Integration & Reference

### 1. PCA9685 (16-Channel PWM Expander)
- **Interface:** I2C (Address 0x40 - 0x47)
- **Usage:** Relay, DC PWM Dimmer, DC Motor, Servo, Analog RGB, Smoke Shooter
- **Note:** Shared frequency per chip — if Servo exists on chip, all channels forced to 50 Hz

### 2. DFPlayer Mini (MP3 Module)
- **Interface:** Serial UART (Serial2 or Serial1, 9600 bps)
- **Pinout:** Board TX → Module RX (via 1KΩ series resistor), Board RX → Module TX
- **DMX:** 3 bytes (Byte 1 = command, Byte 2 = folder/track, Byte 3 = volume 0-30)

### 3. TM1637 & Direct Drive 7-Segment Displays (Type 11, 12, 13)
- **TM1637 (Type 11):** 2-wire GPIO serial CLK/DIO display (uses 2 or 4 DMX channels for Numeric or ASCII text mode). GPIO-only; digital expanders are for direct-drive Type 12/13, not TM1637.
- **Direct Drive (Type 12 / 13):** Direct Pin Drive 7-Segment (7-pin or 8-pin with Decimal Point).
  - **Segment Routing:** Supports independent pin assignment of each segment (A to DP) to various GPIO pins (`seg_pins`) or Expander via `seg_sources`, `seg_addrs`, `seg_channels` in any mixed combination. Direct Dim mode (mc_mode 4/5) must use ESP32 GPIO or PCA9685 only because each segment needs PWM; MCP23017/TCA9555/PCF857x can only be used with No Dim/Common Dim where segments are digital output.
  - **Segment Inversion:** Has an Invert button for each segment pin independently, with values stored as 8-bit bitmask `seg_inverts` and used to logically invert the signal level before driving the output pin in C++
  - **Segment A Mapping (Direct Mode):** In Seg A individual pin mode (modes 2-5), Seg A data must be linked to the channel's main pin settings (`no_source`, `no_pca_addr`, etc.) in both save and load steps, so that sending values to the expander for Seg A works correctly
  - **Dimming Modes:**
    - **No Dimming (1 Ch):** 1 DMX channel controls segment char/state at full brightness.
    - **Dimmed Mode (2 Ch):** DMX Channel 1 controls character, DMX Channel 2 controls PWM duty cycle brightness level (0-255).
    - **Common Dimming (6-9):** Uses Common Anode/Cathode wire connected to PWM (LEDC or PCA9685) for overall brightness control, while other segments are connected as digital GPIO/Expander/PCA9685 for normal data output
      - Mode 6: Common Anode (CA) 7-Pin Dim (2 Ch)
      - Mode 7: Common Cathode (CC) 7-Pin Dim (2 Ch)
      - Mode 8: Common Anode (CA) 8-Pin Dim (2 Ch)
      - Mode 9: Common Cathode (CC) 8-Pin Dim (2 Ch)

### 4. MCP23017 / TCA9555 / PCF857x (I2C GPIO Expander)
- **Interface:** I2C. MCP23017/TCA9555 use addresses 0x20 - 0x27. PCF857x uses 0x20 - 0x27 or 0x38 - 0x3F.
- **Usage:** Digital outputs (Relay, Solenoid, Smoke, 7-Segment display segment mapping), Stepper limit switch input
- **Protection:** I2C Hang Protection 100ms timeout

### 5. PWM DAC (Type 15) — RC Low-Pass Filter
- **Circuit:** GPIO (LEDC PWM) → R → C → Analog Out
- **Web UI:** RC filter calculator in channel config (enter R/C → automatically calculates fc)
- **Resolution:** 8-16 bit through `mc_resolution`, carrier freq via `mc_freq`
- **Calibration:** `pwm_dac_mode` labels Custom/0-10V/4-20mA; `pwm_dac_min` and `pwm_dac_max` store duty percent x100 (0..10000) and map DMX 0..full-scale to the calibrated duty range for external 0-10V or 4-20mA interface circuits.

### 6. Function Generator (Type 16)
- **Engine:** `FuncGenController` in `include/output_devices/funcgen_control.h`
- **Hardware:** esp_timer (high-res timer interrupt) + LEDC (PWM output)
- **DMX 5 Bytes:** Byte 0-1 = Freq (1-10000 Hz), Byte 2 = Waveform, Byte 3 = Amplitude, Byte 4 = DC Offset
- **Waveforms:** 0=Sine, 1=Triangle, 2=Sawtooth, 3=Square, 4=PWM
- **RC Filter Reference:** Uses RC filter circuit to convert PWM to analog

```
GPIO (PWM Out) ── R ──── Analog Out
                   |
                   C
                   |
                  GND
```

### 7. I2C DACs (12-bit)
- **MCP4725:** `source=5`, addresses 0x60 and 0x61, single channel
- **DAC7571:** `source=6`, addresses 0x4C and 0x4D, single channel
- **DAC7573:** `source=7`, addresses 0x4C - 0x5B, quad channel; `pca_channel` selects A-D
- **Usage:** Analog Output (Type 14 DAC) that uses I2C instead of ESP32 DAC pins (GPIO25/26) to avoid conflict with LAN8720 RMII pins
- **Resolution:** 12-bit I2C DAC (sends I2C command with value 0-4095 scaled from DMX 0-255)

### 7.1 I2C Displays
- **OLED:** SSD1306/SH1106 addresses 0x3C or 0x3D
- **LCD Backpack:** PCF8574 LCD addresses 0x27 or 0x3F
- **Validation:** Display address must match display type and is separate from output source validation

### 8. DC Motor EN Pin (H-Bridge Mode 2)
- **Motor Mode 2 (IN1+IN2+EN):** The EN pin requires PWM for speed control, so it must be connected only to ESP32 GPIO (using LEDC) or PCA9685. Connecting to Digital Expander (MCP23017/TCA9555/PCF857x) is strictly forbidden, and validation warns in Web UI and C++ firmware

### 9. Analog RGB / RGBW (Type 5) Independent Routing
- **Routing:** Red, Green, Blue, and White (for RGBW) color channels can independently select Source (ESP32 GPIO or PCA9685), Address, and Pin/Channel for each pin, with no requirement to share the same source/address or be contiguous channels like previous versions
- **Scoring:** Resource scoring calculates per-channel in detail based on actual source selected for each pin

### 10. Sequential Smoke Shooter (Type 18) & Solenoid Trigger (Type 17) Threshold Configuration
- **Solenoid Trigger (Type 17):** Supports only Threshold Mode (Frame-Sync mode has been removed), firing a pulse when DMX value exceeds the configured threshold (`solenoid_threshold`)
- **Sequential Smoke Shooter (Type 18):** Has a Threshold Value field (`smoke_threshold` in the UI, stored as `solenoid_threshold` in JSON) where the trigger condition checks if DMX value exceeds the user-defined threshold instead of the fixed value of 127
- **Routing:** Smoke Valve (Pin 1) and Shoot Valve (Pin 2) pins can independently select Source (GPIO, PCA9685, or digital expanders), Address, and Pin/Channel, providing flexibility in allocating smoke valve and air shoot valve pins

---

## Developer Tools

- **[build_web.py](file:///c:/Users/natta/Documents/bar_program/esp32_eth01_artnet_device/tools/build_web.py)**
  - Minify `web/index.html` → `include/web_pages.h`
  - Called automatically from `build_firmware_bin.bat`
- **[extract_web.py](file:///c:/Users/natta/Documents/bar_program/esp32_eth01_artnet_device/tools/extract_web.py)**
  - Extracts HTML back from `web_pages.h` (used when needing to reverse)
- **[web_mock_server.py](file:///c:/Users/natta/Documents/bar_program/esp32_eth01_artnet_device/tools/web_mock_server.py)**
  - Simulates REST API on `http://localhost:8000`
  - Used to test Web UI without uploading to the actual board
  - Auto-reload: when saving `web/index.html` the browser will refresh itself within about 1 second
  - Quick command for UI dev: `c:\Users\natta\.platformio\penv\Scripts\python.exe tools/web_mock_server.py` then open `http://localhost:8000`
- **[web_ui_smoke_test.mjs](file:///c:/Users/natta/Documents/bar_program/esp32_eth01_artnet_device/tools/web_ui_smoke_test.mjs)**
  - Automated Web UI smoke test from code, no need for user to open the web page manually
  - Uses Node built-in + Chrome/Edge headless via DevTools Protocol, no `npm install` needed
  - Verifies Web UI can render all 19 output types and add Function Generator
  - Command: `node tools/web_ui_smoke_test.mjs`
- **[load_calculator.py](file:///c:/Users/natta/Documents/bar_program/esp32_eth01_artnet_device/tools/load_calculator.py)**
  - Calculates resource/load offline from outputs.json

## Coding Conventions
- **No comments** in code unless necessary
- Follow existing patterns (look at neighboring code first)
- Use `std::vector<OutputChannel>` for channel storage
- JSON serialization with ArduinoJson 7.x
- Web UI source: `web/index.html` → run `build_web.py` → `web_pages.h`
- Thai language for communication
- **Config Appropriateness:** Each channel must always hide irrelevant parameters, e.g., Solenoid (Type 17) must not show the Resolution option and is always set to 1 Ch DMX byte size, to reduce redundancy and prevent user confusion

## Build & Deploy
```powershell
# Fast Web UI test (no build/OTA, auto-reload on save)
c:\Users\natta\.platformio\penv\Scripts\python.exe tools/web_mock_server.py

# Automated Web UI smoke test (preferred before asking user to test)
node tools/web_ui_smoke_test.mjs

# Build
pio run

# Build fallback when pio is not in PATH
& "C:\Users\natta\.platformio\penv\Scripts\platformio.exe" run

# OTA upload
curl.exe -F "update=@.pio\build\wt32-eth01\firmware.bin" http://192.168.1.93/update

# Regenerate web_pages.h after editing web/index.html
c:\Users\natta\.platformio\penv\Scripts\python.exe tools/build_web.py

# Or use batch file (runs build_web.py + pio run + timestamp .bin)
build_firmware_bin.bat
```

## Git Commit Workflow
- Do not commit unless the user explicitly requests a commit.
- Commit message format: Recommend using English or Thai format that is easy to understand, e.g., `feat(7seg): add numeric decoding mode` or `fix(ui): fix Segment A PCA routing`

## When Modifying Code
1. Read existing files to understand patterns
2. Check resource limits (UART, LEDC, RMT, GPIO)
3. Ensure expander source compatibility
4. If editing Web UI: edit `web/index.html` → run `build_web.py` → build firmware
5. Read `docs/domain_model.md` -> `Configuration Contract` before changing domain concepts, validation, scoring, or output routing
