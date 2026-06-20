# WT32-ETH01 / ESP32 Hardware Resource Calculator & Constraints Manual

This document details how hardware resources are allocated on the WT32-ETH01 controller across different output modes, mapping rules for I2C expanders, and guidelines to avoid hardware overloads.

---

## 1. ESP32 Peripheral Limits

The ESP32 microcontroller inside the WT32-ETH01 provides several specialized hardware peripherals. These are shared among all configured output types:

| Peripheral | Max Capacity | Usage Rules | Affected Output Types |
| :--- | :--- | :--- | :--- |
| **RMT Channels** | 8 channels | 1 RMT channel per LED strip. 1 RMT channel per software-based DMX serial output. | LED Strip (NeoPixel), DMX Serial (extra pins) |
| **LEDC PWM** | 16 channels | Channels 0-15. Used to generate PWM signals for dimmers, servos, and H-Bridge motor speed. | DC PWM Dimmer, RC Servo, DC Motor (all modes) |
| **UART Ports** | 2 DMX ports | UART0 is reserved for USB debug/serial. UART1 and UART2 are available as hardware DMX. | DMX Serial (first 2 instances) |
| **Active Channels**| 8 Slots | Max 8 total rows in `/outputs.json` due to firmware capacity checks. | All Output Types |
| **GPIO Outputs** | ~8 Pins | Physical header limitation of WT32-ETH01 (most pins reserved internally). | All direct-connected pins |

---

## 2. Resource Footprint by Output Mode

The resource footprint varies heavily depending on whether a mode uses ESP32 GPIO directly or routes through an expander:

| Output Type | ESP32 GPIO Count | RMT Count | LEDC Count | UART Count | Expander Compatible? |
| :--- | :---: | :---: | :---: | :---: | :--- |
| **LED Strip** | 1 pin | 1 channel | 0 | 0 | ❌ No (requires RMT precision) |
| **DMX Serial** | 1 pin | 1 (if >2 outputs)| 0 | 1 (if <=2 outputs)| ❌ No (requires UART/RMT timing) |
| **Relay** | 1 pin | 0 | 0 | 0 | **Yes** (PCA9685/MCP23017/TCA9555/PCF8574) |
| **Solenoid Trigger** | 1 pin | 0 | 0 | 0 | **Yes** (PCA9685/MCP23017/TCA9555/TPIC6B595) |
| **AC Dimmer** | 1 pin (+1 shared ZC) | 0 | 0 | 0 | ❌ No (requires Zero-Crossing ISR sync) |
| **DC PWM Dimmer** | 1 pin | 0 | 1 | 0 | **Yes** (PCA9685/TLC5947) |
| **RC Servo** | 1 pin | 0 | 1 | 0 | **Yes** (PCA9685) |
| **DC Motor (Open-Loop PWM+PWM)**| 2 pins | 0 | 2 | 0 | **Yes** (PCA9685) |
| **DC Motor (Open-Loop PWM+DIR)**| 2 pins | 0 | 1 | 0 | **Yes** (PCA9685) |
| **DC Motor (Open-Loop IN1+IN2+EN)**| 3 pins| 0 | 1 | 0 | **Yes** (PCA9685) |
| **~~DC Motor Closed-Loop (A/B Hall)~~** | ~~2 out + 2 in (A/B phase)~~ | ~~0~~ | ~~0-2~~ | ~~0~~ | ~~**Yes** (Outputs on PCA9685; inputs must be direct ESP32 GPIO for PCNT)~~ *(Canceled)* |
| **~~DC Motor Closed-Loop (AS5600 I2C)~~** | ~~2 out + 0 extra in (I2C)~~ | ~~0~~ | ~~0-2~~ | ~~0~~ | ~~**Yes** (Outputs on PCA9685; inputs share I2C bus `0x36`, no extra pins)~~ *(Canceled)* |
| **~~DC Motor Closed-Loop (PWM Enc.)~~** | ~~2 out + 1 in (PWM feedback)~~ | ~~0~~ | ~~0-2~~ | ~~0~~ | ~~**Yes** (Outputs on PCA9685; input must be direct ESP32 GPIO for capture)~~ *(Canceled)* |
| **Stepper Motor** | 1 to 4 pins | 0 | 0 | 0 | **Partially** (STEP pin must be direct ESP32 GPIO; DIR/ENABLE can use expander) |
| **WiZ Smart Bulb** | 0 | 0 | 0 | 0 | ❌ N/A (Deprecated/Removed -> Move to PC/host control instead) |
| **Sequential Smoke Shooter** | 2 pins | 0 | 0 | 0 | **Yes** (PCA9685/MCP23017/TCA9555/PCF8574/TPIC6B595) |
| **7-Segment (Driver Chip)** | 2 to 3 pins | 0 | 0 | 0 | **Yes** (TM1637 uses 2 pins; MAX7219 uses 3 pins) |
| **7-Segment Direct Drive** | 7 to 8 pins | 0 | 0 | 0 | **Yes** (Converts DMX ASCII. Strongly recommended to map to I2C expander to save pins) |
| **Analog RGB** | 3 pins | 0 | 3 | 0 | **Yes** (PCA9685/TLC5947) |
| **Analog RGBW** | 4 pins | 0 | 4 | 0 | **Yes** (PCA9685/TLC5947) |
| **Passive Buzzer** | 1 pin | 0 | 1 | 0 | ❌ No (Not recommended due to shared PCA9685 frequencies) |
| **ToF Sensor (VL53L0X)** | 2 shared pins (I2C) | 0 | 0 | 0 | **Yes** (Runs on shared I2C bus at address `0x29`) |
| **Ultrasonic (HC-SR04 / A02YYUW)** | 1 out + 1 in (HC-SR04) or 1 in UART RX (A02YYUW) | 0 | 0 | 0 | **Yes** (HC-SR04 Trig can use expander; A02YYUW uses direct ESP32 GPIO mapped as UART RX) |
| **Analog Microphone** | 1 analog pin (in) | 0 | 0 | 0 | ❌ No (Requires direct ESP32 ADC pin) |

---

## 3. WT32-ETH01 Physical Pin Allocations & Safety

The WT32-ETH01 is highly integrated. Many ESP32 GPIOs are wired internally to the LAN8720 Ethernet PHY chip. Using these pins for outputs will crash the system.

### Internal Reserved Pins (DO NOT USE)
* **Ethernet RMII Interface:** GPIO0, GPIO16, GPIO18, GPIO19, GPIO21, GPIO22, GPIO23, GPIO25, GPIO26, GPIO27
* **Debug UART0:** GPIO1 (TX0), GPIO3 (RX0)
* **Oscillator / XTAL:** GPIO32, GPIO33 (depending on board revision, often reserved for 50MHz PHY clock input)

### Exposed Header Pins
* **Safe Outputs:** GPIO2, GPIO4 (default LED pin), GPIO12, GPIO14, GPIO15, GPIO17 (default DMX TX pin)
* **Safe Inputs Only:** GPIO34, GPIO35, GPIO36, GPIO39 (no internal pull-ups, input-only). 
  * *Tip:* These input-only pins are ideal for sensor inputs:
    * **Ultrasonic Echo pin** (e.g. GPIO34)
    * **Analog Microphone** (e.g. GPIO36 or GPIO39 which are ADC1 channels)
    * **PIR/Laser gate inputs** (e.g. GPIO35)
* **Status LED Pin:** GPIO5 (can be disabled or moved in SystemConfig)
* **Zero-Crossing Pin:** User-configured (recommended to use GPIO35 or another input-only pin)

> [!IMPORTANT]
> Because only **6 to 8 pins** are available for general outputs on the WT32-ETH01 headers, using I2C expander chips is **strongly recommended** for any system requiring more than a couple of actuators or channels.

---

## 4. I2C Expander Device Specifications

I2C expanders communicate with the ESP32 using just 2 pins (SDA and SCL) but map to dozens of external channels.

### 4.1 PCA9685 PWM/Servo Expander (16 Channels)
* **Ideal for:** Servos, PWM dimmers, on-off relay drivers, DC motor signals.
* **Resolution:** 12-bit (0-4095).
* **Limitation:** All 16 channels on the same board **must share the same PWM frequency** (24 Hz to 1526 Hz).
  * *Servo frequency:* 50 Hz.
  * *DC Motor/LED frequency:* 200 Hz to 1000+ Hz.
  * *Optimization rule:* Do not mix Servos (50Hz) and LED dimmers (1000Hz) on the same chip. Use address `0x40` for 50Hz servos and address `0x41` for 1000Hz PWM.

### 4.2 MCP23017 / TCA9555 Digital GPIO Expander (16 Channels)
* **Ideal for:** Relays, solenoids, smoke sequences, digital input sensors.
* **Resolution:** On/Off only (no PWM).
* **Optimization rule:** Highly efficient for multiple relays since they consume 0 LEDC/RMT resources on the main chip.

### 4.3 PCF8574 Low-Cost Expander (8 Channels)
* **Ideal for:** Simple relay modules.
* **Limitation:** Quasi-bidirectional pins have weak pull-ups. Active-low switching is recommended.

### 4.4 TPIC6B595 Power Sink Shift Register (8 Channels)
* **Ideal for:** Heavy solenoid or inductive relay banks.
* **Limitation:** Not I2C (uses SPI/shift register pins). Can be daisy-chained. Acts as a low-side driver.

### 4.5 TLC5947 Constant-Current LED Driver (24 Channels)
* **Ideal for:** Driving multiple small, direct-wired LEDs (e.g. control panel indicators).
* **Limitation:** Current sink only.

---

## 5. Capacity & Network Load Guidelines

1. **Total LED Limit (RAM/Watchdog):**
   * Keep total NeoPixels under **4,000 pixels** to prevent memory starvation.
   * Long individual strips (e.g. >1000 LEDs) will drop the refresh rate:
     $$\text{Max FPS} \approx \frac{1000}{\text{LED count} \times 0.030\text{ ms}}$$
     * 170 LEDs $\approx$ 196 FPS
     * 500 LEDs $\approx$ 66 FPS
     * 1000 LEDs $\approx$ 33 FPS
     * 1500 LEDs $\approx$ 22 FPS (stuttering visible)

2. **WiZ Bulb (Deprecated/Removed):**
   * WiZ bulb control is deprecated and marked for deletion. It is recommended to handle all smart bulb controls on a host computer (Node-RED, QLC+, Home Assistant) rather than on the micro-controller.

3. **Bluetooth/BLE (Excluded):**
   * Bluetooth/BLE has been officially excluded from this project due to excessive flash/RAM consumption which interferes with OTA and Ethernet stability. Do not enable Bluetooth.

3. **7-Segment Display limits:**
   * **TM1637:** Pre-made modules are extremely light. Up to 4 displays can easily run on a single ESP32 without polling lag (takes 2 GPIOs each).
   * **MAX7219:** Daisy-chained SPI modules. Up to 8 modules (64 digits total) can share 1 SPI bus (3 GPIO pins total).

4. **Mixed-Mode Optimization (Pin Saving):**
   * High-speed pulse pins (like **STEP** for Steppers or **CLK** for fast SPI) must stay on direct ESP32 GPIOs.
   * Slow state-change pins (like **DIR** and **ENABLE** for Steppers, or **Latch/CS** for displays) can be offloaded to an I2C GPIO Expander (like MCP23017). This reduces physical ESP32 GPIO usage of a Stepper from 3 or 4 pins down to just **1 pin**.

5. **Power Budget Planning:**
   * **Do not drive relays, servos, motors, or solenoids from the ESP32 5V/3.3V pins.**
   * Separate logic power from load power. Connect all Ground pins together.
   * Calculate total current:
     * *Standard NeoPixel (White full brightness):* 60mA per pixel.
     * *170 pixels (1 Universe) full white:* 10.2A @ 5V.
     * *Servo motor (moving under load):* 500mA - 1.5A per servo.
