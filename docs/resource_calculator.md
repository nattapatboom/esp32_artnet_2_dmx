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
| 14 | DAC | 1 | 0 | 0 | 0 | GPIO DAC is unavailable on WT32-ETH01; I2C DAC source (5/6/7) recommended |
| 15 | PWM DAC | 1 | 0 | 1 | 0 | GPIO+LEDC or PCA9685; duty calibration supports external 0-10V / 4-20mA interface circuits |
| 16 | Function Generator | 1 | 0 | 1 | 0 | GPIO only; uses timer/compute budget |
| 17 | Solenoid Trigger | 1 | 0 | 0 | 0 | GPIO, PCA9685, or digital expander |
| 18 | Sequential Smoke Shooter | 2 | 0 | 0 | 0 | GPIO, PCA9685, or digital expander |

## 3. Score Formula (Refactored — 3 Independent Budgets)

The system uses **three independent budgets**. If any one is exceeded, saving is fully blocked.

### 3A. HardwareResource — Finite ESP32 Peripherals

| Resource | Max | Notes |
| :--- | ---: | :--- |
| LEDC | 16 | PWM timer channels |
| RMT | 8 | WS281x / DMX fallback |
| UART | 2 | DFPlayer or DMX |
| DAC | 2 | Internal DAC (GPIO 25/26, both occupied by Ethernet on WT32-ETH01) |
| Timer | 4 | AC Dimmer shared hardware timer; Function Generator timer-like runtime slots |

GPIO is NOT counted here because expanders can substitute for GPIO pins.
PCA9685 and digital expanders are NOT counted as hardware; they use I2C bus, reflected in CPU budget.

### 3B. CpuBudget — Output Service Time (Microseconds)

CPU budget is now modeled as **output service time** in microseconds per frame (µs/frame), not just core compute cycles. It includes blocking/serialized output work such as DMX512 transmit, LED strip pixel mapping/RMT enqueue, TM1637 bit-bang time, and active I2C transactions.

Timer-driven outputs have two costs: foreground set/update cost in the table below, plus background per-frame equivalent ISR/timer cost. AC Dimmer adds a shared hardware timer cost if any dimmer exists. Function Generator adds per-channel esp_timer ISR cost even when DMX values are unchanged.

Frame budget = `(1,000,000 / fps) - 1,500 µs`.

**Baseline overhead:** `500 µs` per frame for output loop iteration, atomic flag checks, channel traversal, status update, and RTOS jitter.

Higher FPS = less time per frame = smaller budget:
- 30 FPS → 33.3ms/frame → budget = 31,833 µs
- 40 FPS → 25.0ms/frame → budget = 23,500 µs
- 44 FPS → 22.7ms/frame → budget = 21,227 µs

| Type | µs/frame | Notes |
| :--- | ---: | :--- |
| AC Dimmer (0) | 5 + background timer cost | GPIO/state update; ZC timing handled by ISR; consumes 1 shared timer |
| DMX (1) | 22,600 | Full DMX512 frame transmit (513 slots × 11 bits @250kbps) |
| Relay (2) | 5 | Digital state update |
| RGB LED (3) | `80 + count×3` RGB, `80 + count×4` RGBW | CPU pixel mapping + RMT `Show()` enqueue only |
| Single LED (4) | 6 | 1 LEDC or PCA write setup |
| Analog RGB (5) | 18 | 3-4 PWM updates before I2C additions |
| Motor (6) | 35 | Deadband/direction/PWM calculation |
| Stepper (7) | 80 | Command parser, homing checks, FastAccelStepper calls |
| Servo (8) | 12 | Pulse calculation + PWM/PCA write setup |
| Buzzer (9) | 35 | Frequency change + duty update |
| DFPlayer (10) | 30 | Command check + UART command path |
| TM1637 (11) | 900 | 7-byte bit-bang transfer with explicit 5µs delays |
| 7-seg 7-pin (12) | 30 | Decode + 7 segment updates before I2C additions |
| 7-seg 8-pin (13) | 35 | Decode + 8 segment updates before I2C additions |
| DAC (14) | 10 | Internal DAC path before I2C additions |
| PWM DAC (15) | 6 | Calibration + 1 PWM/PCA update |
| Func Gen (16) | 120 + background timer cost | Waveform parameter update + esp_timer ISR reserve; consumes 1 timer-like slot |
| Solenoid (17) | 10 | Pulse state machine |
| Smoke Shooter (18) | 25 | Dual sequence state machine |
| **I2C write** | **+180 each** | Per active PCA/DAC/expander transaction at typical 400kHz bus speed |

ESP-NOW Master overhead (independent of output channels):
```
cpuUs = 500 + peerCount × ceil(512/chunkSize) × 170 + universeCount × 100
ramBytes = 512 + peerCount × (chunkSize + 44)
```

**LED strip frame skip behavior:** Runtime calls NeoPixelBus/RMT `CanShow()` before mapping pixels and calling `Show()`. If the previous WS281x transfer is still busy, that strip's update is skipped for the current frame tick and the next tick sends the newest DMX buffer. The CPU/service estimate intentionally counts only CPU prep/enqueue time, not the full WS281x wire time, because long LED strips are allowed to refresh below the configured global FPS instead of blocking other outputs.

### 3C. RamBudget — Runtime Memory Estimate

RAM estimates now include the `OutputChannel` vector slot, allocator/header slack, the per-channel DMX buffer allocated by firmware, and known runtime driver objects.

Base per channel:
- `224 bytes` for `OutputChannel` + vector/allocator slack
- DMX value buffer is now sized to the bytes each output actually reads
- DMX output still uses 512 bytes; LED strip DMX buffer = `ceil(led_count / pixels_per_universe) × 512`

| Type | RAM (bytes) | Notes |
| :--- | ---: | :--- |
| AC Dimmer (0) | 225 | base + 1 DMX byte |
| DMX (1) | 736 UART path; +20,632 if RMT fallback | base + DMX buffer; RMT fallback allocates encoded item buffer |
| Relay (2) | 225 | base + 1 DMX byte |
| RGB LED (3) | `224 + universes×512 + count×3 + 256` RGB, `224 + universes×512 + count×4 + 256` RGBW | output DMX buffer + NeoPixel pixel buffer + wrapper/object overhead |
| Single LED (4) | `224 + valueBytes` | 1-2 DMX bytes by resolution |
| Analog RGB (5) | 227 RGB / 228 RGBW | base + color DMX bytes |
| Motor (6) | `224 + valueBytes` | 1-2 DMX bytes by resolution |
| Stepper (7) | `224 + valueBytes + 2 + 512` | position bytes + speed + command + FastAccelStepper runtime estimate |
| Servo (8) | `224 + valueBytes` | 1-2 DMX bytes by resolution |
| Buzzer (9) | 226 | frequency + volume |
| DFPlayer (10) | 487 | base + 3 DMX bytes + DFPlayer object + command buffer |
| TM1637 (11) | 226 numeric / 228 ASCII | base + 2 or 4 DMX bytes |
| 7-seg 7-pin (12) | 225 direct / 226 dimmed | base + 1 or 2 DMX bytes; I2C routes add overhead |
| 7-seg 8-pin (13) | 225 direct / 226 dimmed | base + 1 or 2 DMX bytes; I2C routes add overhead |
| DAC (14) | 225 | base + 1 DMX byte; I2C DAC route adds overhead |
| PWM DAC (15) | `224 + valueBytes` | 1-2 DMX bytes by resolution |
| Func Gen (16) | 1349 | base + 5 DMX bytes + waveform tables/timer object estimate |
| Solenoid (17) | 225 | base + 1 DMX byte |
| Smoke Shooter (18) | 225 | base + 1 DMX byte |
| I2C route bookkeeping | +32 per active I2C write route | PCA/DAC/expander manager/device reference estimate |
| DMX RMT fallback | +20,632 per fallback DMX output | `5150 × sizeof(rmt_item32_t) + slack` |

ESP-NOW Master overhead (chunkSize = 200 by default):
```
ramBytes = 512 + peerCount × (chunkSize + 44)
```

**Limit:** `max(0, ESP.getFreeHeap() - 150KB)` capped at 64KB. Keeps 150KB free for system/network/runtime.
At typical 260KB free heap → `260 - 150 = 110KB`, capped to 64KB. Actual limit reported by `/api/scoring`.
Web UI uses a conservative 48KB as local pre-check.

### Validation

Hardware, CPU, and RAM are all checked independently. Hardware blocking is **source-aware**:
types using PCA9685 or digital expanders bypass the count because they don't consume
ESP32-native peripherals.

```cpp
// Hardware — every count ≤ max (source-aware: expander/PCA channels don't count)
bool hwOk = total.ledc ≤ 16 && total.rmt ≤ 8 && total.uart ≤ 2 && total.dac ≤ 2 && total.timer ≤ 4;

// CPU/service time — total µs ≤ (1,000,000 / fps) - 1,500
// Includes BASE_OVERHEAD_US (500 µs) for loop overhead
bool cpuOk = cpuUs ≤ CpuBudget::limit(fps);

// RAM — total bytes ≤ 65535
bool ramOk = ramBytes ≤ RamBudget::limit();

// First failure blocks the save
ScoreBlocker blocker = checkScores(hw, cpu, ram, fps);
```

This replaces the old single combined `SCORE_LIMIT` (109) approach.

## 4. WT32-ETH01 Physical Pin Allocations & Safety

The WT32-ETH01 is highly integrated. Many ESP32 GPIOs are wired internally to the LAN8720 Ethernet PHY chip. Using these pins for outputs can break networking or crash the system.

### Internal Reserved Pins

* **Ethernet RMII interface:** GPIO0 (REF_CLK), GPIO18 (MDIO), GPIO19 (TXD0), GPIO21 (TX_EN), GPIO22 (TXD1), GPIO23 (MDC), GPIO25 (RXD0), GPIO26 (RXD1), GPIO27 (CRS_DV)
  - **GPIO0 note:** This pin receives 50MHz RMII reference clock from LAN8720 AND is the BOOT button. Pressing BOOT during active Ethernet may distort the clock signal and cause link drop. Avoid pressing BOOT while Ethernet is active.
* **Ethernet PHY power control:** GPIO16 (ETH_PHY_POWER) — drives LAN8720 nRST/nEN; not a data signal
* **Triple-purpose GPIO5:** Status LED (default), UART2_RX (DMX RX), and LAN8720 nRST — all on the same pin. LED blink pattern serves as PHY reset indicator at boot.
* **Debug UART0:** GPIO1 (TX0), GPIO3 (RX0)
* **ESP32 DAC pins:** GPIO25/GPIO26 are occupied by Ethernet on WT32-ETH01, so MCP4725 is the practical DAC path.

### Exposed Header Pins

* **Common safe outputs:** GPIO2, GPIO4, GPIO14, GPIO15, GPIO17, GPIO32, GPIO33
* **Input-only pins:** GPIO34, GPIO35, GPIO36, GPIO39
* **Default status LED pin:** GPIO5 (see triple-purpose note above)
* **Default I2C pins:** GPIO14 SDA, GPIO15 SCL
* **Zero-crossing pin:** User-configured; input-only GPIO is recommended.
* **Forbidden pins:** GPIO12 (MTDI bootstrap pin — see ADR006); any use can cause boot loop.

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
* **MCP4725 (`source=5`) address range:** `0x60` or `0x61`.
* **DAC7571 (`source=6`) address range:** `0x4C` or `0x4D`.
* **DAC7573 (`source=7`) address range:** `0x4C..0x5B`; `pca_channel` selects channel A-D.

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
    * Long individual strips reduce refresh rate because WS281x timing is serialized; when the strip is still busy, runtime skips that frame rather than blocking Core 1.

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

## 8. Verification & Calibration

### 8.1 Runtime Benchmarking
Measure real-time indicators on physical boards to calibrate the µs estimates:
1. **Output Jitter & Frame Latency:** Load a configuration near the CPU budget limit at 40 FPS, send Art-Net packets, and capture DMX/LED output using an oscilloscope. If frame drops or jitter occur, reduce the budget safety margin or increase the per-type µs estimates.
2. **Heap Memory Audit:** Monitor free RAM (`ESP.getFreeHeap()`) under DMX load. If heap falls below 30-40 KB, reduce pixel counts or channel count.
3. **Task Execution Time:** Check if the output loop task (Core 1) triggers the FreeRTOS watchdog or experiences scheduling delays.

### 8.2 I2C Bus Contention
Since all PCA9685 and digital expanders share a single I2C bus, high FPS updates can saturate it:
- Measure I2C transaction duration for all expander updates inside the output loop.
- If I2C transactions take longer than 15ms (leaving <10ms for other processing at 40 FPS), increase per-channel I2C overhead µs.

### 8.3 Simulation & Mock Validation
Run the Python offline calculator `tools/load_calculator.py` against boundary configurations to verify interlock validation parity between C++ firmware and JavaScript Web UI.
