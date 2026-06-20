# AGENTS.md — ESP32 Art-Net Firmware

## Build & Deploy
```powershell
# Build
pio run

# OTA upload
curl.exe -F "update=@.pio\build\wt32-eth01\firmware.bin" http://192.168.1.93/update

# Regenerate web_pages.h after editing web/index.html
python tools/build_web.py
```

## Project Structure
- `include/` — C++ headers (output_control.h, motion_control.h, scoring.h, etc.)
- `src/main.cpp` — Entry point, HTTP API, RTOS tasks, validation
- `web/index.html` — Web UI (edit this, then run build_web.py)
- `include/web_pages.h` — Auto-generated from web/index.html
- `.opencode/skills/esp32-firmware/SKILL.md` — Full project context

## Key Coding Rules
- **No comments** in code unless absolutely necessary
- Follow existing patterns (look at neighboring code first)
- C++ → ArduinoJson 7.x, std::vector<OutputChannel>
- ESP32 dual-core: Core 0 (network), Core 1 (output)

## Resource Scoring (Resource-Based)
Score = GPIO×0.5 + LEDC×2.5 + RMT×3.0 + UART×8.0 + DAC×2.0 + PCA×0.25 + EXP×0.125
- Limit: SCORE_LIMIT = 100
- C++: `scoring.h` → `resourceScore()`, `estimateResources()`
- JS: `web/index.html` → `channelScore()`
- Weight constants must match between C++ and JS

## Resource Limits
- LEDC: 16 channels max
- RMT: 8 channels max
- UART: 3 total (UART0=console, UART1/2 dynamically allocated)
- Timer: 4 total (AC dimmer uses 1)
- I2C: 1 bus (Wire), shared by all expanders + display

## Output Types (v3 — 0 to 18)
0=AC Dimmer, 1=DMX, 2=Relay, 3=RGB LED, 4=Single LED, 5=Analog RGB,
6=Motor, 7=Stepper, 8=Servo, 9=Buzzer, 10=DFPlayer,
11=7-Seg TM1637, 12=7-Seg DD 7-Pin PWM, 13=7-Seg DD 8-Pin PWM,
14=DAC, 15=PWM DAC, 16=Func Gen, 17=Solenoid, 18=Smoke Shooter

## Interlocking — MUST VALIDATE
1. **GPIO**: no duplicate pins, no conflict with Status LED (GPIO 5) or ZC pin
2. **PCA9685**: shared frequency per chip — Servo forces 50 Hz
3. **UART**: DFPlayer has priority → UART2 → UART1; DMX falls back to RMT
4. **Validation in both**: C++ (`main.cpp:1098-1125`) + JS (`index.html:2292-2309`)

## Critical Patterns
- Every `Wire` operation → `xSemaphoreTake(i2cMutex, pdMS_TO_TICKS(100))`
- `networkFramePending` → use `exchange(false)` (atomic)
- Config version is 3 with v1→v3 and v2→v3 migration paths
- DAC Type 14 uses GPIO25/26 which are LAN8720 RMII pins on WT32-ETH01 — unavailable
- All GPIO outputs need pull-down resistors when connecting to interface boards
