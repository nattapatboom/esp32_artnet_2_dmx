---
name: esp32-firmware
description: Full-stack ESP32 Art-Net firmware project (WT32-ETH01) — PlatformIO, 16 output types, DMX/Art-Net control, Web UI, OTA deploy
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

## Board: WT32-ETH01
- **CPU:** ESP32 dual-core (Core 0 = network/display, Core 1 = application)
- **Flash:** 4MB (partition: app 1.9MB, LittleFS 190KB) — ปัจจุบัน ~65.1%
- **RAM:** 320KB — ปัจจุบัน ~17.5%
- **Usable GPIO:** 4, 12, 14, 15, 2, 17, 32, 33 (+ input-only 36, 39, 34, 35)
- **Built-in:** Ethernet (LAN8720), no WiFi needed in Ethernet mode

## Output Types (0-15)

| Type | Name | DMX Bytes | Source Support | Resource |
|------|------|-----------|----------------|----------|
| 0 | LED Strip (NeoPixel) | 3-4/led | GPIO only (RMT) | RMT 1ch |
| 1 | DMX Serial Output | 512 | GPIO (UART1/2 or RMT) | UART 1-2 |
| 2 | Relay (On/Off) | 1 | GPIO, PCA, Expander | GPIO 1 |
| 3 | AC Dimmer (TRIAC) | 1-2 | GPIO only | Timer + ZC |
| 5 | DC PWM Dimmer | 1-2 | GPIO, PCA | LEDC 1-2ch |
| 6 | DC Motor (H-Bridge) | 1-2 | GPIO, PCA, Expander | LEDC 1-3ch |
| 7 | Stepper Motor | 3-5 | GPIO+Expander hybrid | FastAccelStepper |
| 8 | RC Servo | 1 | GPIO, PCA | LEDC 1ch |
| 9 | Solenoid Trigger | 1 | GPIO, Expander | GPIO 1 |
| 10 | Analog RGB/RGBW | 3-4 | GPIO, PCA | LEDC 3-4ch |
| 11 | Passive Buzzer | 2 | GPIO only | LEDC 1ch |
| 12 | Sequential Smoke Shooter | 1 | GPIO, Expander | GPIO 2 |
| 13 | 7-Segment Display | 2-4 | GPIO, Expander | GPIO 2-8 |
| 15 | DFPlayer MP3 Module | 3 | GPIO only (UART2) | UART2 |

## Architecture

### Key Files
- `include/output_control.h` — Core: `OutputChannel` struct, `setupChannels()`, `loadChannels()`, `saveChannels()`, DMX processing
- `include/motion_control.h` — `update()` loop for all motion/PWM/DFPlayer types
- `include/web_pages.h` — Embedded HTML/CSS/JS for Web UI (~2350 lines)
- `include/scoring.h` — Resource scoring system (limit 100pts)
- `include/dfplayer_control.h` — DFPlayer MP3 protocol driver
- `include/dimmer_control.h` — AC Dimmer with ZC interrupt
- `include/config.h` — System configuration struct
- `src/main.cpp` — Entry point, HTTP API routes, validation, network tasks

### OutputChannel Struct (output_control.h:98-189)
- `type` (0-15), `source` (0=GPIO, 1=PCA9685, 2=MCP23017, 3=TCA9555, 4=PCF857x)
- `pin`-`pin4` with `_source`/`_addr`/`_channel` for expander hybrid
- `dmxBuffer`, `bufferSize` (512 bytes for all non-LED types)
- `pixelStrip` (LED), `dmxPort`/`rmtDmx` (DMX), `dfPlayer` (MP3)
- Source compatibility: PCA (2,5,6,7,8,10,12), Digital (2,6,9,12)

### Hardware Resource Limits
- **UART:** 3 total. UART0=console. UART1 and UART2 are dynamically allocated. **DFPlayer has priority on UART** (assigns to UART2, then UART1) because DMX can dynamically fall back to RMT.
- **LEDC:** 8 channels max (PWM dimmer, motor, servo, RGB, buzzer)
- **RMT:** 8 channels max (LED strips, extra DMX fallback). Total RMT channels (LEDs + extra DMX fallback) cannot exceed 8.
- **Timer:** 4 total (AC dimmer uses 1)
- **I2C:** 1 bus (Wire), shared by all expanders + display
- **Dynamic UART Allocation:** DFPlayer is allocated first. DMX uses any remaining UART. If none are free, DMX falls back to RMT driver automatically.

### System Security & Stability
- **HTTP DoS Protection:** Rejects any incoming HTTP requests with bodies exceeding 64KB to prevent OOM attacks.
- **Settings Sanitization:** Validates `output_fps` to be between 1 and 100 on loading and saving, preventing division-by-zero crashes.
- **Memory Allocation Safety:** Performs NULL checks on all `calloc()` calls allocating DMX buffers.
- **Interrupt Storm Rate Limiting:** Enforces a 5ms rate limit on zero-cross interrupts to avoid lockups when the Zero-Cross pin floats.
- **Core-Safe Deferral:** ESP-NOW callback pushes packets into a FreeRTOS queue, deferring the thread-unsafe `mapDmxDataToChannels()` processing to the Core 1 `outputTask` thread.

## Coding Conventions
- **No comments** in code unless necessary
- Follow existing patterns (look at neighboring code first)
- Use `std::vector<OutputChannel>` for channel storage
- JSON serialization with ArduinoJson 7.x
- Web UI in embedded HTML string in web_pages.h
- Thai language for communication

## Build & Deploy
```powershell
# Build
pio run

# OTA upload
curl.exe -F "update=@.pio\build\wt32-eth01\firmware.bin" http://192.168.1.93/update
```

## Handover System
- Docs in `handover/` folder, format `x.xx.xx.md`
- Current: `handover/1.15.00.md`
- Rules: no editing old files, max 1000 lines per file
- SEE `handover/README.md` for full system rules

## When Modifying Code
1. Read existing files to understand patterns
2. Check resource limits (UART, LEDC, RMT, GPIO)
3. Ensure expander source compatibility
4. Build before OTA
5. Update handover if adding significant features
