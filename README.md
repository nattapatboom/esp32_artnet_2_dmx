# WT32-ETH01 Art-Net / sACN DMX Lighting Node

Firmware for a WT32-ETH01 ESP32 Ethernet board that receives lighting data over Art-Net, sACN E1.31, or ESP-NOW and drives physical outputs for stage lighting, props, and effects.

The project includes the embedded firmware, a browser-based configuration UI, metadata-driven output definitions, safety validation, and resource scoring for ESP32 peripherals.

## Highlights

- Art-Net input with OpDmx and OpPoll support.
- sACN E1.31 unicast and multicast reception.
- ESP-NOW master/slave wireless DMX bridge mode.
- Ethernet-first WT32-ETH01 networking with Wi-Fi STA/AP fallback.
- Browser Web UI for settings, output routing, tests, and OTA updates.
- LittleFS output configuration and NVS system settings.
- Recovery mode through consecutive resets or BOOT button.
- Firmware-side validation for GPIO conflicts, I2C address rules, peripheral limits, and resource budgets.

## Supported Outputs

| Type | Output |
| ---: | --- |
| 0 | AC dimmer with zero-crossing timing |
| 1 | DMX512 output over UART or RMT fallback |
| 2 | Relay / digital output |
| 3 | RGB/RGBW LED strip |
| 4 | Single-color PWM LED |
| 5 | Analog RGB/RGBW LED |
| 6 | DC motor driver |
| 7 | Stepper motor |
| 8 | RC servo |
| 9 | Passive buzzer |
| 10 | DFPlayer Mini audio trigger |
| 11 | TM1637 7-segment display |
| 12 | Direct-drive 7-pin 7-segment display |
| 13 | Direct-drive 8-pin 7-segment display |
| 14 | I2C DAC output |
| 15 | PWM DAC output |
| 16 | Function generator |
| 17 | Solenoid output |
| 18 | Sequential smoke shooter |

## Routing Sources

The Web UI is generated from firmware metadata, so routing options and validation stay aligned with C++ source-of-truth headers.

| Source ID | Device / family | Address range |
| ---: | --- | --- |
| 0 | ESP32 GPIO | Direct pin |
| 1 | PCA9685 PWM expander | `0x40..0x47` |
| 2 | MCP23017 digital expander | `0x20..0x27` |
| 3 | TCA9555 digital expander | `0x20..0x27` |
| 4 | PCF857x digital expander | `0x20..0x27`, `0x38..0x3F` |
| 5 | MCP4725 I2C DAC | `0x60..0x61` |
| 6 | DAC7571 I2C DAC | `0x4C..0x4D` |
| 7 | DAC7573 I2C DAC | `0x4C..0x5B` |
| 8 | PCA9635 PWM expander | `0x10..0x7F` |
| 9 | SN3218 PWM expander | `0x54` |
| 10 | AW9523 PWM/GPIO expander | `0x58..0x5B` |

## Hardware Notes

- Target board: WT32-ETH01 with ESP32 and LAN8720 Ethernet PHY.
- Output-capable GPIO list in firmware: `4`, `5`, `12`, `14`, `15`, `2`, `17`, `32`, `33`.
- Ethernet/PHY reserved pins: `0`, `16`, `18`, `19`, `21`, `22`, `23`, `25`, `26`, `27`.
- GPIO `34`, `35`, `36`, and `39` are input-only.
- GPIO `12` is allowed but warning-only because it is a boot strapping pin; avoid pulling it high during reset.
- Inductive loads such as relays, solenoids, motors, and smoke valves need external driver hardware and flyback protection.

Read `docs/hardware_guidelines.md` before connecting real loads.

## Build

Install PlatformIO, then build the default environment:

```powershell
pio run
```

On this development machine, the known-good PlatformIO path is:

```powershell
& "C:\Users\natta\.platformio\penv\Scripts\platformio.exe" run
```

## Flashing

Serial flashing:

```powershell
.\flash_firmware_serial.bat
```

OTA flashing after the board is reachable on the network:

```powershell
.\flash_firmware_ota_by_ip.bat
```

The OTA helper reads the target IP from `test_device_ip.txt`.

## Web UI Development

Edit the source UI in `web/index.html`. Do not edit `include/web_pages.h` directly; regenerate it after UI changes:

```powershell
& "C:\Users\natta\.platformio\penv\Scripts\python.exe" tools/build_web.py
```

Run the Web UI smoke test when changing UI behavior:

```powershell
node tools/web_ui_smoke_test.mjs
```

## Project Structure

| Path | Purpose |
| --- | --- |
| `src/main.cpp` | Setup, tasks, HTTP API, validation, network orchestration |
| `include/output_control.h` | Output channel model and output processing coordinator |
| `include/output_devices/` | Runtime drivers for each output type |
| `include/i2c_devices/` | I2C DAC, PWM expander, GPIO expander, and display drivers |
| `include/lighting_protocols/` | Art-Net and sACN receiver logic |
| `include/output_defs.h` | Output mode metadata and source/pin capabilities |
| `include/type_interfaces/` | Per-output Web UI field and test command definitions |
| `include/source_rules.h` | I2C source IDs and address validation metadata |
| `include/gpio_control.h` | WT32-ETH01 GPIO availability and reserved pin metadata |
| `web/` | Web UI source files |
| `tools/` | Web build, mock, smoke-test, and calculator scripts |
| `docs/` | Domain model, resource calculator, hardware guidelines, user manual source |

## Runtime Architecture

The firmware uses the ESP32 dual-core layout intentionally:

| Core | Workload |
| --- | --- |
| Core 0 | Ethernet, Wi-Fi, UDP protocol listeners, Web UI, display task |
| Core 1 | Real-time output loop for DMX, LEDs, PWM, motors, steppers, and effects |

Configuration saves are validated by the firmware API. The Web UI is treated as a thin client: it renders metadata, submits JSON, and displays firmware responses.

## Resource Budgets

The firmware checks three independent budgets before accepting output layouts:

| Budget | Limit / rule |
| --- | --- |
| Hardware | LEDC <= 16, RMT <= 8, UART <= 2, DAC <= 2, timer <= 4 |
| CPU | Per-frame service time must fit `(1,000,000 / fps) - 1,500 us` |
| RAM | Runtime output allocation estimate must fit the configured RAM pool |

See `docs/resource_calculator.md` for scoring details and `docs/domain_model.md` for the configuration contract.

## Documentation

- `docs/domain_model.md`: architecture, bounded contexts, invariants, ADRs, and configuration contract.
- `docs/resource_calculator.md`: hardware, CPU, and RAM scoring model.
- `docs/hardware_guidelines.md`: wiring safety, ESD, flyback diode, and verification guidance.
- `docs/user_manual/`: end-user manual source in Typst.

## Safety

This firmware can control mains dimmers, motors, solenoids, and smoke hardware. Use isolated drivers, proper fusing, flyback suppression, and enclosure-level protection. Do not connect inductive loads or mains circuits directly to ESP32 GPIO.
