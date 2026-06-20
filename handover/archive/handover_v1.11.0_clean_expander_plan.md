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

### Low-Cost GPIO: PCF8575
Source reference:
- TI PCF8575 page: `https://www.ti.com/product/PCF8575`

Why it fits:
- 16-bit digital I/O expander over I2C (dual 8-bit ports).
- Extremely popular for DIY electronics and low-cost relays.
- Pin-compatible upgrade over PCF8574 for 16-channel digital output.

Limitations:
- Quasi-bidirectional I/O structure requires careful output pull-up behavior.
- No hardware PWM.
- Lower sink/source current than MCP23017.

Recommendation:
- Ideal for low-cost 16-channel relay banks or status LED arrays.

### 8-Bit Digital I/O: MCP23008
Source reference:
- Microchip MCP23008 page: `https://www.microchip.com/en-us/product/MCP23008`

Why it fits:
- 8-bit version of the highly stable MCP23017.
- Excellent I2C signal stability and true output latching registers.
- Ideal when only 8 channels are needed and PCF8574's quasi-bidirectional pin state behavior is undesirable.

Limitations:
- Only 8 channels compared to 16 channels on the MCP23017.
- No hardware PWM.

Recommendation:
- Best choice for compact 8-channel relay boards requiring robust digital signals.

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
1. **PCA9685** (16-Ch PWM/Servo): High priority for Servo and Dimmer outputs.
2. **MCP23017 / TCA9555** (16-Ch General GPIO): High priority for robust digital I/O (Relays, Solenoids, Stepper DIR/EN, Homing switches).
3. **PCF8575 / PCF8574** (16/8-Ch General GPIO): Medium priority for low-cost digital output banks.
4. **MCP23008** (8-Ch General GPIO): Medium priority for compact and stable digital outputs.
5. **TPIC6B595** (8-Ch Power Sink): Low priority for high-voltage direct relay/solenoid sinking.
6. **TLC5947** (24-Ch Constant-Current PWM): Low priority for multi-channel analog LEDs.

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

## I2C Expander Compatibility Matrix (ตารางเปรียบเทียบความเข้ากันได้)

| Output Mode (โหมดเอาต์พุต) | PCA9685 (PWM/Servo) | MCP23017 / TCA9555 (Digital GPIO) | PCF8574 / PCF8575 (Digital GPIO) | ESP32 Main Board (GPIO) | คำแนะนำและข้อจำกัดเพิ่มเติม (Key Notes) |
| :--- | :---: | :---: | :---: | :---: | :--- |
| **Relay / Solenoid (On/Off)** | **OK** (ผ่าน MOSFET/Driver) | **RECOMMENDED** | **RECOMMENDED** | **RECOMMENDED** | ควบคุมแบบลอจิก ON/OFF ธรรมดา ความถี่ต่ำ ใช้ตัวไหนก็ได้ตามพินที่เหลือ |
| **PWM Dimmer (LED/Light)** | **RECOMMENDED** (12-bit HW) | **NO** | **NO** | **RECOMMENDED** (LEDC HW) | Digital GPIO expander ไม่มี Hardware PWM หากทำ Software PWM จะคลาดเคลื่อนและทำให้ไฟกระพริบ |
| **RC Servo Motor** | **RECOMMENDED** (50Hz HW) | **NO** | **NO** | **OK** | RC Servo ต้องการ Pulse width 1-2 ms ที่แม่นยำสูง (50Hz) |
| **DC Motor (Speed PWM)** | **OK** (ผ่าน Driver) | **NO** | **NO** | **RECOMMENDED** | หากควบคุมความเร็วมอเตอร์ด้วย PWM ต้องใช้ช่องสัญญาณ PWM (ESP32 หรือ PCA9685) |
| **DC Motor (Direction DIR/EN)** | **OK** | **RECOMMENDED** | **RECOMMENDED** | **OK** | ขาควบคุมทิศทางเป็นสัญญาณลอจิกความถี่ต่ำ สามารถส่งไปควบคุมผ่าน Expander ได้ดี |
| **Stepper Motor (STEP Pin)** | **NO** | **NO** | **NO** | **RECOMMENDED ONLY** | **สำคัญมาก!** STEP ต้องการพัลส์ความเร็วสูงระดับ kHz และแม่นยำระดับไมโครวินาที ห้ามนำไปไว้บน I2C เด็ดขาด |
| **Stepper Motor (DIR/EN Pins)** | **OK** | **RECOMMENDED** | **RECOMMENDED** | **OK** | ขา DIR/EN เปลี่ยนสถานะไม่บ่อยและไม่มีความอ่อนไหวต่อจังหวะเวลา สามารถย้ายไปไว้บน Expander ได้ |
| **Homing Limit Switch (Input)** | **NO** (Output Only) | **RECOMMENDED** (มี Interrupt) | **OK** (Quasi-IO) | **RECOMMENDED** | GPIO Expander รองรับ Input. แนะนำให้เชื่อมขา Interrupt ของ Expander กลับเข้า ESP32 1 ขาเพื่อตรวจจับทันที |
| **WS281x (Addressable LED)** | **NO** | **NO** | **NO** | **RECOMMENDED ONLY** | ต้องการ Timing ระดับนาโนวินาทีที่แม่นยำมาก ต้องใช้ ESP32 GPIO ร่วมกับฮาร์ดแวร์ RMT/I2S เท่านั้น |
| **DMX512 Output (Serial)** | **NO** | **NO** | **NO** | **RECOMMENDED ONLY** | ต้องการความเร็ว 250kbps และ UART hardware ต้องส่งจากพิน TX ของ ESP32 เท่านั้น |
| **Zero-Crossing AC Dimmer** | **NO** | **NO** | **NO** | **RECOMMENDED ONLY** | ต้องการ Hardware Interrupt บน ESP32 ตรวจจับจุดตัดศูนย์ทันทีระดับไมโครวินาทีเพื่อหน่วงเวลาสั่งทริก Triac |

---

## Mixed Output Architecture (การผสมผสานเอาต์พุตระหว่างบอร์ดหลักและ Expander)

ในระบบควบคุมมอเตอร์หรือสัญญาณความถี่สูง เพื่อประสิทธิภาพและประหยัดขาใช้งานของ ESP32 แนะนำให้ใช้โครงสร้างแบบ **Hybrid/Mixed Pin Mapping** ดังนี้:

### 1. โหมดมอเตอร์สเต็ปเปอร์ (Stepper Motor Hybrid Mapping)
- **ขา STEP (Pulse)**: **ต้องต่อเข้ากับ GPIO ของ ESP32 โดยตรงเท่านั้น**
  - *เหตุผล*: การสลับสัญญาณ STEP (High/Low) ทำงานด้วยความถี่สูง (ปกติ 1kHz ถึง 20kHz+) และมีจังหวะเวลา (timing) ที่ต้องนิ่งและรวดเร็ว หากสั่งงานผ่านบัส I2C จะเกิด Bus Latency (ประมาณ 0.5-2ms) ทำให้มอเตอร์หมุนกระตุก ไม่เรียบ หรือเกิดการสูญเสียจังหวะ (Step Loss)
- **ขา DIR (Direction)** และ **ขา ENABLE**: **สามารถเชื่อมต่อผ่าน I2C Expander ได้ทุกตัว** (MCP23017, TCA9555, PCF8574/5, PCA9685)
  - *เหตุผล*: สัญญาณกำหนดทิศทาง (DIR) และการเปิด/ปิดบอร์ดไดรเวอร์ (ENABLE) จะเปลี่ยนสถานะค่อนข้างช้า และจะทำเฉพาะตอนเริ่มต้นขยับมอเตอร์หรือเปลี่ยนทิศทางเท่านั้น จึงไม่มีปัญหาเรื่อง Bus Latency
  - *ตัวอย่างการเดินสาย (Wiring Example for 2 Steppers)*:
    - **Stepper 1**: STEP -> `ESP32 GPIO 4` | DIR -> `MCP23017 Port A0` | ENABLE -> `MCP23017 Port A1`
    - **Stepper 2**: STEP -> `ESP32 GPIO 5` | DIR -> `MCP23017 Port A2` | ENABLE -> `MCP23017 Port A3`
    - *ผลลัพธ์*: ประหยัดพินหลักของ ESP32 ได้ถึง 4 พิน!

```mermaid
graph TD
    subgraph ESP32 Main Board
        Core[ESP32 Controller]
        StepPin1[GPIO 4: STEP 1]
        StepPin2[GPIO 5: STEP 2]
    end

    subgraph I2C Bus
        SDA[SDA GPIO 13]
        SCL[SCL GPIO 16]
    end

    subgraph MCP23017 Expander
        DirPin1[Port A0: DIR 1]
        EnPin1[Port A1: EN 1]
        DirPin2[Port A2: DIR 2]
        EnPin2[Port A3: EN 2]
    end

    subgraph Stepper Driver 1 (A4988/TB6600)
        Dr1_Step[STEP Input]
        Dr1_Dir[DIR Input]
        Dr1_En[EN Input]
    end

    subgraph Stepper Driver 2 (A4988/TB6600)
        Dr2_Step[STEP Input]
        Dr2_Dir[DIR Input]
        Dr2_En[EN Input]
    end

    Core -->|Fast PWM / Timers| StepPin1
    Core -->|Fast PWM / Timers| StepPin2
    Core -->|I2C Command| SDA
    Core -->|I2C Command| SCL
    SDA & SCL --> MCP23017 Expander

    StepPin1 ==>|Direct Wire: Timing Critical| Dr1_Step
    DirPin1 -.->|Expander Wire: Low Speed| Dr1_Dir
    EnPin1 -.->|Expander Wire: Low Speed| Dr1_En

    StepPin2 ==>|Direct Wire: Timing Critical| Dr2_Step
    DirPin2 -.->|Expander Wire: Low Speed| Dr2_Dir
    EnPin2 -.->|Expander Wire: Low Speed| Dr2_En
```

### 2. โหมดมอเตอร์กระแสตรง (DC Motor Hybrid Mapping)
- **ขาควบคุมความเร็ว (PWM / Speed)**: ต่อเข้ากับ ESP32 (LEDC) หรือต่อเข้ากับ PCA9685 (PWM Channel)
- **ขาควบคุมทิศทาง (DIR / EN)**: สามารถต่อเข้ากับ Digital Expander ตัวใดก็ได้ หรือ PCA9685 (ในโหมด On/Off)
- *ผลลัพธ์*: สามารถคุมมอเตอร์ DC ได้หลายตัวโดยใช้ขา PWM จาก PCA9685 และใช้ขา DIR จาก MCP23017 หรือ PCF8574 ร่วมกันได้

### 3. สวิตช์ลิมิตจำกัดระยะทางร่วมกับโฮมมิ่ง (Homing Limit Switches & Interrupts)
- เพื่อความปลอดภัยและความรวดเร็วในการหยุดมอเตอร์เมื่อเดินชนลิมิตสวิตช์ (Homing)
- หากต่อสวิตช์เข้ากับ GPIO Expander (เช่น MCP23017) แนะนำให้ต่อพิน **INTA** หรือ **INTB** (Interrupt Output) ของ MCP23017 กลับมายังขา GPIO ของ ESP32 จำนวน 1 ขา (เช่น GPIO 12)
- *การทำงาน*: เมื่อลิมิตสวิตช์ถูกกด MCP23017 จะดึงขา Interrupt ลงทันที ESP32 จะจับสัญญาณอินเทอร์รัปต์นี้แล้วค่อยส่งคำสั่งอ่านค่าพอร์ตผ่านบัส I2C เพื่อระบุว่าสวิตช์ตัวไหนชน วิธีนี้ทำให้การตอบสนองรวดเร็วกว่าการทำ Polling ผ่านบัส I2C แบบวนลูปธรรมดา และไม่เปลืองโหลดของบัส I2C

---

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

