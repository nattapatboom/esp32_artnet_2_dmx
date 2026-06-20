# Project Handover v1.11.0 (Planning: PCA9685 Expansion)

## Current Status
- v1.10.0 remains the latest implemented and built firmware.
- Latest implemented binary from v1.10.0: `dist\firmware_20260617_020449.bin`.
- This v1.11.0 handover is a planning document only. No PCA9685 implementation has been added yet.
- Browser check target provided by user: `http://192.168.1.93/`.
- Codex in-app browser could not be started in this Windows sandbox session (`CreateProcessAsUserW failed: 5`), so the live web UI was not inspected from here.

## Goal
Add an expander menu so external I/O chips can behave like additional GPIO/PWM resources. Output devices should keep the same user-facing modes where possible, then choose a source such as `ESP32 GPIO` or an expander channel.

## Recommendation
Start with PCA9685 support, but present it as `GPIO/PWM Expander` in the UI rather than as many unrelated output device types. The output mode remains familiar: Relay/On-Off, PWM Dimmer, DC Motor, or RC Servo. The source changes from an ESP32 GPIO pin to an expander channel.

Recommended first modes:
- PCA9685 On/Off
- PCA9685 PWM Dimmer
- PCA9685 RC Servo

Recommended second-pass mode:
- PCA9685 DC Motor control, only through an external motor driver.

Do not drive loads directly from PCA9685 outputs. Treat PCA9685 pins as logic/PWM signals only. Use MOSFETs, relay driver boards, optocouplers, servo signal inputs, or motor drivers as appropriate.

## Popular Expander IC Candidates

### Primary Candidate: PCA9685
Source references:
- NXP/Adafruit PCA9685 datasheet: `https://cdn-shop.adafruit.com/datasheets/PCA9685.pdf`
- Adafruit PCA9685 guide/product notes: `https://learn.adafruit.com/16-channel-pwm-servo-driver?view=all`

Why it fits this project:
- 16 channels per chip.
- 12-bit PWM.
- I2C interface.
- Very common ready-made breakout boards.
- Good for servo signal, PWM dimmer signal, on/off via full-on/full-off, and motor driver control signals.

Limitations:
- All 16 channels share one PWM frequency.
- Output current is signal-level only; use external drivers for real loads.
- Not suitable for WS281x LED strips, DMX output, stepper pulse timing, or zero-cross TRIAC timing.

Recommendation:
- Implement first.
- UI name: `PCA9685 PWM/Servo Expander`.

### General GPIO: MCP23017 / MCP23S17
Source reference:
- Microchip MCP23017 page: `https://www.microchip.com/en-us/product/MCP23017`

Why it fits:
- 16-bit general-purpose I/O expander.
- MCP23017 uses I2C; MCP23S17 is the SPI version.
- Good for relay logic, buttons/inputs, limit sensors, basic on/off outputs, and board status/control signals.

Limitations:
- No hardware PWM.
- Not good for servo/PWM dimming unless software PWM is added, which is not recommended for many channels.

Recommendation:
- Add later as `GPIO Expander`.
- Best mode support: On/Off, input/sensor, relay driver input.

### General GPIO: TCA9555 / PCA9555 Family
Source reference:
- TI TCA9555 page: `https://www.ti.com/product/TCA9555`

Why it fits:
- 16 I/Os over I2C/SMBus.
- Wide 1.65-5.5 V supply range.
- Common industrial-style GPIO expander family.

Limitations:
- No hardware PWM.
- Mainly useful for digital inputs/outputs.

Recommendation:
- Good alternative to MCP23017 when board availability is better.
- Support can share most code concepts with MCP23017 if the expander abstraction is clean.

### Low-Cost GPIO: PCF8574 / PCF8574A
Source reference:
- TI PCF8574 page: `https://www.ti.com/product/PCF8574`

Why it fits:
- 8 I/Os over I2C.
- Very common cheap relay/LCD backpack modules.
- Useful for simple on/off outputs or inputs.

Limitations:
- Quasi-bidirectional I/O behavior is less clean than MCP23017/TCA9555.
- Lower I2C speed class on many variants.
- No PWM.

Recommendation:
- Optional low-cost support only after MCP23017/TCA9555 or if the target hardware already uses PCF8574 relay modules.

### Power Output / Relay-Solenoid Sink: TPIC6B595
Source reference:
- TI TPIC6B595 page: `https://www.ti.com/product/TPIC6B595`

Why it fits:
- 8-channel power shift register with open-drain DMOS outputs.
- Better for relays, solenoids, and higher-voltage loads than ordinary logic GPIO expanders.
- Cascadable over shift-register style serial pins.

Limitations:
- Not I2C.
- Low-side sink only.
- On/off output, not general PWM/servo.

Recommendation:
- Consider later for robust relay/solenoid banks if the project needs many switched outputs.

### LED Constant-Current PWM: TLC5947
Source reference:
- TI TLC5947 page: `https://www.ti.com/product/TLC5947`

Why it fits:
- 24-channel constant-current sink LED driver.
- 12-bit PWM grayscale control.
- Good for many small analog LED channels where constant-current drive is useful.

Limitations:
- Not a GPIO expander.
- LED/current-sink specific.
- Not for servo, motor direction pins, or inputs.

Recommendation:
- Optional future `LED PWM Driver` backend, separate from GPIO Expander.

## Expander Priority
1. PCA9685: first, because it covers PWM, servo, on/off, and motor-driver signal use.
2. MCP23017 or TCA9555: second, for true digital GPIO expansion.
3. PCF8574: optional low-cost compatibility.
4. TPIC6B595: optional relay/solenoid power-sink bank.
5. TLC5947: optional analog LED/current-sink PWM bank.

## Important Hardware Constraint
One PCA9685 chip has one shared PWM frequency for all 16 channels.

Implications:
- RC servo needs about 50 Hz.
- LED/PWM dimming is usually better around 200-1000 Hz or higher.
- DC motor PWM usually wants a higher frequency than servo.

Recommendation:
- Use one PCA9685 address/chip for Servo Bank at 50 Hz.
- Use another PCA9685 address/chip for PWM/On-Off/Motor Bank at a configured PWM frequency.
- The UI should warn if mixed modes on the same chip require conflicting frequencies.

## Proposed Output Types

### 11.1 PCA9685 PWM Bank
Purpose:
- LED analog dimming
- MOSFET dimming
- simple low-current PWM control signal

Config fields:
- I2C address: default `0x40`
- Channel: `0-15`
- Start Universe
- Start Address
- Resolution: 8-bit or 12-bit
- Frequency: default `1000 Hz`
- Invert output: optional
- Output enable behavior on boot/test stop: Off

DMX mapping:
- 8-bit: channel N = level `0-255`
- 12-bit: channel N/N+1 = MSB-first value scaled to `0-4095`

Test tool:
- Slider/number `0-255`
- Buttons: Off, 25%, 50%, 100%

### 11.2 PCA9685 On/Off
Purpose:
- relay board input
- optocoupler input
- simple logic output through suitable driver

Config fields:
- I2C address
- Channel: `0-15`
- Start Universe
- Start Address
- Threshold: default `128`
- Active high / active low

DMX mapping:
- channel N `0-127` = Off
- channel N `128-255` = On

Implementation note:
- Use PCA full-off/full-on register states instead of ordinary duty when possible.

Test tool:
- On
- Off

### 11.3 PCA9685 RC Servo
Purpose:
- RC servo signal outputs
- pan/tilt/small mechanical movement

Config fields:
- I2C address
- Channel: `0-15`
- Start Universe
- Start Address
- Resolution: 8-bit or 16-bit input
- Min pulse: default `1000 us`
- Max pulse: default `2000 us`
- Center pulse: default `1500 us`
- Frequency: fixed/recommended `50 Hz`
- Invert direction

DMX mapping:
- 8-bit: channel N maps `0-255` to min-max pulse
- 16-bit: channel N/N+1 maps `0-65535` to min-max pulse

Test tool:
- Min
- Center
- Max
- Position slider

### 11.4 PCA9685 DC Motor (Phase 2)
Purpose:
- Drive external motor driver control inputs, not motor directly.

Recommended sub-modes:
- PWM + DIR: 2 PCA channels, one PWM channel plus one direction logic channel.
- PWM + PWM: 2 PCA channels, one forward PWM and one reverse PWM.
- IN1 + IN2 + EN: 3 PCA channels, two logic channels plus one PWM enable.

DMX mapping:
- Same user-facing behavior as current ESP32 DC Motor mode:
  - midpoint = stop
  - above midpoint = forward
  - below midpoint = reverse
  - deadband around midpoint

Recommendation:
- Implement after PWM/On-Off/Servo are stable.
- Add a clear UI warning: PCA9685 cannot supply motor current and must connect to a motor driver.
- Keep all PCA motor channels on a non-servo PCA chip because motor PWM frequency should not be 50 Hz in most cases.

## Output Model Proposal
Current output channels use fields like:
- `type`
- `pin`
- `pin2`
- `pin3`
- `pin4`

PCA9685 should use separate fields:
- `source`: output source/backend, e.g. `esp32_gpio`, `pca9685`, `mcp23017`, `tca9555`
- `type`: keep the existing user-facing behavior type where possible
- `i2c_addr`
- `pca_channel`
- `pca_channel2`
- `pca_channel3`
- `pca_frequency`

Recommended approach:
- Keep the existing output device modes in the UI.
- Add `Output Source`:
  - `ESP32 GPIO`
  - `PCA9685`
  - future: `MCP23017`, `TCA9555`, `PCF8574`, `TPIC6B595`, `TLC5947`
- When source is `ESP32 GPIO`, show the current GPIO fields.
- When source is an expander, show the expander-specific address/channel fields.
- Internally, use `source` to choose the driver backend and keep `type` as the behavior mode.

Reason:
- This matches the user's mental model: expanders are additional GPIO/PWM resources, not totally separate device categories.
- It avoids a long Type dropdown full of duplicated modes such as `PWM` and `PCA PWM`.
- GPIO conflict logic can remain clean by checking only real ESP32 pins.

## UI Plan
Add a new menu/section:
- `Expanders`
- First supported expander: `PCA9685 PWM/Servo Expander`
- Later supported expanders: MCP23017/TCA9555/PCF8574 GPIO, TPIC6B595 power sink, TLC5947 LED PWM.

In the existing Outputs tab:
- Keep the current Type dropdown modes.
- Add `Output Source` near the GPIO field:
  - `ESP32 GPIO`
  - `PCA9685`
- If source is `PCA9685`, hide ESP32 GPIO pin selectors and show:
  - I2C Address dropdown/input (`0x40-0x7F`, common `0x40-0x47`)
  - PCA Channel dropdown (`0-15`)
  - PCA Channel 2/3 for motor modes
  - Frequency field where relevant
  - Servo pulse fields for servo mode
- If source is a digital GPIO expander later, show expander address/channel instead of GPIO pin.
- Configured Channels table should show:
  - GPIOs column becomes `PCA 0x40:CH0` or `PCA 0x40:CH0/CH1`
  - Future GPIO expanders can display `MCP23017 0x20:A0` or `TCA9555 0x20:P00`
  - Config column shows mode-specific summary, e.g. `Servo 1000-2000us @ 50Hz`

Validation:
- Block duplicate PCA channel use on the same I2C address.
- Block duplicate channels for future GPIO expanders by `(chip_type, i2c_addr, channel)`.
- Warn if one PCA address mixes frequency groups, such as Servo 50Hz and PWM 1000Hz.
- Do not include PCA channels in ESP32 GPIO auto-selection/conflict checks.
- Keep Status LED and Zero-Crossing conflicts only for real ESP32 GPIOs.

## Firmware Plan
Add new module:
- `include/pca9685_control.h`

Preferred implementation:
- Use `Wire` directly with a small driver to reduce dependency risk and flash growth.
- Register writes needed:
  - MODE1 sleep/restart
  - PRESCALE for frequency
  - LEDn_ON_L/H and LEDn_OFF_L/H for duty/full-on/full-off
- Support up to multiple PCA9685 addresses.

Runtime integration:
- Extend `OutputChannel` with PCA fields.
- During `OutputControl::setupChannels()`, initialize PCA addresses used by configured channels.
- During output update loop, render PCA channels from each channel's `dmxBuffer`.
- PCA update rate should be throttled, e.g. 25-50 Hz, to avoid excessive I2C traffic.
- Use dirty-change tracking so unchanged PCA values are not rewritten every frame.

I2C pins:
- Add global I2C settings in Network or Output section:
  - SDA GPIO
  - SCL GPIO
  - I2C frequency, default `400 kHz`
- Must participate in ESP32 GPIO reservation:
  - output GPIO cannot overlap SDA/SCL
  - Status LED cannot overlap SDA/SCL
  - Zero-Crossing cannot overlap SDA/SCL
- Recommended default pins must be checked against WT32-ETH01 wiring before implementation.

## API Plan
Update `/api/outputs` validation:
- Validate PCA address range.
- Validate PCA channel range `0-15`.
- Validate duplicate PCA channels by `(i2c_addr, pca_channel)`.
- Validate per-address frequency conflicts.
- Preserve existing ESP32 GPIO duplicate/reserved pin checks.

Update `/api/output-test`:
- Reuse existing test buffer path where possible.
- Add type-specific test controls in web UI for PCA PWM/On-Off/Servo/Motor.

## Capacity Notes
PCA9685 helps most with:
- Many relay/control signals
- Many analog PWM dimmers
- Many RC servo signal outputs

PCA9685 does not replace:
- WS281x LED strip data output
- DMX serial output
- Zero-cross TRIAC timing
- Stepper pulse generation at high precision

Other expanders:
- MCP23017/TCA9555/PCF8574 help with many on/off relay channels or input sensors, but not PWM/servo.
- TPIC6B595 is better when the output itself must sink relay/solenoid current, but it is not I2C and needs separate shift-register pins.
- TLC5947 is better for many small constant-current LED channels than generic GPIO/PWM expanders.

## Recommended Implementation Order
1. Add an `Expanders` menu with I2C settings and one PCA9685 device entry.
2. Add `Output Source` to existing output modes.
3. Enable PCA9685 source for Relay/On-Off and PWM Dimmer first.
4. Add duplicate `(address, channel)` validation in UI/API.
5. Add output table rendering and test tools for PCA-backed modes.
6. Add RC Servo source with 50 Hz frequency handling.
7. Add frequency conflict warnings per PCA address.
8. Add DC Motor via PCA9685 only after bench testing PWM and servo.
9. Add MCP23017/TCA9555 digital GPIO expander support later if needed.
10. Update user manual with wiring diagrams/notes.
11. Build and compare flash/RAM size.
12. Bench test one PCA board at `0x40` before supporting multiple boards.

## Open Decisions Before Coding
- Which WT32-ETH01 pins should be default SDA/SCL?
- Should I2C settings live in Network tab or Output tab?
- Should one output channel represent one PCA channel, or should there be a "PCA bank" bulk editor for many channels?
- Should PCA Servo and PCA PWM be blocked from sharing one address, or only warned?
- Should PCA DC Motor support mixed ESP32 GPIO direction + PCA PWM, or PCA-only channels at first?
- Which digital GPIO expander should be supported first after PCA9685: MCP23017 or TCA9555?
- Should PCF8574 be supported as a low-cost option despite its quasi-bidirectional behavior?
- Should TPIC6B595 be treated as an expander source, or as a separate relay/solenoid bank driver?

## Suggested Answer to "What Else Should Be Added?"
Worth adding:
- PCA9685 On/Off
- PCA9685 PWM
- PCA9685 RC Servo
- PCA9685 DC Motor through external driver
- MCP23017 or TCA9555 digital GPIO expander for relay/input channels
- PCF8574 only as a cheap/simple on-off expander option
- TPIC6B595 for relay/solenoid sink banks
- TLC5947 for many constant-current analog LED channels
- Optional: PCA9685 grouped/bank editor later for fast setup of 8 or 16 similar channels

Not recommended for PCA9685:
- Stepper motor pulse output
- WS281x LED strips
- Direct solenoid/motor drive without driver hardware
- AC dimmer zero-cross timing

## Verification Checklist for Future Implementation
- [ ] Build passes.
- [ ] Existing GPIO output types still work.
- [ ] Existing GPIO auto-selection ignores PCA channels.
- [ ] Duplicate PCA channels are rejected.
- [ ] PCA PWM writes stable duty values.
- [ ] PCA On/Off uses true full-on/full-off states.
- [ ] PCA Servo produces correct 1000/1500/2000 us pulses at 50 Hz.
- [ ] PCA frequency conflict warning appears when needed.
- [ ] Output test tool works for PCA modes.
- [ ] User manual updated.

---

## AI Summary Reference

The original v1.11 planning summary is kept in handover_v1.11.0_summary.md. A copy is included below so the main handover still carries the broader AI summary context.

# Project Handover v1.11.0 (Summary of Feature Planning & Architecture)

à¹€à¸­à¸à¸ªà¸²à¸£à¸‰à¸šà¸±à¸šà¸™à¸µà¹‰à¹€à¸›à¹‡à¸™à¸‚à¹‰à¸­à¸¡à¸¹à¸¥à¸ªà¸£à¸¸à¸›à¹à¸œà¸™à¸‡à¸²à¸™à¸­à¸­à¸à¹à¸šà¸šà¹à¸¥à¸°à¸à¸²à¸£à¹€à¸•à¸£à¸µà¸¢à¸¡à¸žà¸±à¸’à¸™à¸²à¸Ÿà¸µà¹€à¸ˆà¸­à¸£à¹Œà¸ªà¸³à¸«à¸£à¸±à¸šà¹€à¸Ÿà¸´à¸£à¹Œà¸¡à¹à¸§à¸£à¹Œà¹€à¸§à¸­à¸£à¹Œà¸Šà¸±à¸™ **v1.11.0** à¹‚à¸”à¸¢à¸ªà¸£à¹‰à¸²à¸‡à¸‚à¸¶à¹‰à¸™à¹ƒà¸«à¸¡à¹ˆà¹€à¸žà¸·à¹ˆà¸­à¸ªà¸£à¸¸à¸›à¸›à¸£à¸°à¹€à¸”à¹‡à¸™à¸à¸²à¸£à¸›à¸£à¸°à¹€à¸¡à¸´à¸™à¸—à¸£à¸±à¸žà¸¢à¸²à¸à¸£ à¸‚à¸™à¸²à¸”à¸«à¸™à¹ˆà¸§à¸¢à¸„à¸§à¸²à¸¡à¸ˆà¸³ à¹à¸¥à¸°à¹‚à¸„à¸£à¸‡à¸ªà¸£à¹‰à¸²à¸‡à¸ªà¸–à¸²à¸›à¸±à¸•à¸¢à¸à¸£à¸£à¸¡à¸£à¸°à¸šà¸š à¹‚à¸”à¸¢à¸¡à¸µà¸à¸²à¸£à¸­à¹‰à¸²à¸‡à¸­à¸´à¸‡à¹€à¸›à¸£à¸µà¸¢à¸šà¹€à¸—à¸µà¸¢à¸šà¸à¸±à¸šà¸šà¸´à¸¥à¸”à¹Œà¸—à¸µà¹ˆà¹ƒà¸Šà¹‰à¸‡à¸²à¸™à¸­à¸¢à¸¹à¹ˆà¸›à¸±à¸ˆà¸ˆà¸¸à¸šà¸±à¸™à¹ƒà¸™à¹€à¸§à¸­à¸£à¹Œà¸Šà¸±à¸™ **v1.10.0** à¸­à¸¢à¹ˆà¸²à¸‡à¸Šà¸±à¸”à¹€à¸ˆà¸™

---

## 1. à¸‚à¹‰à¸­à¸¡à¸¹à¸¥à¸­à¹‰à¸²à¸‡à¸­à¸´à¸‡à¹„à¸Ÿà¸¥à¹Œà¹à¸œà¸™à¸‡à¸²à¸™ (Document References)

*   **à¹€à¸Ÿà¸´à¸£à¹Œà¸¡à¹à¸§à¸£à¹Œà¹€à¸§à¸­à¸£à¹Œà¸Šà¸±à¸™à¹ƒà¸Šà¹‰à¸‡à¸²à¸™à¸ˆà¸£à¸´à¸‡à¸›à¸±à¸ˆà¸ˆà¸¸à¸šà¸±à¸™:** [handover_v1.10.0.md](file:///c:/Users/natta/Documents/bar_program/esp32_eth01_artnet_device/handover/handover_v1.10.0.md) (à¸¥à¹ˆà¸²à¸ªà¸¸à¸” à¸“ à¸§à¸±à¸™à¸—à¸µà¹ˆ 17 à¸¡à¸´à¸–à¸¸à¸™à¸²à¸¢à¸™ 2026 à¸šà¸´à¸¥à¸”à¹Œ `firmware_20260617_020449.bin`)
*   **à¹„à¸Ÿà¸¥à¹Œà¹à¸šà¸šà¸£à¹ˆà¸²à¸‡à¹à¸œà¸™à¸‡à¸²à¸™à¸”à¸´à¸š v1.11.0:** [handover_v1.11.0.md](file:///c:/Users/natta/Documents/bar_program/esp32_eth01_artnet_device/handover/handover_v1.11.0.md) (à¸›à¸£à¸°à¸à¸­à¸šà¸”à¹‰à¸§à¸¢à¸„à¸³à¸–à¸²à¸¡à¹€à¸Šà¸´à¸‡à¸­à¸­à¸à¹à¸šà¸š à¸šà¸±à¸™à¸—à¸¶à¸à¸à¸²à¸£à¸•à¸±à¸”à¸ªà¸´à¸™à¹ƒà¸ˆ à¹à¸¥à¸°à¸‚à¸±à¹‰à¸™à¸•à¸­à¸™à¹à¸šà¸šà¸£à¹ˆà¸²à¸‡à¸‚à¹‰à¸­à¹€à¸ªà¸™à¸­à¹à¸™à¸°)
*   **à¹€à¸­à¸à¸ªà¸²à¸£à¸„à¸³à¸™à¸§à¸“à¸—à¸£à¸±à¸žà¸¢à¸²à¸à¸£à¸šà¸­à¸£à¹Œà¸”:** [resource_calculator.md](file:///c:/Users/natta/Documents/bar_program/esp32_eth01_artnet_device/docs/resource_calculator.md)
*   **à¸ªà¸„à¸£à¸´à¸›à¸•à¹Œà¸„à¸³à¸™à¸§à¸“à¸­à¸±à¸•à¹‚à¸™à¸¡à¸±à¸•à¸´ (Python CLI):** [load_calculator.py](file:///c:/Users/natta/Documents/bar_program/esp32_eth01_artnet_device/tools/load_calculator.py)

---

## 2. à¸•à¸²à¸£à¸²à¸‡à¹€à¸›à¸£à¸µà¸¢à¸šà¹€à¸—à¸µà¸¢à¸šà¸à¸²à¸£à¹€à¸›à¸¥à¸µà¹ˆà¸¢à¸™à¹à¸›à¸¥à¸‡ (Comparison Matrix: v1.10.0 vs v1.11.0)

| à¸Ÿà¸µà¹€à¸ˆà¸­à¸£à¹Œ / à¸«à¸±à¸§à¸‚à¹‰à¸­ | à¸ªà¸–à¸²à¸™à¸°à¹ƒà¸™ v1.10.0 (à¸•à¸±à¸§à¹€à¸à¹ˆà¸²) | à¹à¸œà¸™à¸žà¸±à¸’à¸™à¸²à¹ƒà¸™ v1.11.0 (à¸•à¸±à¸§à¹ƒà¸«à¸¡à¹ˆ) | à¸­à¹‰à¸²à¸‡à¸­à¸´à¸‡à¸«à¸±à¸§à¸‚à¹‰à¸­à¹ƒà¸™à¹„à¸Ÿà¸¥à¹Œà¹à¸œà¸™à¸‡à¸²à¸™à¸”à¸´à¸š | à¸ˆà¸¸à¸”à¸—à¸µà¹ˆà¸•à¹‰à¸­à¸‡à¹à¸à¹‰à¹„à¸‚à¹ƒà¸™à¹‚à¸„à¹‰à¸”à¸«à¸¥à¸±à¸ |
| :--- | :--- | :--- | :--- | :--- |
| **WiZ Smart Bulbs** | ðŸŸ¢ à¹€à¸›à¸´à¸”à¹ƒà¸Šà¹‰à¸‡à¸²à¸™à¸œà¹ˆà¸²à¸™ UDP mapped DMX | âŒ **à¸¥à¸šà¸­à¸­à¸à¸–à¸²à¸§à¸£** (à¸–à¸­à¸™à¹‚à¸„à¹‰à¸”à¹à¸¥à¸°à¸¢à¹‰à¸²à¸¢à¹„à¸›à¸„à¸¸à¸¡à¸œà¹ˆà¸²à¸™ PC à¹€à¸Šà¹ˆà¸™ Node-RED/QLC+) à¹€à¸žà¸·à¹ˆà¸­à¸›à¸£à¸°à¸«à¸¢à¸±à¸”à¸‚à¸™à¸²à¸”à¹à¸Ÿà¸¥à¸Šà¹à¸¥à¸° Heap RAM | [handover_v1.11.0.md: L704-L715](file:///c:/Users/natta/Documents/bar_program/esp32_eth01_artnet_device/handover/handover_v1.11.0.md#L704-L715) | à¸¥à¸šà¸Ÿà¸´à¸¥à¸”à¹Œà¹ƒà¸™ [config.h](file:///c:/Users/natta/Documents/bar_program/esp32_eth01_artnet_device/include/config.h) à¹à¸¥à¸° [output_control.h](file:///c:/Users/natta/Documents/bar_program/esp32_eth01_artnet_device/include/output_control.h) |
| **Bluetooth / BLE** | âŒ à¹„à¸¡à¹ˆà¸¡à¸µà¸à¸²à¸£à¹€à¸›à¸´à¸”à¹ƒà¸Šà¹‰à¸‡à¸²à¸™ | âŒ **à¸¡à¸•à¸´à¸•à¸±à¸”à¸­à¸­à¸à¸–à¸²à¸§à¸£** à¸›à¹‰à¸­à¸‡à¸à¸±à¸™à¸à¸²à¸£à¸Šà¸™à¸à¸±à¸šà¸„à¸§à¸²à¸¡à¸›à¸¥à¸­à¸”à¸ à¸±à¸¢à¸‚à¸­à¸‡ OTA à¹à¸¥à¸°à¸›à¹‰à¸­à¸‡à¸à¸±à¸™à¸à¸£à¸°à¹à¸ªà¹„à¸Ÿà¸à¸£à¸°à¸Šà¸²à¸à¸•à¸­à¸™à¸šà¸¹à¸• | [handover_v1.11.0.md: L717-L728](file:///c:/Users/natta/Documents/bar_program/esp32_eth01_artnet_device/handover/handover_v1.11.0.md#L717-L728) | à¹„à¸¡à¹ˆà¸¡à¸µà¸à¸²à¸£à¸™à¸³à¸¡à¸²à¹ƒà¸Šà¹‰à¹ƒà¸™à¸šà¸´à¸¥à¸”à¹Œ |
| **à¹‚à¸„à¸£à¸‡à¸ªà¸£à¹‰à¸²à¸‡à¸ªà¸–à¸²à¸›à¸±à¸•à¸¢à¸à¸£à¸£à¸¡ Pin** | âŒ à¸•à¹ˆà¸­à¸•à¸£à¸‡à¸œà¹ˆà¸²à¸™ ESP32 GPIO à¹€à¸—à¹ˆà¸²à¸™à¸±à¹‰à¸™ | ðŸŸ¢ **Unified Expander Layer** à¹€à¸žà¸´à¹ˆà¸¡à¸Ÿà¸´à¸¥à¸”à¹Œ `source` à¹€à¸žà¸·à¹ˆà¸­à¸£à¸­à¸‡à¸£à¸±à¸š Virtual Pin à¸—à¸±à¹‰à¸‡à¸šà¸™ ESP32 à¹à¸¥à¸° I2C Expander à¹€à¸Šà¹ˆà¸™ PCA9685/MCP23017 | [handover_v1.11.0.md: L253-L284](file:///c:/Users/natta/Documents/bar_program/esp32_eth01_artnet_device/handover/handover_v1.11.0.md#L253-L284) à¹à¸¥à¸° [L561-L571](file:///c:/Users/natta/Documents/bar_program/esp32_eth01_artnet_device/handover/handover_v1.11.0.md#L561-L571) | à¹à¸à¹‰à¹„à¸‚ `struct OutputChannel` à¹ƒà¸™ [output_control.h](file:///c:/Users/natta/Documents/bar_program/esp32_eth01_artnet_device/include/output_control.h) |
| **à¸à¸²à¸£à¹ƒà¸Šà¹‰à¹‚à¸«à¸¡à¸”à¸žà¸´à¸™à¹à¸šà¸šà¸œà¸ªà¸¡ (Mixed-Mode)** | âŒ à¹„à¸¡à¹ˆà¸£à¸­à¸‡à¸£à¸±à¸šà¸žà¸´à¸™à¸‚à¹‰à¸²à¸¡à¸­à¸¸à¸›à¸à¸£à¸“à¹Œ | ðŸŸ¢ **à¸ªà¸¥à¸±à¸šà¸žà¸´à¸™à¸­à¸´à¸ªà¸£à¸°** à¹€à¸Šà¹ˆà¸™ Stepper à¹ƒà¸Šà¹‰à¸žà¸±à¸¥à¸ªà¹Œà¸„à¸§à¸²à¸¡à¹€à¸£à¹‡à¸§à¸ªà¸¹à¸‡ (STEP) à¸šà¸™ ESP32 à¹à¸¥à¸°à¸¢à¹‰à¸²à¸¢à¸‚à¸²à¸Šà¹‰à¸² (DIR/ENABLE) à¹„à¸›à¸¥à¸‡ I2C Expander | [handover_v1.11.0.md: L550-L559](file:///c:/Users/natta/Documents/bar_program/esp32_eth01_artnet_device/handover/handover_v1.11.0.md#L550-L559) | à¸›à¸£à¸±à¸šà¸›à¸£à¸¸à¸‡à¸£à¸°à¸šà¸šà¸•à¸£à¸§à¸ˆà¸ªà¸­à¸šà¹à¸¥à¸°à¹à¸¢à¸ Driver à¹„à¸”à¸™à¸²à¸¡à¸´à¸ |
| **à¹‚à¸«à¸¡à¸”à¹€à¸„à¸£à¸·à¹ˆà¸­à¸‡à¸—à¸³à¸„à¸§à¸±à¸™à¸ªà¹€à¸•à¸ˆ** | âŒ à¸•à¹‰à¸­à¸‡à¹ƒà¸Šà¹‰ Relay à¹à¸¢à¸ 2 à¸Šà¹ˆà¸­à¸‡à¸„à¸¸à¸¡à¸­à¸´à¸ªà¸£à¸°à¹€à¸­à¸‡ | ðŸŸ¢ **à¹‚à¸«à¸¡à¸” Sequential Smoke Shooter** à¹ƒà¸Šà¹‰ DMX 1 à¸Šà¹ˆà¸­à¸‡à¸—à¸£à¸´à¸à¹€à¸à¸­à¸£à¹Œà¸¥à¸³à¸”à¸±à¸šà¹€à¸§à¸¥à¸²à¹€à¸›à¸´à¸”à¸„à¸§à¸±à¸™à¸£à¸­à¸ªà¸°à¸ªà¸¡à¸•à¸±à¸§à¹à¸¥à¹‰à¸§à¸ˆà¸¶à¸‡à¸ªà¸±à¹ˆà¸‡à¸¢à¸´à¸‡à¸¥à¸¡à¹à¸šà¸šà¸­à¸±à¸•à¹‚à¸™à¸¡à¸±à¸•à¸´ | [handover_v1.11.0.md: L440-L469](file:///c:/Users/natta/Documents/bar_program/esp32_eth01_artnet_device/handover/handover_v1.11.0.md#L440-L469) | à¹€à¸žà¸´à¹ˆà¸¡ State Machine à¹ƒà¸™ [output_control.h](file:///c:/Users/natta/Documents/bar_program/esp32_eth01_artnet_device/include/output_control.h) |
| **~~à¸¡à¸­à¹€à¸•à¸­à¸£à¹Œà¸”à¸µà¸‹à¸µ Closed-Loop~~** | ~~âŒ à¸¡à¸µà¹€à¸‰à¸žà¸²à¸°à¹‚à¸«à¸¡à¸” Open-Loop à¸„à¸¸à¸¡à¹à¸£à¸‡à¸”à¸±à¸™à¸œà¹ˆà¸²à¸™ PWM~~ | âŒ **à¸¢à¸à¹€à¸¥à¸´à¸à¹à¸œà¸™à¸žà¸±à¸’à¸™à¸²** (à¹€à¸™à¸·à¹ˆà¸­à¸‡à¸ˆà¸²à¸à¸„à¸§à¸²à¸¡à¸‹à¸±à¸šà¸‹à¹‰à¸­à¸™à¹ƒà¸™à¸à¸²à¸£à¸ˆà¸¹à¸™ PID à¹à¸¥à¸°à¸à¸²à¸£à¹ƒà¸Šà¹‰à¸‹à¸­à¸Ÿà¸•à¹Œà¹à¸§à¸£à¹Œà¹€à¸‰à¸žà¸²à¸°à¸•à¸±à¸§ à¸ˆà¸¶à¸‡à¹€à¸¥à¸·à¸­à¸à¹ƒà¸Šà¹‰à¹„à¸”à¸£à¹€à¸§à¸­à¸£à¹Œà¸ à¸²à¸¢à¸™à¸­à¸à¸—à¸µà¹ˆà¸£à¸­à¸‡à¸£à¸±à¸š Step/Dir à¹ƒà¸™à¹‚à¸«à¸¡à¸” Stepper à¹à¸—à¸™) | [handover_v1.11.0.md: Section 9](file:///c:/Users/natta/Documents/bar_program/esp32_eth01_artnet_device/handover/handover_v1.11.0.md#L1037-L1107) | - |
| **à¸«à¸™à¹‰à¸²à¸ˆà¸­ 7-Segment** | âŒ à¹„à¸¡à¹ˆà¸£à¸­à¸‡à¸£à¸±à¸š | ðŸŸ¢ **à¹‚à¸«à¸¡à¸” TM1637 (2 à¸žà¸´à¸™) & à¹‚à¸«à¸¡à¸”à¹„à¸”à¹€à¸£à¸à¸•à¹Œ 7-8 à¸žà¸´à¸™** à¸£à¸­à¸‡à¸£à¸±à¸šà¸—à¸±à¹‰à¸‡à¸Šà¸´à¸›à¸ªà¸³à¹€à¸£à¹‡à¸ˆà¸£à¸¹à¸› à¹à¸¥à¸°à¸à¸²à¸£à¸‚à¸±à¸šà¹€à¸‹à¸à¹€à¸¡à¸™à¸•à¹Œà¸•à¸£à¸‡ (CC/CA) à¸œà¹ˆà¸²à¸™à¸‚à¸²à¸•à¹ˆà¸­à¹€à¸™à¸·à¹ˆà¸­à¸‡ | [handover_v1.11.0.md: L606-L633](file:///c:/Users/natta/Documents/bar_program/esp32_eth01_artnet_device/handover/handover_v1.11.0.md#L606-L633) à¹à¸¥à¸° [L633-L702](file:///c:/Users/natta/Documents/bar_program/esp32_eth01_artnet_device/handover/handover_v1.11.0.md#L633-L702) | à¹€à¸žà¸´à¹ˆà¸¡à¹‚à¸¡à¸”à¸¹à¸¥ 7-Segment Font à¹ƒà¸™à¹‚à¸Ÿà¸¥à¹€à¸”à¸­à¸£à¹Œ `include/` à¹à¸¥à¸°à¹‚à¸„à¹‰à¸”à¸‚à¸±à¸šà¸‚à¸² CC/CA |
| **à¸à¸²à¸£à¸œà¸ªà¸¡à¸ªà¸µà¸«à¸¥à¸­à¸”à¹„à¸Ÿà¹à¸­à¸™à¸°à¸¥à¹‡à¸­à¸** | âŒ à¸ªà¸£à¹‰à¸²à¸‡à¸Šà¹ˆà¸­à¸‡à¸«à¸£à¸µà¹ˆà¹„à¸Ÿà¹à¸¢à¸à¸à¸±à¸™ 3-4 à¹à¸–à¸§ | ðŸŸ¢ **à¹‚à¸«à¸¡à¸” Analog RGB / RGBW** à¸£à¸§à¸¡à¸¨à¸¹à¸™à¸¢à¹Œà¸žà¸´à¸™à¸ªà¸µà¹ƒà¸™à¸ªà¸¥à¹‡à¸­à¸•à¹€à¸”à¸µà¸¢à¸§ à¸œà¸¹à¸ DMX à¸•à¹ˆà¸­à¹€à¸™à¸·à¹ˆà¸­à¸‡à¸­à¸±à¸•à¹‚à¸™à¸¡à¸±à¸•à¸´ à¹à¸¥à¸°à¹à¸ªà¸”à¸‡ Color Picker à¸šà¸™ Web UI | [handover_v1.11.0.md: L616-L631](file:///c:/Users/natta/Documents/bar_program/esp32_eth01_artnet_device/handover/handover_v1.11.0.md#L616-L631) | à¹à¸à¹‰à¹„à¸‚à¹ƒà¸™ [web_pages.h](file:///c:/Users/natta/Documents/bar_program/esp32_eth01_artnet_device/include/web_pages.h) à¹à¸¥à¸° DMX mapping loop |
| **à¹€à¸ªà¸µà¸¢à¸‡à¸ªà¸±à¸à¸à¸²à¸“à¹à¸ˆà¹‰à¸‡à¹€à¸•à¸·à¸­à¸™** | âŒ à¹„à¸¡à¹ˆà¸£à¸­à¸‡à¸£à¸±à¸š | ðŸŸ¢ **Passive Buzzer** à¸„à¸¸à¸¡à¸¢à¹ˆà¸²à¸™à¸„à¸§à¸²à¸¡à¸–à¸µà¹ˆ (100Hz-5kHz) à¸£à¹ˆà¸§à¸¡à¸à¸±à¸šà¸£à¸°à¸”à¸±à¸šà¸„à¸§à¸²à¸¡à¸”à¸±à¸‡ (Volume 0-50% Duty) à¸œà¹ˆà¸²à¸™à¸ªà¸±à¸à¸à¸²à¸“ DMX 2 à¸Šà¹ˆà¸­à¸‡ | [handover_v1.11.0.md: L729-L761](file:///c:/Users/natta/Documents/bar_program/esp32_eth01_artnet_device/handover/handover_v1.11.0.md#L729-L761) | à¹€à¸žà¸´à¹ˆà¸¡à¹‚à¸„à¹‰à¸”à¸„à¸§à¸šà¸„à¸¸à¸¡ LEDC tone generator |
| **à¸£à¸°à¸šà¸šà¸£à¸±à¸šà¸­à¸´à¸™à¸žà¸¸à¸•à¹€à¸‹à¸™à¹€à¸‹à¸­à¸£à¹Œ** | âŒ à¸£à¸­à¸‡à¸£à¸±à¸šà¹€à¸‰à¸žà¸²à¸° Zero-Crossing input | ðŸŸ¢ **Sensor Input System** à¸£à¸­à¸‡à¸£à¸±à¸š ToF (VL53L0X), Ultrasonic (HC-SR04 / à¸à¸±à¸™à¸™à¹‰à¸³ A02YYUW UART), à¹à¸¥à¸° Analog Mic | [handover_v1.11.0.md: Section 8](file:///c:/Users/natta/Documents/bar_program/esp32_eth01_artnet_device/handover/handover_v1.11.0.md#L802-L907) | à¹€à¸žà¸´à¹ˆà¸¡à¸£à¸°à¸šà¸šà¸­à¹ˆà¸²à¸™à¸­à¸´à¸™à¸žà¸¸à¸•à¹à¸¥à¸°à¸ˆà¸±à¸”à¸ªà¸£à¸£à¸žà¸­à¸£à¹Œà¸•à¸‹à¸µà¹€à¸£à¸µà¸¢à¸¥ UART |
| **à¹‚à¸«à¸¥à¸”à¹€à¸™à¹‡à¸•à¹€à¸§à¸´à¸£à¹Œà¸à¹à¸¥à¸°à¸à¸²à¸£à¸ªà¹ˆà¸‡à¸à¸¥à¸±à¸š** | âŒ à¸¡à¸µà¹€à¸‰à¸žà¸²à¸°à¸à¸²à¸£à¸£à¸±à¸šà¹à¸žà¹‡à¸à¹€à¸à¹‡à¸•à¸„à¸­à¸™à¹‚à¸—à¸£à¸¥ | ðŸŸ¢ **Rate Limiting & Unicast Target** à¸ˆà¸³à¸à¸±à¸”à¸­à¸±à¸•à¸£à¸²à¸ªà¸•à¸£à¸µà¸¡à¸ªà¹ˆà¸‡à¸à¸¥à¸±à¸š (30-50Hz), à¸šà¸±à¸‡à¸„à¸±à¸šà¹€à¸ˆà¸²à¸°à¸ˆà¸‡ IP à¸›à¸¥à¸²à¸¢à¸—à¸²à¸‡à¹€à¸žà¸·à¹ˆà¸­à¸¥à¸”à¹‚à¸«à¸¥à¸” WiFi | [handover_v1.11.0.md: Section 8.5](file:///c:/Users/natta/Documents/bar_program/esp32_eth01_artnet_device/handover/handover_v1.11.0.md#L907-L947) | à¹€à¸žà¸´à¹ˆà¸¡ UDP Unicast Sender à¹à¸¥à¸° Task Pinning à¹ƒà¸™ `src/main.cpp` |

---

## 3. à¸£à¸²à¸¢à¸¥à¸°à¹€à¸­à¸µà¸¢à¸”à¹à¸¥à¸°à¸‚à¹‰à¸­à¸¡à¸¹à¸¥à¸„à¸³à¸™à¸§à¸“à¹€à¸‰à¸žà¸²à¸°à¸”à¹‰à¸²à¸™ (Key Planning Insights)

### 3.1 à¸à¸²à¸£à¸„à¸³à¸™à¸§à¸“à¸žà¸·à¹‰à¸™à¸—à¸µà¹ˆà¹à¸Ÿà¸¥à¸Šà¹à¸¥à¸°à¹€à¸ªà¸–à¸µà¸¢à¸£à¸ à¸²à¸žà¸£à¸°à¸šà¸š (Flash Memory Size Calculations)
*   **à¸‚à¸™à¸²à¸”à¹„à¸Ÿà¸¥à¹Œ Bin à¸›à¸±à¸ˆà¸ˆà¸¸à¸šà¸±à¸™ (v1.10.0):** `1.11 MB` (à¸„à¸´à¸”à¹€à¸›à¹‡à¸™ 59.3% à¸‚à¸­à¸‡à¸„à¸§à¸²à¸¡à¸ˆà¸¸à¹à¸­à¸›à¸žà¸¥à¸´à¹€à¸„à¸Šà¸±à¸™ 1.90 MB)
*   **à¸žà¸·à¹‰à¸™à¸—à¸µà¹ˆà¹à¸Ÿà¸¥à¸Šà¸§à¹ˆà¸²à¸‡à¹€à¸«à¸¥à¸·à¸­:** **~780 KB** (à¹€à¸žà¸µà¸¢à¸‡à¸žà¸­à¸ªà¸³à¸«à¸£à¸±à¸šà¸à¸²à¸£à¸ªà¸³à¸£à¸­à¸‡à¸‚à¹‰à¸­à¸¡à¸¹à¸¥à¸‚à¸“à¸°à¸—à¸³ OTA à¸›à¸¥à¸­à¸”à¸ à¸±à¸¢à¸ªà¸¹à¸‡à¸¡à¸²à¸)
*   **à¸‚à¸™à¸²à¸”à¹‚à¸„à¹‰à¸”à¸£à¸§à¸¡à¹„à¸¥à¸šà¸£à¸²à¸£à¸µà¹ƒà¸«à¸¡à¹ˆà¸—à¸µà¹ˆà¸ˆà¸°à¹€à¸žà¸´à¹ˆà¸¡à¹€à¸‚à¹‰à¸²à¹ƒà¸™ v1.11.0:**
    *   *ToF Sensor (VL53L0X):* ~10 KB (à¹ƒà¸Šà¹‰à¹„à¸¥à¸šà¸£à¸²à¸£à¸µà¸‚à¸™à¸²à¸”à¹€à¸šà¸²à¸‚à¸­à¸‡ Pololu)
    *   *Ultrasonic (HC-SR04 / A02YYUW):* <1 KB (à¸„à¸³à¸™à¸§à¸“à¸„à¸¥à¸·à¹ˆà¸™à¸žà¸±à¸¥à¸ªà¹Œà¸•à¸£à¸‡ à¸«à¸£à¸·à¸­à¸”à¸¶à¸‡à¸„à¹ˆà¸²à¸œà¹ˆà¸²à¸™à¸‹à¸µà¹€à¸£à¸µà¸¢à¸¥à¸­à¸´à¸™à¹€à¸—à¸­à¸£à¹Œà¹€à¸Ÿà¸‹ UART1 à¹ƒà¸™à¸•à¸±à¸§)
    *   *Analog Mic (MAX4466):* <1 KB (à¸”à¸¶à¸‡ API à¸•à¸£à¸‡à¸ˆà¸²à¸ ADC)
    *   *Modbus TCP/RTU:* ~15 KB (à¹ƒà¸Šà¹‰à¹„à¸¥à¸šà¸£à¸²à¸£à¸µ `ModbusMaster` à¹à¸¥à¸° Ethernet socket à¹ƒà¸™à¸•à¸±à¸§)
    *   *à¸£à¸°à¸šà¸šà¹€à¸‹à¸™à¹€à¸‹à¸­à¸£à¹Œà¸£à¸§à¸¡:* à¹ƒà¸Šà¹‰à¸žà¸·à¹‰à¸™à¸—à¸µà¹ˆà¹à¸Ÿà¸¥à¸Šà¹€à¸žà¸´à¹ˆà¸¡à¸£à¸§à¸¡à¸à¸±à¸™à¸›à¸£à¸°à¸¡à¸²à¸“ **15 KB - 50 KB** à¹€à¸—à¹ˆà¸²à¸™à¸±à¹‰à¸™ (à¹€à¸«à¸¥à¸·à¸­à¸žà¸·à¹‰à¸™à¸—à¸µà¹ˆà¸§à¹ˆà¸²à¸‡à¸­à¸µà¸à¸à¸§à¹ˆà¸² 730 KB à¸ªà¸³à¸«à¸£à¸±à¸šà¸­à¸™à¸²à¸„à¸•)

### 3.2 à¸ªà¸–à¸²à¸›à¸±à¸•à¸¢à¸à¸£à¸£à¸¡à¸šà¸±à¸ª I2C à¸£à¹ˆà¸§à¸¡à¸à¸±à¸™ (I2C Bus Coexistence Architecture)
à¸žà¸­à¸£à¹Œà¸•à¹€à¸Šà¸·à¹ˆà¸­à¸¡à¸•à¹ˆà¸­à¸šà¸±à¸ª I2C (SDA/SCL) à¸ªà¸²à¸¡à¸²à¸£à¸–à¸žà¹ˆà¸§à¸‡à¸­à¸¸à¸›à¸à¸£à¸“à¹Œà¸£à¹ˆà¸§à¸¡à¸à¸±à¸™à¹„à¸”à¹‰à¸—à¸±à¹‰à¸‡à¸«à¸¡à¸”à¹‚à¸”à¸¢à¹„à¸¡à¹ˆà¸¡à¸µà¸à¸²à¸£à¸£à¸šà¸à¸§à¸™à¸ªà¸±à¸à¸à¸²à¸“ à¹€à¸™à¸·à¹ˆà¸­à¸‡à¸ˆà¸²à¸à¸—à¸¸à¸à¸•à¸±à¸§à¹ƒà¸Šà¹‰ Hardware Address à¸•à¹ˆà¸²à¸‡à¸à¸±à¸™à¸­à¸¢à¹ˆà¸²à¸‡à¸ªà¸´à¹‰à¸™à¹€à¸Šà¸´à¸‡:
*   **à¸šà¸±à¸ª SDA/SCL à¸£à¹ˆà¸§à¸¡à¸à¸±à¸™:** à¸à¸³à¸«à¸™à¸”à¹ƒà¸«à¹‰à¹ƒà¸Šà¹‰à¸žà¸´à¸™à¸›à¸¥à¸­à¸”à¸ à¸±à¸¢ **SDA = GPIO14** à¹à¸¥à¸° **SCL = GPIO15** à¸‚à¸­à¸‡à¸šà¸­à¸£à¹Œà¸” WT32-ETH01
*   **à¸à¸²à¸£à¸à¸£à¸°à¸ˆà¸²à¸¢à¹à¸­à¸”à¹€à¸”à¸£à¸ªà¸šà¸™à¸šà¸±à¸ª:**
    *   `0x20` $\rightarrow$ MCP23017 à¸«à¸£à¸·à¸­ TCA9555 (à¸žà¸­à¸£à¹Œà¸•à¸­à¸´à¸™à¸žà¸¸à¸•/à¹€à¸­à¸²à¸•à¹Œà¸žà¸¸à¸•à¸”à¸´à¸ˆà¸´à¸—à¸±à¸¥à¸£à¸µà¹€à¸¥à¸¢à¹Œ)
    *   `0x29` $\rightarrow$ ToF VL53L0X (à¹€à¸‹à¸™à¹€à¸‹à¸­à¸£à¹Œà¸£à¸°à¸¢à¸°à¸—à¸²à¸‡à¹€à¸¥à¹€à¸‹à¸­à¸£à¹Œ)
    *   `0x40` $\rightarrow$ PCA9685 (à¸žà¸­à¸£à¹Œà¸•à¹€à¸‹à¸­à¸£à¹Œà¹‚à¸§à¹à¸¥à¸°à¹€à¸­à¸²à¸•à¹Œà¸žà¸¸à¸•à¸”à¸µà¸¡à¹€à¸¡à¸­à¸£à¹Œ PWM)
*   *à¸‚à¹‰à¸­à¸„à¸§à¸£à¸£à¸°à¸§à¸±à¸‡:* à¸«à¸²à¸à¹ƒà¸Šà¹‰à¹€à¸‹à¸™à¹€à¸‹à¸­à¸£à¹Œ ToF (VL53L0X) à¸¡à¸²à¸à¸à¸§à¹ˆà¸² 1 à¸•à¸±à¸§ à¹à¸­à¸”à¹€à¸”à¸£à¸ªà¸—à¸µà¹ˆà¹€à¸«à¸¡à¸·à¸­à¸™à¸à¸±à¸™à¸ˆà¸°à¸Šà¸™à¸à¸±à¸™ à¸•à¹‰à¸­à¸‡à¸•à¹ˆà¸­à¸žà¸´à¸™à¸‚à¸¢à¸²à¸¢à¹€à¸‚à¹‰à¸²à¹„à¸›à¸—à¸µà¹ˆà¸‚à¸² `XSHUT` à¸‚à¸­à¸‡à¹à¸•à¹ˆà¸¥à¸°à¹‚à¸¡à¸”à¸¹à¸¥ à¹€à¸žà¸·à¹ˆà¸­à¹ƒà¸«à¹‰à¸‹à¸­à¸Ÿà¸•à¹Œà¹à¸§à¸£à¹Œà¸ªà¸²à¸¡à¸²à¸£à¸–à¸ªà¸±à¹ˆà¸‡à¹€à¸›à¸´à¸”à¸šà¸¹à¸•à¸šà¸­à¸£à¹Œà¸”à¸—à¸µà¸¥à¸°à¸•à¸±à¸§à¹à¸¥à¸°à¸—à¸³à¸à¸²à¸£à¹€à¸›à¸¥à¸µà¹ˆà¸¢à¸™à¹à¸­à¸”à¹€à¸”à¸£à¸ªà¸Šà¸±à¹ˆà¸§à¸„à¸£à¸²à¸§à¹€à¸›à¹‡à¸™à¹à¸­à¸”à¹€à¸”à¸£à¸ªà¸­à¸·à¹ˆà¸™ (à¹€à¸Šà¹ˆà¸™ `0x30`, `0x31`, ...) à¹ƒà¸™à¸Šà¹ˆà¸§à¸‡à¹€à¸£à¸´à¹ˆà¸¡à¸•à¹‰à¸™ setup à¸£à¸°à¸šà¸š à¸«à¸£à¸·à¸­à¹€à¸¥à¸·à¸­à¸à¹ƒà¸Šà¹‰à¸Šà¸´à¸›à¸ªà¸§à¸´à¸•à¸Šà¹Œà¸šà¸±à¸ª I2C (TCA9548A)
*   *à¸­à¸´à¸™à¹€à¸—à¸­à¸£à¹Œà¹€à¸Ÿà¸‹à¹à¸¢à¸à¸ªà¸³à¸«à¸£à¸±à¸š A02YYUW:* à¹€à¸™à¸·à¹ˆà¸­à¸‡à¸ˆà¸²à¸à¸•à¸±à¸§à¹€à¸‹à¸™à¹€à¸‹à¸­à¸£à¹Œ A02YYUW à¹€à¸›à¹‡à¸™à¹à¸šà¸šà¸ªà¸²à¸¢à¸­à¸™à¸¸à¸à¸£à¸¡ (UART) à¸ˆà¸¶à¸‡à¹à¸¢à¸à¸ªà¸²à¸¢à¸ªà¸±à¸à¸à¸²à¸“à¸­à¸­à¸à¸ˆà¸²à¸à¸šà¸±à¸ª I2C à¹„à¸›à¸•à¹ˆà¸­à¹€à¸‚à¹‰à¸²à¸žà¸´à¸™à¸­à¸´à¸™à¸žà¸¸à¸•à¸•à¸£à¸‡ (à¹€à¸Šà¹ˆà¸™ GPIO34 RX1) à¹à¸¥à¸°à¹€à¸Šà¸·à¹ˆà¸­à¸¡à¸•à¹ˆà¸­à¸œà¹ˆà¸²à¸™à¸Šà¹ˆà¸­à¸‡à¸ªà¸±à¸à¸à¸²à¸“à¸‹à¸µà¹€à¸£à¸µà¸¢à¸¥ UART1 à¹ƒà¸™à¸•à¸±à¸§à¸Šà¸´à¸› ESP32

### 3.3 à¸à¸²à¸£à¸„à¸³à¸™à¸§à¸“à¹à¸¥à¸°à¸›à¸£à¸°à¹€à¸¡à¸´à¸™à¹‚à¸«à¸¥à¸”à¸‚à¸­à¸‡à¹€à¸™à¹‡à¸•à¹€à¸§à¸´à¸£à¹Œà¸ (Network Load Optimization Rules)
à¸£à¸°à¸šà¸šà¸ªà¹ˆà¸‡à¸‚à¹‰à¸­à¸¡à¸¹à¸¥à¸•à¸­à¸šà¸ªà¸™à¸­à¸‡à¸£à¸°à¸¢à¸°à¹à¸¥à¸°à¸žà¸·à¹‰à¸™à¸—à¸µà¹ˆ (Sensor Feedback) à¸•à¹‰à¸­à¸‡à¹„à¸¡à¹ˆà¹„à¸›à¹€à¸žà¸´à¹ˆà¸¡à¸—à¸£à¸²à¸Ÿà¸Ÿà¸´à¸à¸ˆà¸™à¹€à¸„à¸£à¸·à¸­à¸‚à¹ˆà¸²à¸¢ WiFi à¸ªà¸°à¸”à¸¸à¸”à¸«à¸£à¸·à¸­à¸—à¸³à¹ƒà¸«à¹‰à¸«à¸™à¹ˆà¸§à¸¢à¸„à¸§à¸²à¸¡à¸ˆà¸³à¸šà¸­à¸£à¹Œà¸”à¸—à¸³à¸‡à¸²à¸™à¸«à¸™à¸±à¸à¹€à¸à¸´à¸™à¹„à¸›:
1.  **Rate Limiting:** à¸ˆà¸³à¸à¸±à¸”à¸„à¸§à¸²à¸¡à¸–à¸µà¹ˆà¸à¸²à¸£à¸ªà¹ˆà¸‡à¸ªà¸•à¸£à¸µà¸¡à¸‚à¹‰à¸­à¸¡à¸¹à¸¥ ToF/Ultrasonic (HC-SR04 à¹à¸¥à¸° A02YYUW) à¸à¸¥à¸±à¸šà¹„à¸›à¸¢à¸±à¸‡ PC à¸—à¸µà¹ˆà¸­à¸±à¸•à¸£à¸² **30 Hz à¸–à¸¶à¸‡ 50 Hz** à¹€à¸—à¹ˆà¸²à¸™à¸±à¹‰à¸™ à¹€à¸žà¸·à¹ˆà¸­à¹ƒà¸«à¹‰à¸ªà¸­à¸”à¸„à¸¥à¹‰à¸­à¸‡à¸à¸±à¸šà¸­à¸±à¸•à¸£à¸²à¹€à¸Ÿà¸£à¸¡à¹€à¸£à¸•à¸à¸²à¸£à¹à¸ªà¸”à¸‡à¸œà¸¥à¸‚à¸­à¸‡à¹‚à¸›à¸£à¹à¸à¸£à¸¡ Visual (30fps/60fps) à¹à¸¥à¸°à¸„à¸§à¸²à¸¡à¸ªà¸²à¸¡à¸²à¸£à¸–à¹ƒà¸™à¸à¸²à¸£à¸§à¸±à¸”à¸£à¸°à¸¢à¸°à¸—à¸²à¸‡à¸ˆà¸£à¸´à¸‡à¸‚à¸­à¸‡à¸Šà¸´à¸› ToF à¸‹à¸¶à¹ˆà¸‡à¹€à¸žà¸µà¸¢à¸‡à¸žà¸­à¸ªà¸³à¸«à¸£à¸±à¸šà¸£à¸°à¸šà¸šà¹‚à¸•à¹‰à¸•à¸­à¸šà¸—à¸µà¹ˆà¸¥à¸·à¹ˆà¸™à¹„à¸«à¸¥à¸£à¸°à¸”à¸±à¸šà¸ªà¸´à¸šà¸¡à¸´à¸¥à¸¥à¸´à¸§à¸´à¸™à¸²à¸—à¸µ
2.  **Audio Compression:** à¹„à¸¡à¹ˆà¸ªà¸•à¸£à¸µà¸¡à¸„à¸¥à¸·à¹ˆà¸™à¹€à¸ªà¸µà¸¢à¸‡à¹à¸­à¸™à¸°à¸¥à¹‡à¸­à¸à¸”à¸´à¸šà¸œà¹ˆà¸²à¸™à¹€à¸™à¹‡à¸•à¹€à¸§à¸´à¸£à¹Œà¸ à¹ƒà¸«à¹‰ ESP32 à¸„à¸³à¸™à¸§à¸“à¸«à¸²à¸„à¹ˆà¸²à¸„à¸§à¸²à¸¡à¸”à¸±à¸‡à¹€à¸‰à¸¥à¸µà¹ˆà¸¢ (RMS Peak) à¹à¸¥à¹‰à¸§à¸¢à¸´à¸‡à¸„à¹ˆà¸²à¸£à¸°à¸”à¸±à¸šà¸„à¸§à¸²à¸¡à¸”à¸±à¸‡ 1 à¸Šà¹ˆà¸­à¸‡ (8-bit) à¸—à¸µà¹ˆà¸„à¸§à¸²à¸¡à¸–à¸µà¹ˆ 45Hz à¹à¸—à¸™
3.  **Report-on-Change:** à¸›à¸¸à¹ˆà¸¡à¸à¸”à¸«à¸£à¸·à¸­à¹€à¸‹à¸™à¹€à¸‹à¸­à¸£à¹Œà¸›à¸£à¸°à¹€à¸ à¸—à¸—à¸£à¸´à¸à¹€à¸à¸­à¸£à¹Œà¸ªà¸–à¸²à¸™à¸° (PIR/à¸¡à¹ˆà¸²à¸™à¹à¸ªà¸‡) à¸ˆà¸°à¸ªà¹ˆà¸‡à¸‚à¹‰à¸­à¸¡à¸¹à¸¥à¹€à¸‰à¸žà¸²à¸°à¹€à¸¡à¸·à¹ˆà¸­à¹€à¸›à¸¥à¸µà¹ˆà¸¢à¸™à¸ªà¸–à¸²à¸™à¸°à¹€à¸—à¹ˆà¸²à¸™à¸±à¹‰à¸™ à¸žà¸£à¹‰à¸­à¸¡à¸£à¸°à¸šà¸š Debounce à¸›à¹‰à¸­à¸‡à¸à¸±à¸™à¸‚à¹‰à¸­à¸¡à¸¹à¸¥à¸£à¸šà¸à¸§à¸™
4.  **Target IP Unicast:** à¸šà¸±à¸‡à¸„à¸±à¸šà¹ƒà¸«à¹‰à¸•à¸±à¹‰à¸‡à¸„à¹ˆà¸² IP à¸‚à¸­à¸‡à¹€à¸›à¹‰à¸²à¸«à¸¡à¸²à¸¢ (TouchDesigner Host PC) à¸šà¸™à¸«à¸™à¹‰à¸² Web UI à¹€à¸žà¸·à¹ˆà¸­à¹ƒà¸«à¹‰à¸šà¸­à¸£à¹Œà¸”à¸ªà¹ˆà¸‡à¸‚à¹‰à¸­à¸¡à¸¹à¸¥à¸•à¸£à¸‡à¹„à¸›à¸¢à¸±à¸‡à¹€à¸„à¸£à¸·à¹ˆà¸­à¸‡à¸›à¸¥à¸²à¸¢à¸—à¸²à¸‡ à¹„à¸¡à¹ˆà¹ƒà¸Šà¹‰ Broadcast (`255.255.255.255`) à¸‹à¸¶à¹ˆà¸‡à¹€à¸›à¹‡à¸™à¸ªà¸²à¹€à¸«à¸•à¸¸à¹ƒà¸«à¹‰à¸ªà¸±à¸à¸à¸²à¸“ WiFi à¸«à¸™à¹ˆà¸§à¸‡à¹à¸¥à¸°à¸£à¸šà¸à¸§à¸™à¸šà¸­à¸£à¹Œà¸”à¸­à¸·à¹ˆà¸™
5.  **Task Pinning:** à¸šà¸±à¸‡à¸„à¸±à¸šà¹ƒà¸«à¹‰à¸à¸²à¸£à¸§à¸™à¸¥à¸¹à¸›à¸à¸²à¸£à¸­à¹ˆà¸²à¸™à¸„à¹ˆà¸²à¸®à¸²à¸£à¹Œà¸”à¹à¸§à¸£à¹Œà¹€à¸‹à¸™à¹€à¸‹à¸­à¸£à¹Œ (I2C/ADC/Pulse) à¹„à¸›à¸›à¸£à¸°à¸¡à¸§à¸¥à¸œà¸¥à¸­à¸¢à¸¹à¹ˆà¸šà¸™ **CPU Core 1** à¹à¸¥à¸°à¹à¸¢à¸à¸à¸²à¸£à¸›à¸£à¸°à¸¡à¸§à¸¥à¸œà¸¥à¹€à¸„à¸£à¸·à¸­à¸‚à¹ˆà¸²à¸¢à¹à¸¥à¸°à¸à¸²à¸£à¸£à¸±à¸šà¸ªà¸±à¸à¸à¸²à¸“à¹„à¸Ÿà¹€à¸­à¸²à¸•à¹Œà¸žà¸¸à¸•à¸«à¸¥à¸±à¸à¹ƒà¸«à¹‰à¸­à¸¢à¸¹à¹ˆà¸šà¸™ **CPU Core 0** à¹€à¸žà¸·à¹ˆà¸­à¸›à¹‰à¸­à¸‡à¸à¸±à¸™à¹„à¸¡à¹ˆà¹ƒà¸«à¹‰à¸à¸²à¸£à¸«à¸™à¹ˆà¸§à¸‡à¹€à¸§à¸¥à¸²à¸à¸²à¸£à¸›à¸£à¸°à¸¡à¸§à¸¥à¸œà¸¥à¸‚à¸­à¸‡à¹€à¸‹à¸™à¹€à¸‹à¸­à¸£à¹Œà¹„à¸›à¸—à¸³à¹ƒà¸«à¹‰à¸ˆà¸±à¸‡à¸«à¸§à¸°à¸à¸²à¸£à¹€à¸›à¸´à¸”/à¸›à¸´à¸”à¹„à¸Ÿà¸ªà¹€à¸•à¸ˆà¹€à¸à¸´à¸”à¸­à¸²à¸à¸²à¸£à¸à¸£à¸°à¸•à¸¸à¸

### 3.4 à¸à¸²à¸£à¸›à¸£à¸°à¸¢à¸¸à¸à¸•à¹Œà¹ƒà¸Šà¹‰à¹‚à¸«à¸¡à¸” Stepper à¸à¸±à¸š AC Servo (AC Servo Application)
*   **à¸­à¸´à¸™à¹€à¸—à¸­à¸£à¹Œà¹€à¸Ÿà¸‹à¸„à¸§à¸šà¸„à¸¸à¸¡:** AC Servo Driver à¸—à¸±à¹ˆà¸§à¹„à¸›à¹ƒà¸Šà¹‰à¸­à¸´à¸™à¹€à¸—à¸­à¸£à¹Œà¹€à¸Ÿà¸‹à¹à¸šà¸š Step/Dir (Pulse/Direction) à¸‹à¸¶à¹ˆà¸‡à¸ªà¸²à¸¡à¸²à¸£à¸–à¸—à¸³à¸‡à¸²à¸™à¸£à¹ˆà¸§à¸¡à¸à¸±à¸šà¹‚à¸«à¸¡à¸” Stepper (FastAccelStepper) à¹„à¸”à¹‰à¸—à¸±à¸™à¸—à¸µ
*   **à¸à¸²à¸£à¹€à¸¥à¸·à¸­à¸à¸ˆà¸³à¸™à¸§à¸™à¹„à¸šà¸•à¹Œà¸žà¸´à¸à¸±à¸”à¸•à¸³à¹à¸«à¸™à¹ˆà¸‡:** 
    *   **à¹à¸™à¸°à¸™à¸³à¹ƒà¸«à¹‰à¹€à¸¥à¸·à¸­à¸à¹‚à¸«à¸¡à¸” 32-bit (4 à¹„à¸šà¸•à¹Œ) à¸ªà¸³à¸«à¸£à¸±à¸šà¸Šà¹ˆà¸­à¸‡à¸ªà¸±à¸à¸à¸²à¸“à¹€à¸›à¹‰à¸²à¸«à¸¡à¸²à¸¢à¸•à¸³à¹à¸«à¸™à¹ˆà¸‡ (Target Position)** 
    *   *à¹€à¸«à¸•à¸¸à¸œà¸¥:* à¹€à¸žà¸·à¹ˆà¸­à¸›à¹‰à¸­à¸‡à¸à¸±à¸™à¸à¸²à¸£à¸«à¸¡à¸¸à¸™à¸žà¸´à¸à¸±à¸”à¸•à¸³à¹à¸«à¸™à¹ˆà¸‡à¸¥à¹‰à¸™ (Position Overflow) à¹€à¸™à¸·à¹ˆà¸­à¸‡à¸ˆà¸²à¸à¸¡à¸­à¹€à¸•à¸­à¸£à¹Œ AC Servo à¸¡à¸µà¸„à¸§à¸²à¸¡à¹€à¸£à¹‡à¸§à¸£à¸­à¸šà¸ªà¸¹à¸‡ (3000 RPM) à¹à¸¥à¸°à¸¡à¸µà¸„à¸§à¸²à¸¡à¸¥à¸°à¹€à¸­à¸µà¸¢à¸”à¸ªà¸¹à¸‡ (à¹€à¸Šà¹ˆà¸™ à¸•à¸±à¹‰à¸‡à¸„à¹ˆà¸²à¸£à¸±à¸šà¸žà¸±à¸¥à¸ªà¹Œà¸—à¸µà¹ˆ 10,000 pulses/rev) à¸—à¸³à¹ƒà¸«à¹‰à¹„à¸”à¹‰à¸Šà¹ˆà¸§à¸‡à¸žà¸´à¸à¸±à¸”à¸•à¸³à¹à¸«à¸™à¹ˆà¸‡à¸§à¸´à¹ˆà¸‡à¸ªà¸¹à¸‡à¸ªà¸¸à¸”à¸–à¸¶à¸‡ 429,496 à¸£à¸­à¸š
*   **à¹à¸œà¸™à¸œà¸±à¸‡à¹à¸Šà¸™à¹€à¸™à¸¥ DMX à¸—à¸µà¹ˆà¹€à¸«à¸¡à¸²à¸°à¸ªà¸¡ (7 à¸Šà¹ˆà¸­à¸‡à¸ªà¸±à¸à¸à¸²à¸“à¸•à¹ˆà¸­à¸¡à¸­à¹€à¸•à¸­à¸£à¹Œ):**
    *   `DMX 1-4` (4 à¹„à¸šà¸•à¹Œ) $\rightarrow$ à¸žà¸´à¸à¸±à¸”à¹€à¸›à¹‰à¸²à¸«à¸¡à¸²à¸¢ 32-bit (Target Position)
    *   `DMX 5-6` (2 à¹„à¸šà¸•à¹Œ) $\rightarrow$ à¸ªà¸›à¸µà¸”à¸„à¸§à¸²à¸¡à¹€à¸£à¹‡à¸§à¸žà¸±à¸¥à¸ªà¹Œ 16-bit (Travel Speed)
    *   `DMX 7` (1 à¹„à¸šà¸•à¹Œ) $\rightarrow$ à¸„à¸³à¸ªà¸±à¹ˆà¸‡à¸—à¸£à¸´à¸à¸„à¸§à¸šà¸„à¸¸à¸¡à¸£à¸°à¸šà¸š (Command: Run, Stop, Home, Reset, Disable)
*   **à¸‚à¹‰à¸­à¹à¸™à¸°à¸™à¸³à¹€à¸£à¸·à¹ˆà¸­à¸‡à¸ªà¸›à¸µà¸”:** ESP32 à¸‚à¸±à¸šà¸žà¸±à¸¥à¸ªà¹Œà¹„à¸”à¹‰à¹€à¸ªà¸–à¸µà¸¢à¸£à¸—à¸µà¹ˆ ~200kHz-300kHz à¹€à¸žà¸·à¹ˆà¸­à¹ƒà¸«à¹‰à¹„à¸”à¸£à¸Ÿà¹Œà¹€à¸‹à¸­à¸£à¹Œà¹‚à¸§à¸«à¸¡à¸¸à¸™à¸ªà¸›à¸µà¸”à¸ªà¸¹à¸‡à¸ªà¸¸à¸”à¹„à¸”à¹‰à¸—à¸µà¹ˆ 3000 RPM à¹à¸™à¸°à¸™à¸³à¹ƒà¸«à¹‰à¸ªà¹€à¸à¸¥à¸•à¸±à¹‰à¸‡à¹€à¸Ÿà¸·à¸­à¸‡à¸­à¸´à¹€à¸¥à¹‡à¸à¸—à¸£à¸­à¸™à¸´à¸à¸ªà¹Œ (Electronic Gear Ratio) à¹ƒà¸™à¸•à¸±à¸§à¹„à¸”à¸£à¸Ÿà¹Œà¹€à¸§à¸­à¸£à¹Œà¹€à¸‹à¸­à¸£à¹Œà¹‚à¸§à¹ƒà¸«à¹‰à¸­à¸¢à¸¹à¹ˆà¹ƒà¸™à¸Šà¹ˆà¸§à¸‡ **4,000 - 5,000 pulses/rev**
*   **à¸£à¸°à¸šà¸šà¸•à¸±à¸§à¸„à¸¹à¸“à¸­à¸±à¸•à¸£à¸²à¸—à¸” (Software Scale Factor):** à¸£à¸­à¸‡à¸£à¸±à¸šà¸à¸²à¸£à¸›à¹‰à¸­à¸™à¸•à¸±à¸§à¸„à¸¹à¸“à¸—à¸¨à¸™à¸´à¸¢à¸¡ (à¹€à¸Šà¹ˆà¸™ `Scale Factor = 80.0`) à¹€à¸žà¸·à¹ˆà¸­à¹à¸›à¸¥à¸‡à¸„à¹ˆà¸² DMX à¹ƒà¸«à¹‰à¸ˆà¹ˆà¸²à¸¢à¸žà¸±à¸¥à¸ªà¹Œà¸—à¸§à¸µà¸„à¸¹à¸“à¸­à¸­à¸à¹„à¸› à¹€à¸«à¸¡à¸²à¸°à¸ªà¸³à¸«à¸£à¸±à¸šà¸‡à¸²à¸™à¹€à¸à¸µà¸¢à¸£à¹Œà¸—à¸”à¸£à¸­à¸šà¸ªà¸¹à¸‡ à¸«à¸£à¸·à¸­à¸à¸²à¸£à¹à¸›à¸¥à¸‡à¸«à¸™à¹ˆà¸§à¸¢ DMX à¹€à¸›à¹‡à¸™à¸£à¸°à¸¢à¸°à¸—à¸²à¸‡à¸„à¸§à¸²à¸¡à¸¢à¸²à¸§à¸—à¸²à¸‡à¸à¸²à¸¢à¸ à¸²à¸žà¸¡à¸´à¸¥à¸¥à¸´à¹€à¸¡à¸•à¸£ (mm) à¸•à¸£à¸‡à¸•à¸±à¸§à¹€à¸›à¹Šà¸° à¹€à¸Šà¹ˆà¸™ à¸•à¸±à¹‰à¸‡à¸•à¸±à¸§à¸„à¸¹à¸“à¹€à¸›à¹‡à¸™ 80.0 à¹€à¸¡à¸·à¹ˆà¸­à¸œà¸¹à¹‰à¹ƒà¸Šà¹‰à¸­à¸­à¸à¸„à¸³à¸ªà¸±à¹ˆà¸‡ DMX = 150 à¸šà¸­à¸£à¹Œà¸”à¸ˆà¸°à¸«à¸¡à¸¸à¸™ 12,000 steps à¹€à¸¥à¸·à¹ˆà¸­à¸™à¹à¸—à¹ˆà¸™à¸ªà¹„à¸¥à¸”à¹Œà¹„à¸› 150 mm à¸—à¸±à¸™à¸—à¸µà¹‚à¸”à¸¢à¹„à¸¡à¹ˆà¸•à¹‰à¸­à¸‡à¸„à¸³à¸™à¸§à¸“à¸‹à¸±à¸šà¸‹à¹‰à¸­à¸™à¹ƒà¸™à¹‚à¸›à¸£à¹à¸à¸£à¸¡à¸•à¹‰à¸™à¸—à¸²à¸‡

---

## 4. à¹à¸œà¸™à¸‡à¸²à¸™à¸‚à¸±à¹‰à¸™à¸•à¸­à¸™à¸–à¸±à¸”à¹„à¸› (Next Steps Timeline)

à¹€à¸¡à¸·à¹ˆà¸­à¹„à¸”à¹‰à¸£à¸±à¸šà¸­à¸™à¸¸à¸¡à¸±à¸•à¸´à¹à¸œà¸™à¸‡à¸²à¸™à¸­à¸­à¸à¹à¸šà¸šà¹ƒà¸™à¹€à¸§à¸­à¸£à¹Œà¸Šà¸±à¸™ v1.11.0 à¸™à¸µà¹‰à¹à¸¥à¹‰à¸§ à¸ˆà¸°à¹€à¸£à¸´à¹ˆà¸¡à¸”à¸³à¹€à¸™à¸´à¸™à¸à¸²à¸£à¸”à¸±à¸‡à¸™à¸µà¹‰:
1.  **à¸ªà¸£à¹‰à¸²à¸‡à¹à¸¥à¸°à¸›à¸£à¸±à¸šà¸›à¸£à¸¸à¸‡à¹‚à¸„à¸£à¸‡à¸ªà¸£à¹‰à¸²à¸‡à¸‚à¹‰à¸­à¸¡à¸¹à¸¥à¸­à¸´à¸™à¸žà¸¸à¸•/à¹€à¸­à¸²à¸•à¹Œà¸žà¸¸à¸•:**
    *   à¸­à¸±à¸›à¹€à¸”à¸•à¹‚à¸¡à¹€à¸”à¸¥à¸žà¸´à¸™à¸œà¸ªà¸¡à¹ƒà¸™ `struct OutputChannel` à¹à¸¥à¸°à¹€à¸žà¸´à¹ˆà¸¡à¹€à¸¡à¸™à¸¹ Expanders à¸šà¸±à¸™à¸—à¸¶à¸ I2C pins à¸¥à¸‡à¸šà¸™ NVS
2.  **à¹€à¸‚à¸µà¸¢à¸™à¸£à¸°à¸šà¸šà¸‚à¸±à¸šà¸­à¸´à¸™à¸žà¸¸à¸•à¹€à¸‹à¸™à¹€à¸‹à¸­à¸£à¹Œ:**
    *   à¸ªà¸£à¹‰à¸²à¸‡à¹„à¸Ÿà¸¥à¹Œà¹„à¸¥à¸šà¸£à¸²à¸£à¸µà¸•à¸±à¸§à¹à¸›à¸£à¹€à¸šà¸²à¸ªà¸³à¸«à¸£à¸±à¸šà¸­à¹ˆà¸²à¸™ ToF VL53L0X, à¸ªà¸±à¸à¸à¸²à¸“ Echo à¸‚à¸­à¸‡ HC-SR04, à¹à¸¥à¸°à¸à¸²à¸£à¸­à¹ˆà¸²à¸™ ADC à¸‚à¸­à¸‡à¹„à¸¡à¹‚à¸„à¸£à¹‚à¸Ÿà¸™
3.  **à¹€à¸‚à¸µà¸¢à¸™à¹‚à¸¡à¸”à¸¹à¸¥à¸à¸²à¸£à¸ªà¹ˆà¸‡à¸à¸¥à¸±à¸šà¹€à¸„à¸£à¸·à¸­à¸‚à¹ˆà¸²à¸¢ (Feedback Module):**
    *   à¸ªà¸£à¹‰à¸²à¸‡à¸£à¸°à¸šà¸šà¹à¸›à¸¥à¸‡à¸ªà¸±à¸”à¸ªà¹ˆà¸§à¸™à¸‚à¹‰à¸­à¸¡à¸¹à¸¥à¸ªà¹ˆà¸‡à¸­à¸­à¸à¸œà¹ˆà¸²à¸™ UDP Art-Net (ArtDmx) à¹à¸¥à¸°à¸£à¸¹à¸›à¹à¸šà¸š OSC
4.  **à¸›à¸£à¸±à¸šà¸›à¸£à¸¸à¸‡à¸«à¸™à¹‰à¸²à¹€à¸§à¹‡à¸šà¸­à¸´à¸™à¹€à¸—à¸­à¸£à¹Œà¹€à¸Ÿà¸ª (Web UI):**
    *   à¹€à¸žà¸´à¹ˆà¸¡à¸ªà¸¥à¹‡à¸­à¸•à¹€à¸¡à¸™à¸¹à¸•à¸±à¹‰à¸‡à¸„à¹ˆà¸²à¸ªà¸³à¸«à¸£à¸±à¸šà¸­à¸´à¸™à¸žà¸¸à¸•à¹€à¸‹à¸™à¹€à¸‹à¸­à¸£à¹Œà¹à¸¥à¸°à¸•à¸±à¸§à¹à¸›à¸¥à¸‡à¸„à¸§à¸²à¸¡à¸–à¸µà¹ˆà¸‚à¸­à¸‡ I2C à¸šà¸±à¸ª
5.  **à¸„à¸­à¸¡à¹„à¸žà¸¥à¹Œà¸›à¸£à¸°à¹€à¸¡à¸´à¸™à¸œà¸¥à¸‚à¸™à¸²à¸”à¹à¸¥à¸°à¸ˆà¸±à¸”à¸—à¸³ Walkthrough à¸à¸²à¸£à¸—à¸”à¸ªà¸­à¸š**
6.  **à¸§à¸²à¸‡à¹à¸œà¸™à¸¨à¸¶à¸à¸©à¸²à¹à¸¥à¸°à¹€à¸•à¸£à¸µà¸¢à¸¡à¹‚à¸„à¸£à¸‡à¸ªà¸£à¹‰à¸²à¸‡à¸à¸²à¸£à¸žà¸±à¸’à¸™à¸²à¸«à¸™à¹‰à¸²à¸ˆà¸­à¹à¸ªà¸”à¸‡à¸ªà¸–à¸²à¸™à¸° Shared I2C Diagnostics Display (OLED & Character LCD 16x2/20x4) à¹ƒà¸™à¹€à¸Ÿà¸ªà¸–à¸±à¸”à¹„à¸› (à¸£à¸°à¸”à¸±à¸šà¸„à¸§à¸²à¸¡à¸ªà¸³à¸„à¸±à¸: à¸•à¹ˆà¸³à¸¡à¸²à¸ / Low Priority)**

