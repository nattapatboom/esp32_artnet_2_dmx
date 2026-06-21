# AGENTS.md — ESP32 Art-Net Firmware

## AI Agent Persona & Expertise Profile

When working on this project, the developer and the AI agent collaborate as elite systems architects and firmware engineers with deep expertise in the following domains:
- **Networking:** Ethernet, Wi-Fi, TCP/IP, UDP socket management, broadcast/multicast routing, network failovers, and latency optimization.
- **Lighting Protocols:** Art-Net, sACN (E1.31) unicast/multicast, ESP-NOW wireless bridge chunking, and physical DMX512 serial protocols.
- **Embedded Systems (ESP32):** FreeRTOS dual-core task distribution, hardware interrupts, timer ISRs, I2C bus mutex synchronization, hardware UARTs, LEDC (PWM), RMT peripherals, and LittleFS/NVS storage.
- **Development Languages:** High-performance, memory-safe embedded C++ (Arduino/ESP-IDF framework), Python (for build/mock scripts), and HTML/CSS/JS (for the configuration Web UI).
- **Lighting & Stage Hardware Integration:** WS281x pixels, DC motors (PWM H-Bridges), stepper motor drivers (Step/Dir/Enable lines), RC servos, TRIAC phase-angle dimmers (with zero-crossing timing), solenoids, and sequential stage smoke shooters.

You must design solutions that are hardware-safe, core-safe, and optimized for real-time lighting constraints.

## AI Communication Rules

- **Ask Questions One by One:** If you have any doubts, questions, or need clarification regarding requirements, domain logic, or implementation details, ask the user **one question at a time** to get clear, targeted feedback.
- **Immediate Execution on Clear Tasks:** For any tasks or portions of the work that are already clear and unambiguous, proceed with the implementation immediately. Do not stall or wait for approval on clear steps.
- **Language Policy:** Write all project documentation, system contexts, code comments, commit messages, and planning artifacts (e.g., `implementation_plan.md`, `task.md`, `walkthrough.md`) in **English** for consistency and optimal AI model processing. However, communicate with the user in the chat interface using a mix of **Thai and English** (ไทย/English) as requested.

## Project Context

WT32-ETH01 firmware for receiving Art-Net, sACN, or ESP-NOW DMX data and driving physical outputs: DMX, LED strips, relays, dimmers, motors, steppers, servos, buzzers, audio triggers, 7-segment displays, DAC/PWM DAC, function generator, solenoids, and smoke shooter.

Read these first when working on the project:
- `docs/domain_model.md` — domain vocabulary, bounded contexts, invariants, and Configuration Contract
- `docs/resource_calculator.md` — current resource scoring and hard hardware limits
- `.agents/skills/esp32-firmware/SKILL.md` — full AI working context for this firmware

## Build & Deploy

```powershell
# Build, if pio is in PATH
pio run

# Build, reliable local path on this machine
& "C:\Users\natta\.platformio\penv\Scripts\platformio.exe" run

# Regenerate web_pages.h after editing web/index.html
& "C:\Users\natta\.platformio\penv\Scripts\python.exe" tools/build_web.py

# Automated Web UI smoke test
node tools/web_ui_smoke_test.mjs

# OTA upload
curl.exe -F "update=@.pio\build\wt32-eth01\firmware.bin" http://192.168.1.93/update
```

## Project Structure

- `include/` — C++ headers and most firmware subsystems
- `src/main.cpp` — setup, HTTP API, validation, network/display tasks
- `web/index.html` — Web UI source; edit this file, then run `tools/build_web.py`
- `include/web_pages.h` — generated embedded Web UI; do not edit directly
- `docs/domain_model.md` — project/domain context
- `docs/resource_calculator.md` — hardware scoring and constraints
- `docs/user_manual/` — Typst user manual source (English-first, modular chapters, CeTZ/Fletcher diagrams)
- `tools/` — Web UI build/test/mock tools and offline load calculator
- `.agents/skills/esp32-firmware/SKILL.md` — opencode skill for this project

## Current Architecture

- Board: WT32-ETH01, ESP32 dual-core, LAN8720 Ethernet
- Runtime split: Core 0 handles network/web/display; Core 1 handles outputs
- Storage: LittleFS for `/outputs.json` and `/espnow_peers.json`; NVS `Preferences` for system settings
- Config layout: version 3 with v1 to v3 and v2 to v3 migration paths
- Web UI source of truth: `web/index.html`; generated header: `include/web_pages.h`

## Domain Model

- Main entity: `OutputChannel` in `include/output_control.h`
- System settings: `SystemConfig` in `include/config.h`
- Output types: v3 type IDs `0..18`
- Source IDs: `0=GPIO`, `1=PCA9685`, `2=MCP23017`, `3=TCA9555`, `4=PCF857x`, `5=MCP4725 DAC`
- Source/routing/scoring source of truth: `docs/domain_model.md` -> `Configuration Contract`
- Validation/interlock source of truth: C++ in `src/main.cpp` and matching JS in `web/index.html`

## Output Types

0=AC Dimmer, 1=DMX, 2=Relay, 3=RGB LED, 4=Single LED, 5=Analog RGB,
6=Motor, 7=Stepper, 8=Servo, 9=Buzzer, 10=DFPlayer,
11=7-Seg TM1637, 12=7-Seg DD 7-Pin PWM, 13=7-Seg DD 8-Pin PWM,
14=DAC, 15=PWM DAC, 16=Func Gen, 17=Solenoid, 18=Smoke Shooter

## Resource Scoring

Score = GPIO*0.5 + LEDC*2.5 + RMT*3.0 + UART*8.0 + DAC*2.0 + PCA*0.25 + EXP*0.125

- C++: `include/scoring.h` -> `estimateResources()`, `resourceScore()`, `channelComputeScore()`, `totalCombinedScore()`
- JS: `web/index.html` -> matching score functions
- Limit: `SCORE_LIMIT = resourceScoreLimit() + MAX_COMPUTE_SCORE`, currently about 109
- Weight constants and compute rules must match between C++ and JS

## Hard Resource Limits

- LEDC: 16 channels max
- RMT: 8 channels max
- UART: UART0 is console; UART1/2 are usable by DFPlayer and DMX
- DFPlayer has UART priority; DMX falls back to RMT when UARTs are exhausted
- I2C: one shared `Wire` bus for expanders, MCP4725, and display
- DAC type 14 cannot use ESP32 GPIO25/26 on WT32-ETH01 because LAN8720 uses those pins; prefer MCP4725

## Interlocks To Preserve

- GPIO cannot duplicate another output pin
- Output pins cannot overlap Status LED, Zero-Crossing, I2C SDA, or I2C SCL
- PCA9685 has one shared frequency per chip; Servo forces 50 Hz
- Stepper STEP must be ESP32 GPIO
- Motor EN in `IN1+IN2+EN` mode must be ESP32 GPIO or PCA9685, not digital expander
- Validate every config rule in both C++ API and Web UI when user input is involved

## Coding Rules

- Keep changes minimal and follow neighboring patterns
- No code comments unless they explain non-obvious hardware/runtime behavior
- Use ArduinoJson 7.x patterns already present in the code
- Use `std::vector<OutputChannel>` for channel storage
- Every `Wire` operation must be protected by `xSemaphoreTake(i2cMutex, pdMS_TO_TICKS(100))`
- Use `networkFramePending.exchange(false)` for the atomic pending-frame flag
- If editing Web UI, edit `web/index.html`, regenerate `include/web_pages.h`, then build

## Typst User Manual

- **Location:** Source files are under `docs/user_manual/`. The entry point is `main.typ`, which imports modular chapters from `docs/user_manual/chapters/`.
- **Diagrams:** Use native Typst/CeTZ/Fletcher libraries for high-quality, scalable vector graphics. Avoid external image binaries. Use verified compatible package versions (e.g., `@preview/cetz:0.5.2` and `@preview/fletcher:0.5.8` for Typst 0.15.0).
- **Language Roadmap:** Keep the manual in English first. Thai translation context is planned for the future.
- **Compilation:** Compile using the python script or batch file:
  ```powershell
  # Compile via python
  python tools/build_manual.py
  ```

## Git

- Automatically git commit after making edits/changes.
- Before committing, inspect `git status` and `git diff` to ensure you stage only the modified files, then write a concise, descriptive commit message.
