# WT32-ETH01 / ESP32 Hardware Resource Calculator

This document describes how the firmware estimates hardware load and applies hard safety constraints for WT32-ETH01 output configurations.

The scoring contract is in `docs/domain_model.md`. Firmware implementation lives in `include/scoring.h`, with matching JavaScript expected in `web/index.html`.

## 1. Peripheral Limits

The ESP32 peripherals are shared by all configured outputs.

| Peripheral | Max Capacity | Usage Rules | Affected Output Types |
| :--- | :--- | :--- | :--- |
| **RMT Channels** | 8 channels | 1 RMT channel per LED strip. 1 RMT channel per software-based DMX serial output. | LED Strip, DMX Serial fallback |
| **LEDC PWM** | 16 channels | Channels 0-15. Used to generate PWM signals for dimmers, servos, motors, buzzers, 7-segment direct drive, PWM DAC, and function generator. | Types 4, 5, 6, 8, 9, 12, 13, 15, 16 |
| **UART Ports** | 2 usable ports | UART0 is reserved for console/debug. UART1 and UART2 are shared by DFPlayer and hardware DMX. DFPlayer has priority; DMX falls back to RMT. | DFPlayer, DMX Serial |
| **Configured Outputs** | Score-limited | The firmware uses resource score plus hard peripheral validation instead of the old fixed 8-channel capacity rule. | All output types |
| **GPIO Outputs** | About 8 pins | Physical header limitation of WT32-ETH01. Many ESP32 pins are reserved by LAN8720 Ethernet. | Direct-connected outputs |

## 2. Resource Footprint by Output Type

The current v3 output types are `0..18`. Actual resource use depends on selected source, mode, and per-pin routing.

Resource scoring is intended to be routing-accurate: firmware and Web UI score calculations should account for selected sources, hybrid GPIO/PCA/expander pins, segment-level routing, and DMX UART-to-RMT fallback after DFPlayer UART allocation. Hard validation remains a separate gate for interlocks and absolute peripheral limits.

Implementation status: this is the domain rule and target behavior. When changing scoring code, audit both `include/scoring.h` and `web/index.html`; the C++ JSON scoring path must copy every routing field it needs from saved/output JSON before estimating resources.

| Type | Output Type | Typical ESP32 GPIO | RMT | LEDC | UART | Expander / PCA Notes |
| :---: | :--- | :---: | :---: | :---: | :---: | :--- |
| 0 | AC Dimmer | 1 | 0 | 0 | 0 | GPIO only; uses shared zero-crossing input |
| 1 | DMX Serial Output | 1 | fallback | 0 | 1 preferred | GPIO only; uses UARTs left after DFPlayer, then RMT fallback |
| 2 | Relay | 1 | 0 | 0 | 0 | GPIO, PCA9685, or digital expander |
| 3 | RGB/RGBW LED Strip | 1 | 1 | 0 | 0 | GPIO only; requires RMT timing |
| 4 | Single Color LED | 1 | 0 | 1 | 0 | GPIO+LEDC or PCA9685 |
| 5 | Analog RGB/RGBW | 3-4 | 0 | 3-4 | 0 | Each color can route to GPIO or PCA9685 |
| 6 | DC Motor | 2-3 | 0 | 1-2 | 0 | GPIO or PCA9685; EN pin cannot use digital expander |
| 7 | Stepper Motor | 2-3 | 2 | 0 | 0 | STEP must be GPIO; DIR/ENABLE may be hybrid routed |
| 8 | RC Servo | 1 | 0 | 1 | 0 | GPIO+LEDC or PCA9685; servo forces PCA chip to 50 Hz |
| 9 | Passive Buzzer | 1 | 0 | 1 | 0 | GPIO only |
| 10 | DFPlayer MP3 | 2 | 0 | 0 | 1 | GPIO only; max 2 channels |
| 11 | 7-Segment TM1637 | 2 | 0 | 0 | 0 | GPIO only |
| 12 | 7-Segment DD 7-Pin PWM | per segment | 0 | routing-dependent | 0 | Direct Dim uses GPIO/PCA9685 only; No Dim/Common Dim may use digital expanders for digital segment pins |
| 13 | 7-Segment DD 8-Pin PWM | per segment | 0 | routing-dependent | 0 | Direct Dim uses GPIO/PCA9685 only; No Dim/Common Dim may use digital expanders for digital segment pins |
| 14 | DAC | 1 | 0 | 0 | 0 | GPIO DAC is unavailable on WT32-ETH01; MCP4725 source recommended |
| 15 | PWM DAC | 1 | 0 | 1 | 0 | GPIO+LEDC or PCA9685; duty calibration supports external 0-10V / 4-20mA interface circuits |
| 16 | Function Generator | 1 | 0 | 1 | 0 | GPIO only; uses timer/compute budget |
| 17 | Solenoid Trigger | 1 | 0 | 0 | 0 | GPIO, PCA9685, or digital expander |
| 18 | Sequential Smoke Shooter | 2 | 0 | 0 | 0 | GPIO, PCA9685, or digital expander |

## 3. Score Formula

Resource score weights:

| Resource | Weight |
| --- | ---: |
| GPIO | 0.5 |
| LEDC | 2.5 |
| RMT | 3.0 |
| UART | 8.0 |
| DAC | 2.0 |
| PCA9685 channel | 0.25 |
| Digital expander channel | 0.125 |

Formula:

```text
resourceScore = GPIO*0.5 + LEDC*2.5 + RMT*3.0 + UART*8.0 + DAC*2.0 + PCA*0.25 + EXP*0.125
computeScore  = sum(type compute cost) + (output_fps / 60) * 5
totalScore    = resourceScore + computeScore
```

The firmware limit is `SCORE_LIMIT`, currently `resourceScoreLimit() + 25.0`.

Compute score is intentionally separate from hard peripheral validation. A configuration must pass both total score and interlock checks.

## 4. WT32-ETH01 Physical Pin Allocations & Safety

The WT32-ETH01 is highly integrated. Many ESP32 GPIOs are wired internally to the LAN8720 Ethernet PHY chip. Using these pins for outputs can break networking or crash the system.

### Internal Reserved Pins

* **Ethernet RMII interface:** GPIO0, GPIO16, GPIO18, GPIO19, GPIO21, GPIO22, GPIO23, GPIO25, GPIO26, GPIO27
* **Debug UART0:** GPIO1 (TX0), GPIO3 (RX0)
* **ESP32 DAC pins:** GPIO25/GPIO26 are occupied by Ethernet on WT32-ETH01, so MCP4725 is the practical DAC path.

### Exposed Header Pins

* **Common safe outputs:** GPIO2, GPIO4, GPIO12, GPIO14, GPIO15, GPIO17, GPIO32, GPIO33
* **Input-only pins:** GPIO34, GPIO35, GPIO36, GPIO39
* **Default status LED pin:** GPIO5
* **Default I2C pins:** GPIO14 SDA, GPIO15 SCL
* **Zero-crossing pin:** User-configured; input-only GPIO is recommended.

Because only a small number of general output pins are available, I2C expanders are strongly recommended for installations with many relays, solenoids, indicators, or 7-segment segments.

## 5. I2C Device Specifications

### 5.1 PCA9685 PWM/Servo Expander

* **Interface:** I2C.
* **Address range:** `0x40..0x47`.
* **Channels:** 16 PWM outputs per chip.
* **Best for:** Servos, PWM dimmers, analog RGB/RGBW, PWM DAC, relay drivers, and motor signals.
* **Key constraint:** All 16 channels on the same chip share one PWM frequency. If any servo uses a chip, the chip is forced to 50 Hz.

### 5.2 MCP23017 / TCA9555 Digital GPIO Expander

* **Interface:** I2C.
* **Address range:** `0x20..0x27`.
* **Best for:** Relays, solenoids, smoke shooter valves, and digital-only segment routing.
* **Key constraint:** Digital only; do not use for PWM-only pins such as motor EN or 7-segment Direct Dim segments.

### 5.3 PCF857x Digital GPIO Expander

* **Interface:** I2C.
* **Address range:** `0x20..0x27` for PCF8574, `0x38..0x3F` for PCF8574A.
* **Best for:** Simple relay modules and low-speed digital outputs.
* **Key constraint:** Quasi-bidirectional pins have weak pull-ups; active-low wiring is often easier.

### 5.4 I2C DACs

* **Interface:** I2C.
* **Resolution:** 12-bit.
* **Best for:** Analog output when ESP32 DAC pins are unavailable.
* **MCP4725 (`dac_model=0`) address range:** `0x60` or `0x61`.
* **DAC7571 (`dac_model=1`) address range:** `0x4C` or `0x4D`.
* **DAC7573 (`dac_model=2`) address range:** `0x4C..0x5B`; `pca_channel` selects channel A-D.

### 5.5 I2C Displays

* **SSD1306/SH1106 OLED address range:** `0x3C` or `0x3D`.
* **PCF8574 LCD backpack address range:** `0x27` or `0x3F`.
* **Key constraint:** Display addresses are validated against the selected display type and are not treated as output sources.

## 6. Hard Validation Rules

The score calculator is not the only gate. Firmware and Web UI validation also enforce these rules:

* GPIO pins cannot duplicate another output pin.
* Output pins cannot overlap Status LED, Zero-Crossing, I2C SDA, or I2C SCL.
* I2C SDA and SCL cannot be the same pin.
* I2C output addresses must match the selected source/model: PCA9685 `0x40..0x47`, MCP23017/TCA9555 `0x20..0x27`, PCF857x `0x20..0x27` or `0x38..0x3F`, MCP4725 `0x60/0x61`, DAC7571 `0x4C/0x4D`, DAC7573 `0x4C..0x5B`.
* I2C display addresses must match the selected display type: OLED `0x3C/0x3D`, PCF8574 LCD `0x27/0x3F`.
* DFPlayer channels cannot exceed 2.
* DFPlayer allocates UART before DMX; DMX uses remaining UARTs and then RMT fallback.
* RMT usage from LED strips plus DMX fallback cannot exceed 8.
* Source must be compatible with output type.
* Stepper STEP must be ESP32 GPIO.
* Motor EN in `IN1+IN2+EN` mode must use ESP32 GPIO or PCA9685.
* 7-segment Direct Dim modes must use ESP32 GPIO or PCA9685 for segment outputs; digital GPIO expanders are allowed only for digital segment modes.
* PCA9685 frequency conflicts must be handled per chip.

## 7. Capacity & Network Load Guidelines

1. **Total LED Limit**
    * Keep total NeoPixels under about 4,000 pixels to avoid memory starvation.
    * Long individual strips reduce refresh rate because WS281x timing is serialized.

2. **Bluetooth/BLE Excluded**
    * Bluetooth/BLE is intentionally excluded because it consumes flash/RAM needed for OTA and Ethernet stability.

3. **7-Segment Display Limits**
    * TM1637 modules are GPIO-only and consume two signal pins each.
    * Direct-drive 7-segment No Dim/Common Dim segment outputs can use digital expanders to preserve ESP32 GPIO.
    * Direct Dim segment outputs need PWM and should use PCA9685 when there are not enough ESP32 GPIO/LEDC channels.

4. **Mixed-Mode Optimization**
    * Fast timing pins such as Stepper STEP must stay on direct ESP32 GPIO.
    * Slow state pins such as Stepper DIR/ENABLE can be moved to expanders.

5. **Power Budget Planning**
    * Do not drive relays, servos, motors, solenoids, or LED strips from ESP32 3.3V pins.
    * Use separate load power and connect grounds together.
    * Use flyback protection for relay, solenoid, and motor coils.
    * All GPIO outputs should have pull-down resistors when connected to interface boards.

6. **ESP-NOW Airtime Planning**
    * ESP-NOW forwarding is not part of the hardware resource score, but it can become a wireless airtime bottleneck.
    * Current DMX forwarding is chunked into 200-byte payloads plus a 12-byte firmware header, so a full 512-channel universe uses up to 3 ESP-NOW packets.
    * If a peer's DMX range is larger than the chunk size, the master sends multiple packets with increasing `offset`; the slave applies each packet by `universe` and `offset`.
    * Future Web UI/system settings should allow user-configurable ESP-NOW chunk size because RF reliability and airtime overhead vary by installation.
    * Configure per-peer universe/channel ranges instead of broadcasting all data to every slave.
    * Slave boards keep their setup AP available for field configuration; avoid unnecessary clients/AP traffic during show-critical wireless operation.

## 8. Scoring System Verification & Calibration

To ensure the scoring system aligns with the actual physical limits of the WT32-ETH01 / ESP32 hardware and does not cause task crashes or output stutter, follow these verification methodologies:

### 8.1 Hardware Parity Verification (Mathematical Alignment)
Compare weight constants to physical limitations to confirm that single-resource exhaustion triggers a score overload:
- **GPIO limit (8 pins):** `8 * W_GPIO (0.5) = 4.0` points
- **LEDC PWM limit (16 channels):** `16 * W_LEDC (2.5) = 40.0` points
- **RMT channel limit (8 channels):** `8 * W_RMT (3.0) = 24.0` points
- **UART port limit (2 usable):** `2 * W_UART (8.0) = 16.0` points
- **Resource limit sum:** `4.0 + 40.0 + 24.0 + 16.0 = 84.0` (matching `resourceScoreLimit()`).
- This mathematically guarantees that if any single physical resource is exhausted, the score reflects it.

### 8.2 Runtime Benchmarking (Performance & Latency Profiling)
Measure real-time indicators on physical boards to calibrate compute score values (e.g. Stepper: `2.0`, Function Gen: `2.0`):
1. **Output Jitter & Frame Latency:** Load a configuration close to `SCORE_LIMIT` (e.g., combined score of 100-109), send Art-Net packets at 40 FPS, and capture physical DMX/LED output lines using an oscilloscope or logic analyzer. If frame drops or jitter occur, the `SCORE_LIMIT` is too high or the compute weights are too low.
2. **Heap Memory Audit:** Monitor free RAM (`ESP.getFreeHeap()`) under active DMX load. If heap falls below **30-40 KB**, lower the `SCORE_LIMIT` or increase the weights of memory-heavy channels (like WS2812B strips).
3. **Task Execution Time:** Check if the output loop task (Core 1) triggers the FreeRTOS watchdog or experiences scheduling delays.

### 8.3 I2C Bus Contention Stress Testing
Since all PCA9685 and digital expanders share a single I2C bus (`Wire`), high FPS updates can saturate the bus:
- **Test:** Measure transaction duration for all expander updates inside the output loop.
- **Calibration:** If I2C transactions take longer than **15-20 ms** (leaving too little time for the remaining 25 ms cycle at 40 FPS), increase the expander weights (`0.125` / `0.25`) to limit the count of allowed expander channels.

### 8.4 Simulation & Mock Validation
Run the Python offline calculator `tools/load_calculator.py` against boundary/extreme mock configurations to check if the interlock validation matches the C++ firmware and JavaScript Web UI scoring engine logic.
