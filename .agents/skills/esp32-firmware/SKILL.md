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
  language: th
---

## Project Overview

ESP32-based Art-Net to DMX/Pixel/Relay/Motion controller for stage lighting.
รับ Art-Net ผ่าน Ethernet (LAN) → ควบคุม output หลายประเภทผ่าน GPIO, PCA9685, I2C expander

### Context Map
- `AGENTS.md` — concise project instructions loaded for agents
- `docs/domain_model.md` — domain vocabulary, bounded contexts, invariants, and grilling results
- `docs/resource_calculator.md` — current scoring formula, hardware limits, and validation rules
- `docs/user_manual.md` — user-facing behavior and setup flow
- `web/index.html` — Web UI source of truth; regenerate `include/web_pages.h` after edits
- `docs/domain_model.md` -> `Configuration Contract` — source/routing/scoring contract for validation and implementation work

### Current State
- **Board:** 192.168.1.93 (WT32-ETH01) — ONLINE
- **Build:** Flash ~67.8%, RAM ~17.5%
- **Layout:** v3 (0-18) — config version 3
- **Protocols:** Art-Net, sACN E1.31, ESP-NOW bridge
- **Last Version:** 1.30.00 (7-Segment Decode Mode toggle for 7/8 pin direct drive)

## Board: WT32-ETH01
- **CPU:** ESP32 dual-core (Core 0 = network/display, Core 1 = application)
- **Flash:** 4MB (partition: app 1.9MB, LittleFS 190KB) — ปัจจุบัน ~65.7%
- **RAM:** 320KB — ปัจจุบัน ~17.5%
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
- `include/output_control.h` — Core: `OutputChannel` struct, `setupChannels()`, `loadChannels()`, `saveChannels()`, DMX processing
- `include/motion_control.h` — `update()` loop for all motion/PWM/DFPlayer types
- `include/scoring.h` — Resource + Compute scoring (limit ~109pts)
- `include/funcgen_control.h` — Function Generator (Type 16) waveform engine using esp_timer + LEDC
- `include/dfplayer_control.h` — DFPlayer MP3 protocol driver
- `include/dimmer_control.h` — AC Dimmer with ZC interrupt
- `include/config.h` — System configuration struct
- `src/main.cpp` — Entry point, HTTP API routes, validation, network tasks
- `include/web_pages.h` — Embedded HTML/CSS/JS for Web UI (auto-generated from `web/index.html`)
- `web/index.html` — Edit this file, run `tools/build_web.py` to regenerate `web_pages.h`

### OutputChannel Struct (output_control.h:98-192)
- `type` (0-18 v3), `source` (0=GPIO, 1=PCA9685, 2=MCP23017, 3=TCA9555, 4=PCF857x, 5=I2C DAC family)
- `pin`-`pin4` with `_source`/`_addr`/`_channel` for expander hybrid
- `dmxBuffer`, `bufferSize` (512 bytes for DMX, smaller for others)
- `pixelStrip` (LED), `dmxPort`/`rmtDmx` (DMX), `dfPlayer` (MP3)
- `ledc_chan2`/`ledc_chan3`/`ledc_chan4` — extra LEDC channels for Motor/RGB
- `seg_pins`, `seg_sources`, `seg_addrs`, `seg_channels` — 8-element arrays for individual segment routing of 7-segment displays (Type 12 / 13) to GPIO or expanders
- Source compatibility (v3): PCA (2,4,5,6,8,12,13,15,17,18), Digital (2,6,17,18), I2C DAC family (14)

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

คะแนนรวมคำนวณจาก 2 ส่วน:
1. **Resource Score** — ทรัพยากรฮาร์ดแวร์ที่แต่ละแชนเนลใช้; contract อยู่ใน `docs/domain_model.md`, implementation อยู่ที่ `scoring.h:estimateResources()`
2. **Compute Score** — CPU load ที่แต่ละ type สร้าง + ตาม FPS (`scoring.h:channelComputeScore()` + `fpsComputeFactor()`)

Resource Score เป็น routing-accurate goal: C++ และ JS ต้องคิดตาม source/mode/per-pin routing จริง รวมถึง hybrid pins, segment-level routing, และ DMX fallback จาก UART ไป RMT หลัง DFPlayer จอง UART แล้ว. Hard validation ยังต้องเช็ค interlock/limit จริงแยกจาก score เสมอ.

Known drift to audit next: `include/scoring.h::totalOutputScoreFromJson()` ต้อง copy routing fields จาก JSON ให้ครบก่อนเรียก `estimateResources()` และต้องเทียบ parity กับ `web/index.html::channelScore()`. Web UI `channelScore()` ยังใช้ global `outputs` ในบาง path และ reserved-pin validation อาจพลาด hybrid GPIO เมื่อ primary source ไม่ใช่ GPIO.

### Resource Weights

| Resource | Weight | คำอธิบาย |
|----------|--------|----------|
| GPIO | 0.5 | ต่อ pin output ปกติ |
| LEDC | 2.5 | ต่อ channel PWM (มี 16 ch. ทั้งหมด) |
| RMT | 3.0 | ต่อ channel (มี 8 ch. ทั้งหมด) |
| UART | 8.0 | ต่อ port (มีแค่ 2 port) |
| DAC | 2.0 | ต่อ channel (GPIO25/26 โดน LAN8720 จองไว้) |
| PCA9685 | 0.25 | ต่อ channel (ถูกเพราะ I2C, มี 16 ch./chip) |
| I2C Expander | 0.125 | ต่อ pin |

### หลักการคิดคะแนนต่อ channel (source=GPIO)

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
| **MCP4725 DAC** | - | - | - | - | - | - | - | 1 (Exp) |

*ตัวเลขเปลี่ยนตาม source ที่เลือก (0=GPIO+LEDC, 1=PCA, 2-4=Expander, 5=MCP4725 DAC)
- 7-Segment Type 12/13 ในโหมด direct drive ต้องคิดคะแนนตาม route ของแต่ละ segment จริง ผ่าน `seg_sources`/`seg_pins` หรือ base routing ไม่ใช่นับ full GPIO/LEDC เสมอ
- DAC Type 14 เมื่อเลือก Source 5 (MCP4725) จะไม่กินขา GPIO/DAC ของ ESP32 แต่คิดเป็น I2C Expander 1 pin (weight 0.125) และ PCA address configuration

### สูตรคำนวณ
```
resourceScore = GPIO×0.5 + LEDC×2.5 + RMT×3.0 + UART×8.0 + DAC×2.0 + PCA×0.25 + EXP×0.125
computeScore  = Σ channelCompute(type) + (fps/60)×5
totalScore    = resourceScore + computeScore
```

รวมทุก channel ต้องไม่เกิน **SCORE_LIMIT ≈ 109** (84 resource + 25 compute)

### Resource Max (ตาม hardware WT32-ETH01)
| Resource | Available | Weight | คะแนนเต็ม |
|----------|-----------|--------|-----------|
| GPIO (output) | 8 usable | ×0.5 | 4.0 |
| LEDC | 16 | ×2.5 | 40.0 |
| RMT | 8 | ×3.0 | 24.0 |
| UART | 2 | ×8.0 | 16.0 |
| DAC | 0 (blocked by ETH) | ×2.0 | 0 |
| **รวม resource max** | | | **84.0** |
| **Compute max** | | | **25.0** |
| **รวม total max** | | | **≈ 109** |

## Computational Scoring Factor

| Type | CPU Points | เหตุผล |
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

FPS Factor: `(fps/60) × 5` — ที่ 40fps (default) = 3.33

### Weight/Compute อยู่ที่ไฟล์ไหน
- **C++:** `scoring.h` — `W_GPIO` etc., `resourceScore()`, `channelComputeScore()`, `fpsComputeFactor()`
- **JS:** `web/index.html` — `channelScore()`, `channelComputeScore()`, `fpsComputeFactor()`, `totalScoreLimit()`
- **สำคัญ:** ถ้าแก้ weight ต้องแก้ทั้ง C++ และ JS ให้ตรงกัน

## Web UI Hardware Monitor

Web UI มีแถบ **Hardware Resource Monitor** (4 cards) อยู่ใต้ Resource Score bar:
- **RMT Channels** (max 8): LED Strip count + DMX channels ที่ล้นเกิน UART → ใช้ RMT
- **UART Ports** (max 2): DFPlayer count + DMX ที่ได้ UART
- **LEDC Channels** (max 16): PWM, Motor, Servo, RGB/RGBW, Buzzer, 7-Seg DD, PWM DAC, Func Gen
- **Total Score** (max ≈109): resource + compute score

สี: 🟢 ปกติ, 🟡 เต็ม limit, 🔴 เกิน limit → ปิดปุ่ม Save อัตโนมัติ

### ตรรกะการนับ UART/RMT (JS: `updateScoreBar()`)
```js
freeUarts = max(0, 2 - dfPlayerCount)
dmxUartUse = min(dmxCount, freeUarts)
dmxRmtUse = max(0, dmxCount - freeUarts)
uartUsed = dfPlayerCount + dmxUartUse
rmtUsed = ledCount + dmxRmtUse
```

## Interlocking System

### 1. GPIO Interlock (Pin Conflict Prevention)

| กฎ | C++ | JS |
|----|-----|-----|
| ห้าม duplicate GPIO ระหว่าง channel | main.cpp:1098-1108 | index.html |
| ห้ามใช้ pin เดียวกับ Status LED | main.cpp:500-519 | index.html |
| ห้ามใช้ pin เดียวกับ ZC pin | main.cpp:1098-1108 | index.html |

เตือนถ้ามี AC Dimmer แต่ ZC pin = 255 (Disabled) — Web UI `#zc-warning`

### 2. PCA9685 Frequency Interlock

PCA9685 แต่ละ chip แชร์ frequency เดียวกันทั้ง chip:
1. **Servo (Type 8) — Priority HIGHEST:** บังคับ 50 Hz
2. **Motor/PWM types:** ใช้ `mc_freq` ของ channel แรกบน chip นั้น
3. **Default:** 1000 Hz

Validation ใน Web UI: ถ้า `mc_freq` ต่างกันบน PCA เดียวกัน → แสดง warning ตอน save

### 3. UART Interlock

- DFPlayer (Type 10) มี priority สูงสุด → UART2 (Serial2) → UART1 (Serial1)
- DMX (Type 1) ใช้ UART ที่เหลือ ถ้าไม่มี → fallback RMT
- Maximum 2 DFPlayer channels (UART limit)

### 4. Smoke Shooter State Machine

IDLE → SMOKE → SETTLE → SHOOT → COOLDOWN → IDLE
- `smoke_lockout_ms` (default 2000ms) ป้องกันยิงซ้ำ
- ดูที่ `output_control.h:updateSmokeShooters()`

---

## Critical Code Patterns — MUST KNOW

1. **i2cMutex** — Every `Wire` operation: `xSemaphoreTake(i2cMutex, pdMS_TO_TICKS(100)) == pdTRUE`
2. **Scoring** — C++ `scoring.h` / JS `channelScore()` + `channelComputeScore()` ต้องตรงกัน ถ้าแก้ weight แก้ทั้งสองไฟล์
3. **Layout versioning** — `/outputs.json` มี `version: 3`, v1→v3 และ v2→v3 migration ใน `loadChannels()`
4. **PCA frequency interlock** — `getPcaSharedFrequency()` — Servo บน chip → บังคับ 50 Hz
5. **GPIO interlock** — validate ทั้ง C++ (`main.cpp`) และ JS (`index.html`)
6. **JS falsy trap** — ใช้ `o.pin4!==undefined?o.pin4:255` แทน `o.pin4||255` เพราะ `0` เป็น falsy
7. **web/index.html** — แก้ที่นี่ แล้วรัน `tools/build_web.py` → `include/web_pages.h`
8. **DFPlayer UART priority** — `setupChannels()` loop หา DFPlayer ก่อน จอง UART ก่อน DMX

## P0 Bugs (all fixed in v1.16.00)

All 8 P0 bugs resolved: dimmer ISR safety, static globals, ESP-NOW queuing, unaligned pointer, calloc NULL check, div-by-zero, HTTP OOM, UART2 conflict (DFPlayer priority + DMX RMT fallback).

## File Map

```
include/
  config.h              SystemConfig, NVS load/save
  output_control.h      Output mapping, DMX/LED rendering, save/load JSON
  scoring.h             Resource + Compute scoring (limit ~109pts)
  web_pages.h           Embedded web UI (HTML+CSS+JS, auto-generated)
  dimmer_control.h      AC dimmer ZC + timer ISR
  motion_control.h      Motor, stepper, servo, 7-seg (11/12/13), DAC, PWM DAC, Buzzer, FuncGen
  dfplayer_control.h    DFPlayer MP3 protocol driver (Type 10)
  funcgen_control.h     Function Generator (Type 16, esp_timer + LEDC waveform engine)
  i2c_gpio_expander.h   MCP23017, TCA9555, PCF857x drivers
  pca9685_control.h     PCA9685 PWM driver
  artnet_control.h      Art-Net UDP listener
  sacn_control.h        sACN E1.31 listener
  espnow_control.h      ESP-NOW master/slave bridge
  rmt_dmx.h             RMT-based DMX TX fallback
  display_driver.h      SSD1306/SH1106 OLED + PCF8574 LCD driver
src/
  main.cpp              Setup, network, web APIs, RTOS tasks, OTA, validation
web/
  index.html            Web UI source — EDIT THIS, then run build_web.py
docs/
  user_manual.md        User-facing manual
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
  - **Segment Routing:** รองรับการตั้งค่าขาแยกอิสระของแต่ละ Segment (A ถึง DP) ไปยังขา GPIO ต่างๆ (`seg_pins`) หรือ Expander ผ่าน `seg_sources`, `seg_addrs`, `seg_channels` แบบผสมกันได้อย่างอิสระ. โหมด Direct Dim (mc_mode 4/5) ต้องใช้ ESP32 GPIO หรือ PCA9685 เท่านั้นเพราะต้อง PWM ราย segment; MCP23017/TCA9555/PCF857x ใช้ได้เฉพาะ No Dim/Common Dim ที่ segment เป็น digital output.
  - **Segment Inversion:** มีปุ่ม `Invert` ให้เลือกสำหรับแต่ละ segment pin แยกจากกันอิสระ โดยค่าจะบันทึกในรูปแบบ bitmask 8 บิต `seg_inverts` และนำไปลอจิกอินเวอร์ตระดับสัญญาณก่อนสั่งขับออกขาทาง C++
  - **Segment A Mapping (Direct Mode):** ในโหมดแยกขา Seg A (modes 2-5) ข้อมูล Seg A จะต้องเชื่อมโยงกับการตั้งค่าพินหลักของช่องสัญญาณ (`no_source`, `no_pca_addr` ฯลฯ) ทั้งขั้นตอนการจัดเก็บและการดึงข้อมูล เพื่อให้การส่งค่าไปยัง expander สำหรับ Seg A ทำงานได้ถูกต้อง
  - **Dimming Modes:**
    - **No Dimming (1 Ch):** 1 DMX channel controls segment char/state at full brightness.
    - **Dimmed Mode (2 Ch):** DMX Channel 1 controls character, DMX Channel 2 controls PWM duty cycle brightness level (0-255).
    - **Common Dimming (6-9):** ใช้สาย Common Anode/Cathode ต่อกับ PWM (LEDC หรือ PCA9685) เพื่อควบคุมความสว่างแบบรวม ส่วน segment อื่นๆ ต่อเป็น digital GPIO/Expander/PCA9685 เพื่อส่งข้อมูลปกติ
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
- **Web UI:** RC filter calculator ใน channel config (ใส่ R/C → คำนวณ fc อัตโนมัติ)
- **Resolution:** 8-16 bit ผ่าน `mc_resolution`, carrier freq via `mc_freq`
- **Calibration:** `pwm_dac_mode` labels Custom/0-10V/4-20mA; `pwm_dac_min` and `pwm_dac_max` store duty percent x100 (0..10000) and map DMX 0..full-scale to the calibrated duty range for external 0-10V or 4-20mA interface circuits.

### 6. Function Generator (Type 16)
- **Engine:** `FuncGenController` ใน `include/funcgen_control.h`
- **Hardware:** esp_timer (high-res timer interrupt) + LEDC (PWM output)
- **DMX 5 Bytes:** Byte 0-1 = Freq (1-10000 Hz), Byte 2 = Waveform, Byte 3 = Amplitude, Byte 4 = DC Offset
- **Waveforms:** 0=Sine, 1=Triangle, 2=Sawtooth, 3=Square, 4=PWM
- **RC Filter Reference:** ใช้ RC filter circuit เพื่อแปลง PWM เป็น analog

```
GPIO (PWM Out) ── R ──── Analog Out
                   |
                   C
                   |
                  GND
```

### 7. I2C DACs (12-bit)
- **MCP4725:** `dac_model=0`, addresses 0x60 and 0x61, single channel
- **DAC7571:** `dac_model=1`, addresses 0x4C and 0x4D, single channel
- **DAC7573:** `dac_model=2`, addresses 0x4C - 0x5B, quad channel; `pca_channel` selects A-D
- **Usage:** Analog Output (Type 14 DAC) ที่ใช้ I2C แทน DAC pin (GPIO25/26) ของ ESP32 เพื่อหลีกเลี่ยงการชนกับ LAN8720 RMII pins
- **Resolution:** 12-bit I2C DAC (ส่งคำสั่ง I2C ด้วยค่า 0-4095 สเกลจาก DMX 0-255)

### 7.1 I2C Displays
- **OLED:** SSD1306/SH1106 addresses 0x3C or 0x3D
- **LCD Backpack:** PCF8574 LCD addresses 0x27 or 0x3F
- **Validation:** Display address must match display type and is separate from output source validation

### 8. DC Motor EN Pin (H-Bridge Mode 2)
- **Motor Mode 2 (IN1+IN2+EN):** ขา EN ต้องใช้ PWM ในการคุมความเร็วรอบ ดังนั้นจะต้องต่อเข้ากับ ESP32 GPIO (ใช้ LEDC) หรือ PCA9685 เท่านั้น ห้ามเลือกต่อกับ Digital Expander (MCP23017/TCA9555/PCF857x) เป็นอันขาด และมี Validation ตรวจสอบเตือนใน Web UI และ C++ firmware

### 9. Analog RGB / RGBW (Type 5) Independent Routing
- **Routing:** แชนเนลสี Red, Green, Blue, และ White (สำหรับ RGBW) สามารถเลือก Source (ESP32 GPIO หรือ PCA9685), Address, และ Pin/Channel ของแต่ละขาแยกกันได้อย่างอิสระเต็มที่ ไม่จำเป็นต้องแชร์ source/address เดียวกันหรือเป็น channel ที่เรียงต่อกันเหมือนเวอร์ชันก่อนๆ
- **Scoring:** Resource scoring คำนวณรายแชนเนลแบบละเอียดตาม source จริงที่เลือกใช้ในแต่ละขา

### 10. Sequential Smoke Shooter (Type 18) & Solenoid Trigger (Type 17) Threshold Configuration
- **Solenoid Trigger (Type 17):** รองรับเฉพาะ **Threshold Mode** เท่านั้น (Frame-Sync mode ถูกนำออกแล้ว) โดยจะยิงพัลส์เมื่อค่า DMX เกินค่า threshold ที่กำหนด (`solenoid_threshold`)
- **Sequential Smoke Shooter (Type 18):** มีช่องให้ตั้งค่า **Threshold Value** (`smoke_threshold` ในหน้า UI บันทึกในรูปแบบ `solenoid_threshold` ใน JSON) โดยเงื่อนไขการทริกเกอร์ลำดับการทำงานจะเช็คจากค่า DMX ที่มากกว่า threshold ที่ผู้ใช้กำหนด แทนค่าคงที่ 127 เดิม
- **Routing:** ขา Smoke Valve (Pin 1) และ Shoot Valve (Pin 2) สามารถเลือก Source (GPIO, PCA9685, หรือ digital expanders), Address, และ Pin/Channel แยกกันได้อย่างอิสระเต็มที่ ช่วยให้ยืดหยุ่นในการจัดสรรพินของวาล์วพ่นควันและวาล์วยิงลม

---

## Developer Tools

- **[build_web.py](file:///c:/Users/natta/Documents/bar_program/esp32_eth01_artnet_device/tools/build_web.py)**
  - Minify `web/index.html` → `include/web_pages.h`
  - ถูกเรียกอัตโนมัติจาก `build_firmware_bin.bat`
- **[extract_web.py](file:///c:/Users/natta/Documents/bar_program/esp32_eth01_artnet_device/tools/extract_web.py)**
  - สกัด HTML กลับออกจาก `web_pages.h` (ใช้เมื่อต้องการ reverse)
- **[web_mock_server.py](file:///c:/Users/natta/Documents/bar_program/esp32_eth01_artnet_device/tools/web_mock_server.py)**
  - จำลอง API REST บน `http://localhost:8000`
  - ใช้ทดสอบ Web UI โดยไม่ต้องอัปโหลดไปบอร์ดจริง
  - Auto-reload: เมื่อ save `web/index.html` browser จะ refresh เองภายในประมาณ 1 วินาที
  - คำสั่งเร็วสำหรับ UI dev: `c:\Users\natta\.platformio\penv\Scripts\python.exe tools/web_mock_server.py` แล้วเปิด `http://localhost:8000`
- **[web_ui_smoke_test.mjs](file:///c:/Users/natta/Documents/bar_program/esp32_eth01_artnet_device/tools/web_ui_smoke_test.mjs)**
  - Automated Web UI smoke test จากโค้ด ไม่ต้องให้ user เปิดหน้าเว็บเอง
  - ใช้ Node built-in + Chrome/Edge headless ผ่าน DevTools Protocol ไม่ต้อง `npm install`
  - ตรวจว่า Web UI render output type ทั้ง 19 แบบได้ และ add Function Generator ได้
  - คำสั่ง: `node tools/web_ui_smoke_test.mjs`
- **[load_calculator.py](file:///c:/Users/natta/Documents/bar_program/esp32_eth01_artnet_device/tools/load_calculator.py)**
  - คำนวณ resource/load แบบ offline จาก outputs.json

## Coding Conventions
- **No comments** in code unless necessary
- Follow existing patterns (look at neighboring code first)
- Use `std::vector<OutputChannel>` for channel storage
- JSON serialization with ArduinoJson 7.x
- Web UI source: `web/index.html` → run `build_web.py` → `web_pages.h`
- Thai language for communication
- **Config Appropriateness:** แต่ละแชนเนลจะต้องซ่อนพารามิเตอร์ที่ไม่เกี่ยวข้องออกไปเสมอ เช่น Solenoid (Type 17) ต้องไม่มีตัวเลือก Resolution และถูกกำหนด DMX byte ขนาด 1 Ch เสมอ เพื่อลดความซ้ำซ้อนและป้องกันความสับสนของผู้ใช้

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
- Commit message format: แนะนำให้ใช้รูปแบบภาษาอังกฤษหรือภาษาไทยที่เข้าใจง่าย เช่น `feat(7seg): add numeric decoding mode` หรือ `fix(ui): fix Segment A PCA routing`

## When Modifying Code
1. Read existing files to understand patterns
2. Check resource limits (UART, LEDC, RMT, GPIO)
3. Ensure expander source compatibility
4. If editing Web UI: edit `web/index.html` → run `build_web.py` → build firmware
5. Read `docs/domain_model.md` -> `Configuration Contract` before changing domain concepts, validation, scoring, or output routing
