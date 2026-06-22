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
- `docs/domain_model.md` — domain vocabulary, bounded contexts, invariants, Configuration Contract, device modes, thread safety, and ADRs
- `docs/resource_calculator.md` — current resource scoring and hard hardware limits
- `docs/hardware_guidelines.md` — ESD protection, flyback diode, datasheet checklist, verification guidelines
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
```

### OTA Upload

The device IP is stored in `test_device_ip.txt` as `IP=192.168.1.93`:

```powershell
# Read IP and upload via PowerShell:
$ip = (Get-Content test_device_ip.txt | Select-String "^IP=" | ForEach-Object { $_.Line.Split('=')[1] }).Trim(); curl.exe -F "update=@.pio\build\wt32-eth01\firmware.bin" "http://$ip/update"

# Or use the helper batch script:
.\flash_firmware_ota_by_ip.bat
```

## Project Structure

- `include/` — C++ headers and most firmware subsystems
- `include/config.h` — SystemConfig, NVS load/save
- `include/espnow_control.h` — ESP-NOW master/slave bridge
- `include/motion_control.h` — thin coordinator delegating motion/PWM/audio output types to `output_devices/`
- `include/ota_control.h` — OTA update state/task declarations
- `include/recovery_control.h` — recovery mode constants/state declarations
- `include/scoring.h` — hardware/CPU/RAM scoring logic
- `include/output_devices/` — one file per output device type:
  - `ledc_helpers.h` — shared LEDC allocation, DMX value, segment, PWM DAC calibration helpers
  - `dimmer.h` — Type 0 AC Dimmer setup/update (ZC + timer ISR)
  - `rmt_dmx.h` — RMT-based DMX TX fallback
  - `dmx.h` — Type 1 DMX Output setup/update (UART/RMT)
  - `relay.h` — Type 2 Relay setup/update
  - `led_strip.h` — Type 3 RGB/RGBW LED Strip setup/update (NeoPixelBus/RMT)
  - `single_led.h` — Type 4 Single Color LED setup/update
  - `analog_rgb.h` — Type 5 Analog RGB LED setup/update
  - `motor.h` — Type 6 DC Motor setup/update (PWM+PWM, PWM+DIR, IN1+IN2+EN)
  - `stepper.h` — Type 7 Stepper Motor setup/update (FastAccelStepper)
  - `servo.h` — Type 8 RC Servo setup/update
  - `buzzer.h` — Type 9 Passive Buzzer setup/update
  - `dfplayer_control.h` — DFPlayer MP3 protocol driver
  - `dfplayer.h` — Type 10 DFPlayer MP3 update
  - `seven_seg_digits.h` — shared 7-segment digit lookup table
  - `seven_seg.h` — Types 11/12/13 7-Segment setup/update (TM1637 + DD via GPIO/PCA/EXP)
  - `dac.h` — Type 14 I2C DAC update (MCP4725, DAC7571, DAC7573)
  - `pwm_dac.h` — Type 15 PWM DAC setup/update
  - `funcgen_control.h` — Function Generator waveform engine
  - `funcgen.h` — Type 16 Function Generator setup/update
  - `solenoid.h` — Type 17 Solenoid setup/update
  - `smoke_shooter.h` — Type 18 Smoke Shooter setup/update
- `include/i2c_devices/` — one file per I2C device type:
  - `i2c_dac.h` — MCP4725, DAC7571, DAC7573 inline write functions
  - `i2c_gpio_expander.h` — DigitalExpanderManager (MCP23017, TCA9555, PCF857x)
  - `pca9685.h` — PCA9685Driver/PCA9685Manager (16-ch PWM)
  - `display_driver.h` — DisplayDriver (SSD1306, SH1106, PCF8574 LCD)
  - (old `i2c_gpio_expander.h` and `pca9685_control.h` in `include/` root removed)
- `include/lighting_protocols/` — protocol receiver headers:
  - `artnet_control.h` — Art-Net UDP listener
  - `sacn_control.h` — sACN E1.31 listener
- `src/main.cpp` — setup, HTTP API, validation, network/display tasks
- `web/index.html` — Web UI source; edit this file, then run `tools/build_web.py`
- `include/web_pages.h` — generated embedded Web UI; do not edit directly
- `include/type_protocol.h` — shared types for the firmware↔Web UI interface contract (`FieldDef`, `TestCmdDef`)
- `include/type_interfaces/type_N.h` — per-output-type interface definitions (config fields, test commands, field validation limits)
- `include/gpio_control.h` — GPIO pin availability, reserved pins, validation helpers (`isInputOnlyPin`, `isReservedEthernetPin`)
- `include/source_rules.h` — I2C source address validation rules (`SourceRules::addressValid()`, etc.)
- `include/display_protocol.h` — display type IDs and I2C address validation
- `include/scoring_limits.h` — hardware limits (`MAX_LEDC`, `MAX_RMT`), CPU/RAM budget constants
- `include/network_protocol.h` — network/system config field keys, limits, defaults, validation helpers; single source of truth for `/api/settings` JSON contract
- `docs/domain_model.md` — project/domain context, device modes, thread safety, ADRs
- `web/js/network_protocol.js` — JS mirror of `network_protocol.h`; single source of truth for network config field keys, limits, and validation
- `docs/resource_calculator.md` — hardware scoring and constraints
- `docs/hardware_guidelines.md` — ESD protection, flyback diode, datasheet checklist
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
- Source IDs: `0=GPIO`, `1=PCA9685`, `2=MCP23017`, `3=TCA9555`, `4=PCF857x`, `5=MCP4725 DAC`, `6=DAC7571`, `7=DAC7573`
- Source/routing/scoring source of truth: `docs/domain_model.md` -> `Configuration Contract`
- Validation/interlock source of truth: C++ in `src/main.cpp` and matching JS in `web/index.html`

## Output Types

0=AC Dimmer, 1=DMX, 2=Relay, 3=RGB LED, 4=Single LED, 5=Analog RGB,
6=Motor, 7=Stepper, 8=Servo, 9=Buzzer, 10=DFPlayer,
11=7-Seg TM1637, 12=7-Seg DD 7-Pin PWM, 13=7-Seg DD 8-Pin PWM,
14=DAC, 15=PWM DAC, 16=Func Gen, 17=Solenoid, 18=Smoke Shooter

## Resource Scoring & Hard Hardware Limits

The single source of truth for resource scoring, hard peripheral limits, physical pin allocations, safety guidelines, and validation/interlock rules is:
- **Source of Truth:** [docs/resource_calculator.md](file:///c:/Users/natta/Documents/bar_program/esp32_eth01_artnet_device/docs/resource_calculator.md)
- **Scoring:** 3 independent budgets → **CPU or RAM full = blocked; hardware = source-aware block**
  - **HardwareResource:** counts only (LEDC ≤16, RMT ≤8, UART ≤2, DAC ≤2, timer ≤4) – GPIO/PCA/EXP not counted
  - **CpuBudget:** per-type µs/frame + 500 µs base overhead + per-I2C-write overhead + AC Dimmer/Function Generator background timer cost + ESP-NOW Master; limit = `(1,000,000 / fps) - 1,500` µs
  - **RamBudget:** `224B` channel slot + actual DMX buffer estimate + runtime objects (NeoPixel/DFPlayer/Stepper/FuncGen/RMT fallback/I2C route); limit = 65535 (64 KB)
- **Verification Parity:** Must match between:
  - C++ Firmware: `include/scoring.h` -> `estimateHardware()`, `estimateChannelCost()`, `checkScores()`
  - Web UI: `web/index.html` -> `channelHardware()`, `channelCost()`, `hwBlocked()/cpuBlocked()/ramBlocked()`
- **LED strip runtime:** `OutputControl::updateLeds()` must call NeoPixelBus/RMT `CanShow()` before pixel mapping and `Show()`; if busy, skip that strip for the current tick and send the newest DMX buffer on a later frame.
- **Hard Resource Limits:** Refer to [docs/resource_calculator.md](file:///c:/Users/natta/Documents/bar_program/esp32_eth01_artnet_device/docs/resource_calculator.md#1-peripheral-limits) for LEDC (16 max), RMT (8 max), UART (2 usable), timer (4 max), and shared I2C bus constraints.
- **Interlocks To Preserve:** Validate every config rule in both the C++ API and Web UI. You must prevent duplicate GPIOs, pin overlaps (Status LED, I2C, ZC), PCA9685 frequency conflicts, and incorrect expander usage. See [docs/resource_calculator.md](file:///c:/Users/natta/Documents/bar_program/esp32_eth01_artnet_device/docs/resource_calculator.md#6-hard-validation-rules) for details.

## Documentation as Source of Truth

The documentation files under `docs/` serve as the **canonical design map** of this firmware. They describe the intended concepts, contracts, and invariants — which code should implement. Code may drift, but the docs are the reference to correct it.

When working on this project:
- **Docs first** — Understand the relevant `docs/` section before writing or modifying code. The domain model, scoring rules, validation gates, and hardware constraints are all captured there.
- **Code is more likely wrong** — AI-generated code has a higher error rate than manually curated docs. If code and docs disagree, treat the doc as the assumed truth and fix the code to match, unless the doc is explicitly marked as drift.
- **Fix docs when you fix code** — Any behavioral change must be reflected in the docs. Stale docs cause the next iteration to reintroduce bugs.
- **Keep docs concise and current** — Remove or strike through resolved items, update packet structures, and sync scoring tables after refactoring.

> This principle exists because a well-maintained `docs/` directory is the most reliable long-term asset for AI-assisted development. Without it, each session starts from scratch and error patterns repeat.

## Policy

- **No backward-compat:** This is v1. Don't keep backward-compat wrappers or migration shims. Delete dead files and update all callers directly.

## Coding Rules

- Keep changes minimal and follow neighboring patterns
- Use NASA programming rules (JPL's Power of 10 rules for safety-critical code, such as avoiding dynamic memory allocation after initialization, bounding all loops, checking parameter/return value validity, and keeping functions short)
- No code comments unless they explain non-obvious hardware/runtime behavior
- Use ArduinoJson 7.x patterns already present in the code
- Use `std::vector<OutputChannel>` for channel storage
- Every `Wire` operation must be protected by `xSemaphoreTake(i2cMutex, pdMS_TO_TICKS(100))`
- Use `networkFramePending.exchange(false)` for the atomic pending-frame flag
- If editing Web UI, edit `web/index.html`, regenerate `include/web_pages.h`, then build
- Web UI output menus must load output device metadata, I2C expander options, and I2C address rules from the firmware header/source-of-truth generator path, not hardcoded JS copies. If menu data is missing, add it to the firmware header source of truth (for example `OUTPUT_MODES[]` / related protocol headers) and generate the Web UI data from there. Common Web UI fields that are not output-device menu metadata, such as `start_universe`, `start_address`, display layout, and other shared base form controls, may remain implemented on the Web UI side.

## Typst User Manual

- **Purpose:** The user manual is an **end-user-facing document** explaining how to assemble, wire, configure, and operate the WT32-ETH01 board as a DMX lighting node. It is **not** an internal developer reference.
- **Diagram Quality:** All technical concepts (wiring, pinouts, topology, startup sequence, protection circuits) **must** be illustrated with Typst-native vector diagrams using CeTZ and Fletcher. No ASCII art or bitmap images in the final PDF.
- **Location:** Source files are under `docs/user_manual/`. The entry point is `main.typ`, which imports modular chapters from `docs/user_manual/chapters/`.
- **Diagrams:** Use native Typst/CeTZ/Fletcher libraries for high-quality, scalable vector graphics. Avoid external image binaries. Use verified compatible package versions (e.g., `@preview/cetz:0.5.2` and `@preview/fletcher:0.5.8` for Typst 0.15.0).
- **Hardware Guidelines Diagrams:** Schematics in `docs/hardware_guidelines.md` use Mermaid as an interim rendering aid; the **canonical** versions intended for end-user delivery should be migrated to Typst/CeTZ diagrams in the user manual.
- **Language Roadmap:** Keep the manual in English first. Thai translation context is planned for the future.
- **Compilation:** Compile using the python script or batch file:
  ```powershell
  # Compile via python
  python tools/build_manual.py
  ```

## Git

- Automatically git commit after making edits/changes.
- Before committing, inspect `git status` and `git diff` to ensure you stage only the modified files, then write a concise, descriptive commit message.
