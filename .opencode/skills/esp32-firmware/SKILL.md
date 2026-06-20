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

### Current State
- **Board:** 192.168.1.93 (WT32-ETH01) — ONLINE
- **Build:** Flash ~65.7%, RAM ~17.5%
- **Layout:** v3 (0-18) — config version 3
- **Protocols:** Art-Net, sACN E1.31, ESP-NOW bridge

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

## Output Types (v3 — 0-18)

| Type | Name | DMX Bytes | Source Support | Resource |
|------|------|-----------|----------------|----------|
| 0 | AC Dimmer (TRIAC) | 1-2 | GPIO only | GPIO 1 + ZC Timer |
| 1 | DMX Serial Output | 512 | GPIO (UART1/2 or RMT) | UART 1-2 |
| 2 | Relay (On/Off) | 1 | GPIO, PCA, Expander | GPIO or PCA/Exp 1 |
| 3 | RGB LED (NeoPixel) | 3-4/led | GPIO only (RMT) | RMT 1ch |
| 4 | Single Color LED (PWM) | 1-2 | GPIO, PCA | GPIO+LEDC or PCA 1 |
| 5 | Analog RGB/RGBW | 3-4 | GPIO, PCA | LEDC 3-4ch |
| 6 | DC Motor (H-Bridge) | 1-2 | GPIO, PCA, Expander | LEDC 1-3ch |
| 7 | Stepper Motor | 3-5 | GPIO+Expander hybrid | FastAccelStepper (2 RMT) |
| 8 | RC Servo | 1 | GPIO, PCA | LEDC 1ch (50Hz) |
| 9 | Passive Buzzer | 2 | GPIO only | LEDC 1ch |
| 10 | DFPlayer MP3 Module | 3 | GPIO only (UART) | UART 1 |
| 11 | 7-Segment 2-Pin (TM1637) | 2-4 | GPIO, Expander | GPIO 2 |
| 12 | 7-Segment DD 7-Pin PWM | 1 (Direct) | GPIO only | LEDC 7ch |
| 13 | 7-Segment DD 8-Pin PWM | 1 (Direct) | GPIO only | LEDC 8ch |
| 14 | DAC (Analog Out) | 1 | GPIO only (GPIO25/26) | DAC 1ch (blocked on WT32-ETH01) |
| 15 | PWM DAC (RC Filter) | 1-2 | GPIO, PCA | LEDC 1ch |
| 16 | Function Generator | 5 | GPIO only | LEDC 1ch + esp_timer |
| 17 | Solenoid Trigger | 1 | GPIO, Expander | GPIO 1 |
| 18 | Sequential Smoke Shooter | 1 | GPIO, PCA, Expander | GPIO 2 |

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
- `type` (0-18 v3), `source` (0=GPIO, 1=PCA9685, 2=MCP23017, 3=TCA9555, 4=PCF857x)
- `pin`-`pin4` with `_source`/`_addr`/`_channel` for expander hybrid
- `dmxBuffer`, `bufferSize` (512 bytes for DMX, smaller for others)
- `pixelStrip` (LED), `dmxPort`/`rmtDmx` (DMX), `dfPlayer` (MP3)
- `ledc_chan2`/`ledc_chan3`/`ledc_chan4` — extra LEDC channels for Motor/RGB
- Source compatibility (v3): PCA (2,4,5,6,8,15,18), Digital (2,6,17,18)

### Hardware Resource Limits
- **UART:** 3 total. UART0=console. UART1 and UART2 are dynamically allocated. **DFPlayer (Type 10) has priority on UART** (assigns to UART2, then UART1) because DMX can dynamically fall back to RMT.
- **LEDC:** 16 channels max (Type 4, 5, 6, 8, 9, 12, 13, 15, 16)
- **RMT:** 8 channels max (Type 3 LED strips + extra DMX fallback). Total RMT channels cannot exceed 8.
- **Timer:** 4 total (AC dimmer uses 1, Function Generator uses 1 per channel)
- **I2C:** 1 bus (Wire), shared by all expanders + display
- **Dynamic UART Allocation:** DFPlayer is allocated first. DMX uses any remaining UART. If none are free, DMX falls back to RMT driver automatically.

### System Security & Stability
- **HTTP DoS Protection:** Rejects any incoming HTTP requests with bodies exceeding 64KB to prevent OOM attacks.
- **Settings Sanitization:** Validates `output_fps` to be between 1 and 100 on loading and saving.
- **Memory Allocation Safety:** Performs NULL checks on all `calloc()` calls allocating DMX buffers.
- **Interrupt Storm Rate Limiting:** Enforces a 5ms rate limit on zero-cross interrupts.
- **Core-Safe Deferral:** ESP-NOW callback pushes packets into a FreeRTOS queue, deferring processing to Core 1 `outputTask`.
- **Atomic Flags:** `std::atomic<bool>` for `networkFramePending`, `newRxData`.
- **I2C Hang Protection:** 100ms timeout on `i2cMutex` takes (`xSemaphoreTake(i2cMutex, pdMS_TO_TICKS(100)) == pdTRUE`).
- **Bootstrapping Pin Warning:** Web UI warns if GPIO 12 (MTDI) is configured in any output/LED/ZC/I2C slot.
- **Configuration Layout Versioning:** Config version 3 with v1→v3 and v2→v3 migration paths in `loadChannels()`.

## Resource + Compute Scoring System (v3)

คะแนนรวมคำนวณจาก 2 ส่วน:
1. **Resource Score** — ทรัพยากรฮาร์ดแวร์ที่แต่ละแชนเนลใช้ (`scoring.h:estimateResources()`)
2. **Compute Score** — CPU load ที่แต่ละ type สร้าง + ตาม FPS (`scoring.h:channelComputeScore()` + `fpsComputeFactor()`)

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
| 7 Stepper | 2-3 | - | 2 | - | - | - | 2 |
| 8 Servo | 1* | 1* | - | - | - | 1* | - |
| 9 Buzzer | 1 | 1 | - | - | - | - | - |
| 10 DFPlayer | 2 | - | - | 1 | - | - | - |
| 11 7-Seg 2-Pin | 2 | - | - | - | - | - | 2 |
| 12 7-Seg 7-Pin | 7 | 7 | - | - | - | - | - |
| 13 7-Seg 8-Pin | 8 | 8 | - | - | - | - | - |
| 14 DAC | 1 | - | - | - | 1 | - | - |
| 15 PWM DAC | 1* | 1* | - | - | - | 1* | - |
| 16 Func Gen | 1 | 1 | - | - | - | - | - |
| 17 Solenoid | 1* | - | - | - | - | - | 1* |
| 18 Smoke | 2* | - | - | - | - | 2* | 2* |

*ตัวเลขเปลี่ยนตาม source ที่เลือก (0=GPIO+LEDC, 1=PCA, 2-4=Expander)

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
  web_mock_server.py    Mock REST API for web UI dev (http://localhost:5000)
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

### 3. TM1637 (4-Digit 7-Segment Display)
- **Interface:** 2-wire serial (CLK, DIO)
- **Usage:** ตัวเลข/เวลา — TM1637 mode (Type 11, mc_mode 0)

### 4. MCP23017 / TCA9555 / PCF857x (I2C GPIO Expander)
- **Interface:** I2C (Address 0x20 - 0x27)
- **Usage:** Digital outputs (Relay, Solenoid, Smoke), Stepper limit switch input
- **Protection:** I2C Hang Protection 100ms timeout

### 5. PWM DAC (Type 15) — RC Low-Pass Filter
- **Circuit:** GPIO (LEDC PWM) → R → C → Analog Out
- **Web UI:** RC filter calculator ใน channel config (ใส่ R/C → คำนวณ fc อัตโนมัติ)
- **Resolution:** 8-16 bit ผ่าน `mc_resolution`, carrier freq via `mc_freq`

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

---

## Developer Tools

- **[build_web.py](file:///c:/Users/natta/Documents/bar_program/esp32_eth01_artnet_device/tools/build_web.py)**
  - Minify `web/index.html` → `include/web_pages.h`
  - ถูกเรียกอัตโนมัติจาก `build_firmware_bin.bat`
- **[extract_web.py](file:///c:/Users/natta/Documents/bar_program/esp32_eth01_artnet_device/tools/extract_web.py)**
  - สกัด HTML กลับออกจาก `web_pages.h` (ใช้เมื่อต้องการ reverse)
- **[web_mock_server.py](file:///c:/Users/natta/Documents/bar_program/esp32_eth01_artnet_device/tools/web_mock_server.py)**
  - จำลอง API REST บน `http://localhost:5000`
  - ใช้ทดสอบ Web UI โดยไม่ต้องอัปโหลดไปบอร์ดจริง
- **[load_calculator.py](file:///c:/Users/natta/Documents/bar_program/esp32_eth01_artnet_device/tools/load_calculator.py)**
  - คำนวณ resource/load แบบ offline จาก outputs.json

## Coding Conventions
- **No comments** in code unless necessary
- Follow existing patterns (look at neighboring code first)
- Use `std::vector<OutputChannel>` for channel storage
- JSON serialization with ArduinoJson 7.x
- Web UI source: `web/index.html` → run `build_web.py` → `web_pages.h`
- Thai language for communication

## Build & Deploy
```powershell
# Build
pio run

# OTA upload
curl.exe -F "update=@.pio\build\wt32-eth01\firmware.bin" http://192.168.1.93/update

# Regenerate web_pages.h after editing web/index.html
c:\Users\natta\.platformio\penv\Scripts\python.exe tools/build_web.py

# Or use batch file (runs build_web.py + pio run + timestamp .bin)
build_firmware_bin.bat
```

## Handover System
- Docs in `handover/` folder, format `x.xx.xx.md`
- Current: [1.23.00.md](file:///c:/Users/natta/Documents/bar_program/esp32_eth01_artnet_device/handover/1.23.00.md)
- Rules: no editing old files, max 1000 lines per file
- SEE `handover/README.md` for full system rules

## When Modifying Code
1. Read existing files to understand patterns
2. Check resource limits (UART, LEDC, RMT, GPIO)
3. Ensure expander source compatibility
4. If editing Web UI: edit `web/index.html` → run `build_web.py` → build firmware
5. Update handover if adding significant features
6. Read this SKILL.md first for full project context
