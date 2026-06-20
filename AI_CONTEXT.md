# AI_CONTEXT.md — WT32-ETH01 Art-Net / sACN Node

Read this first before working on the project.

## Project

Firmware for a WT32-ETH01 lighting node. Receives Art-Net/sACN over Ethernet/Wi-Fi, bridges DMX with ESP-NOW, drives local output devices. Output type layout now uses **v3 (0-18)** with contiguous numbering and grouped categories.

### Output Types (v3 — 0-18)

| ID | Type | Group | DMX Bytes | Source | Resource |
|:--:|------|-------|:---------:|:------:|:--------:|
| 0 | AC Dimmer | Lighting | 1-2 | GPIO | Timer+ZC |
| 1 | DMX Serial Out | Lighting | 512 | GPIO (UART/RMT) | UART |
| 2 | Relay | Lighting | 1 | GPIO/PCA/Exp | GPIO 1 |
| 3 | RGB LED | Lighting | 3-4/led | GPIO (RMT) | RMT 1ch |
| 4 | Single Color LED | Lighting | 1-2 | GPIO/PCA | LEDC 1ch |
| 5 | Analog RGB | Lighting | 3-4 | GPIO/PCA | LEDC 3-4ch |
| 6 | DC Motor | Motion | 1-2 | GPIO/PCA/Exp | LEDC 1-3ch |
| 7 | Stepper | Motion | 3-5 | GPIO+Exp hybrid | FastAccel |
| 8 | RC Servo | Motion | 1 | GPIO/PCA | LEDC 1ch |
| 9 | Buzzer | Audio | 2 | GPIO | LEDC 1ch |
| 10 | DFPlayer MP3 | Audio | 3 | GPIO (UART) | UART |
| 11 | 7-Seg 2-Pin | Display | 2-4 | GPIO/Exp | GPIO 2 |
| 12 | 7-Seg DD 7-Pin PWM | Display | 1 | GPIO (LEDC) | LEDC 7ch |
| 13 | 7-Seg DD 8-Pin PWM | Display | 1 | GPIO (LEDC) | LEDC 8ch |
| 14 | DAC | Analog Out | 1 | GPIO (25/26) | DAC |
| 15 | PWM DAC | Analog Out | 1-2 | GPIO/PCA | LEDC 1ch |
| 16 | Function Gen | Signal | 5 | GPIO | LEDC 1ch+Tmr |
| 17 | Solenoid | Trigger | 1 | GPIO/Exp | GPIO 1 |
| 18 | Smoke Shooter | Trigger | 1 | GPIO/PCA/Exp | GPIO 2 |

## Current State

- **Board:** 192.168.1.93 (WT32-ETH01) — ONLINE
- **Build:** Flash ~66.6%, RAM ~17.5%
- **Protocols:** Art-Net, sACN E1.31, ESP-NOW bridge
- **Layout:** v3 (0-18) — migrated from v2, config version 3
- **Last feature:** v3 Layout Migration (types renumbered 0-18 with grouped categories)
- **Active handover:** `handover/1.23.00.md`
- **Skill:** `.opencode/skills/esp32-firmware/SKILL.md`

## Critical Code Patterns — MUST KNOW

1. **i2cMutex** — Every `Wire` operation must be wrapped with `xSemaphoreTake(i2cMutex, pdMS_TO_TICKS(100))` (100ms timeout, no longer `portMAX_DELAY`)
2. **Scoring duplication** — Some JS `channelScore()` values may differ from C++ `scoreForType()` — verify when adding new types
3. **Layout versioning** — Config file `/outputs.json` has `version: 3` with v1→v3 and v2→v3 migration paths
4. **Dead code cleanup** — v1.x deprecated types removed, no backward compat needed

## P0 Bugs (all fixed in v1.16.00)

All 8 P0 bugs resolved: dimmer ISR safety, static globals, ESP-NOW queuing, unaligned pointer, calloc NULL check, div-by-zero, HTTP OOM, UART2 conflict (DFPlayer priority + DMX RMT fallback).

## File Map

```
include/
  config.h              SystemConfig, NVS load/save
  output_control.h      Output mapping, DMX/LED rendering, save/load JSON
  scoring.h             Dynamic scoring system (100-point limit)
  web_pages.h           Embedded web UI (HTML+CSS+JS)
  dimmer_control.h      AC dimmer ZC + timer ISR
  motion_control.h      Motor, stepper, servo, 7-seg (types 11/12/13), DAC, PWM DAC, Buzzer, FuncGen
  dfplayer_control.h    DFPlayer MP3 protocol driver (Type 10)
  funcgen_control.h     Function Generator (Type 16)
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
  README.md             Handover system rules
  1.23.00.md            Current — v3 layout migration
  archive/              Drafts and backups
.opencode/
  skills/
    esp32-firmware/
      SKILL.md          Project skill for agent context
tools/
  build_web.py          Minify web/index.html → web_pages.h
  extract_web.py        Extract web_pages.h → web/index.html
  web_mock_server.py    Mock REST API for web UI dev
```

## Runtime Architecture

- **Core 0 — networkTask:** Art-Net, sACN, ESP-NOW, AsyncWebServer, I2C Display update, I2C Scan (queue-based)
- **Core 1 — outputTask:** DMX TX, LED output, motion control, output test
- **Storage:** LittleFS (`/outputs.json`, `/espnow_peers.json`), NVS Preferences (settings)

## Hardware Notes

- Avoid GPIO16 (LAN8720A Ethernet power)
- Default status LED: GPIO 5
- Common output GPIOs: 4, 12, 14, 15, 2, 17, 32, 33
- Input-only: 36, 39, 34, 35
- GPIO12 is boot strap pin — careful with pull-up/down
- **ALL GPIO outputs MUST have pull-down resistors** when connecting to interface boards
- DAC (Type 14) GPIO25/26 are used by LAN8720 RMII on WT32-ETH01 — unavailable

## Build

```powershell
& "C:\Users\natta\.platformio\penv\Scripts\platformio.exe" run
```

Windows bin helper: `build_firmware_bin.bat`

OTA upload:
```powershell
curl.exe -F "update=@.pio\build\wt32-eth01\firmware.bin" http://192.168.1.93/update
```

Keep flash below ~75% for OTA margin. Current: **~66.6%**.
