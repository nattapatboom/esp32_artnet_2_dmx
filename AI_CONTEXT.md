# AI_CONTEXT.md — WT32-ETH01 Art-Net / sACN Node

Read this first before working on the project.

## Project

Firmware for a WT32-ETH01 lighting node. Receives Art-Net/sACN over Ethernet/Wi-Fi, bridges DMX with ESP-NOW, drives local output devices.

### Output Types (13 types)

| ID | Type | Expander? | Notes |
|:--:|------|:---------:|-------|
| 0 | LED Strip | ❌ | RMT timing critical |
| 1 | DMX Serial | ❌ | UART/RMT |
| 2 | Relay | ✅ PCA+Exp | — |
| 3 | AC Dimmer | ❌ | ISR timing, ZC pin |
| 5 | PWM Dimmer | ✅ PCA only | Needs HW PWM |
| 6 | DC Motor | ✅ Hybrid D/R/EN | Bug: Web UI shows main src expander, server rejects |
| 7 | Stepper | ✅ DIR/EN hybrid | STEP=ESP32 forced |
| 8 | RC Servo | ✅ PCA only | 50Hz PWM |
| 9 | Solenoid | ✅ Expander | — |
| 10 | Analog RGB | ✅ PCA only | 3-4 PWM |
| 11 | Buzzer | ❌ | Variable freq |
| 12 | Smoke Shooter | ✅ PCA+Exp | — |
| 13 | 7-Segment | ✅ DD segs via pin2 | TM1637 on ESP32 only |

## Current State

- **Board:** 192.168.1.93 (WT32-ETH01)
- **Build:** Flash 65.0% (1,277,825B), RAM 17.5% (57,252B)
- **Max outputs:** 16
- **Protocols:** Art-Net, sACN E1.31, ESP-NOW bridge
- **Active handover:** `handover/handover_v1.14.0.md`
- **Improvement plan:** `handover/handover_v1.15.0_improvements.md`

## Critical Code Patterns — MUST KNOW

1. **i2cMutex** — Every `Wire` operation must be wrapped with `xSemaphoreTake(i2cMutex, portMAX_DELAY)` / `xSemaphoreGive(i2cMutex)`
2. **DMX buffer race** — `mapDmxDataToChannels()` (Core 0) writes `ch.dmxBuffer` while `updateLeds()` (Core 1) reads — no mutex
3. **Scoring duplication** — JS `channelScore()` returns `2` for type 5/6/10 but C++ `scoreForType()` returns `3.0` (BUG)
4. **dimmer_control.h** — `static` globals in header (ODR); `digitalWrite()` in ISR (not IRAM-safe)
5. **ESP-NOW onDataRecv** — called in ISR context, calls `mapDmxDataToChannels()` — dangerous

## P0 Bugs (fix first)

| Bug | File:Line |
|---|---|
| `digitalWrite()` in ISR (dimmer timer) | `dimmer_control.h:26-60` |
| static globals in header (ODR) | `dimmer_control.h:11-23` |
| ESP-NOW ISR calling mapDmxDataToChannels | `espnow_control.h:208-237` |
| ESP-NOW unaligned pointer cast | `espnow_control.h:216` |
| `calloc` no NULL check | `output_control.h:491,515,606` |
| Div-by-zero `1000/output_fps` | `output_control.h:865,994` |
| HTTP OOM (no Content-Length limit) | `main.cpp:236-238` |

## File Map

```
include/
  config.h              SystemConfig, NVS load/save
  output_control.h      Output mapping, DMX/LED rendering, save/load JSON
  scoring.h             Dynamic scoring system (100-point limit)
  web_pages.h           Embedded web UI (HTML+CSS+JS, ~2300 lines)
  dimmer_control.h      AC dimmer ZC + timer ISR
  motion_control.h      Stepper, DC motor, servo, solenoid, 7-segment
  i2c_gpio_expander.h   MCP23017, TCA9555, PCF857x drivers
  pca9685_control.h     PCA9685 PWM driver
  artnet_control.h      Art-Net UDP listener
  sacn_control.h        sACN E1.31 listener
  espnow_control.h      ESP-NOW master/slave bridge
  rmt_dmx.h             RMT-based DMX TX fallback
  display_driver.h      SSD1306/SH1106 OLED + PCF8574 LCD driver
src/
  main.cpp              Setup, network, web APIs, RTOS tasks, OTA, validation
docs/
  user_manual.md        User-facing manual
handover/
  handover_v1.12.0_handover.md      Original architecture
  handover_v1.13.0.md               Feature implementation detail
  handover_v1.14.0.md               Session summary (current active)
  handover_v1.15.0_improvements.md  Improvement plan (bugs, safety, code quality)
```

## Runtime Architecture

- **Core 0 — networkTask:** Art-Net, sACN, ESP-NOW, AsyncWebServer, I2C Display update, **I2C Scan (queue-based)**
- **Core 1 — outputTask:** DMX TX, LED output, motion control, output test
- **Storage:** LittleFS (`/outputs.json`, `/espnow_peers.json`), NVS Preferences (settings)

## Hardware Notes

- Avoid GPIO16 (LAN8720A Ethernet power)
- Default status LED: GPIO 5
- Common output GPIOs: 4, 12, 14, 15, 2, 17, 32, 33
- Input-only: 36, 39, 34, 35
- GPIO12 is boot strap pin — careful with pull-up/down
- **ALL GPIO outputs MUST have pull-down resistors** when connecting to interface boards (floating pin protection when controller removed)

## Build

```powershell
& "C:\Users\natta\.platformio\penv\Scripts\platformio.exe" run
```

Windows bin helper: `build_firmware_bin.bat`

OTA upload:
```powershell
curl.exe -F "update=@.pio\build\wt32-eth01\firmware.bin" http://192.168.1.93/update
```

Keep flash below ~75% for OTA margin. Current: 65.0%.
