= Outputs \& Capacity Planning

The node allocates outputs dynamically according to a hardware budget scoring algorithm. Output channels map DMX data to physical pins using multiple sources.

== Output Concepts

Each output channel is defined by a start universe, start DMX address, type (0--18), source (GPIO, PCA9685, digital expander, I2C DAC), and parameters.

#v(1em)
#import "@preview/cetz:0.5.2": canvas, draw

#align(center)[
#canvas(length: 1cm, {
  import draw: *
  
  // Styles
  let block-style = (fill: rgb("#e1f5fe"), stroke: 1pt + rgb("#0288d1"), radius: 0.1)
  let ch-style = (fill: rgb("#fff9c4"), stroke: 1pt + rgb("#fbc02d"), radius: 0.1)
  let hw-style = (fill: rgb("#e8f5e9"), stroke: 1pt + rgb("#388e3c"), radius: 0.1)
  
  // DMX Buffer
  rect((-6, 2), (6, 1.2), ..block-style, name: "dmx")
  content("dmx", [*DMX Universe Packet (Channels 1 - 512)*])
  
  // Channels
  rect((-5, -0.2), (-2.8, -1.2), ..ch-style, name: "ch1")
  content("ch1", align(center)[#text(8.5pt)[Ch N:\ Position MSB]])
  
  rect((-2.4, -0.2), (-0.2, -1.2), ..ch-style, name: "ch2")
  content("ch2", align(center)[#text(8.5pt)[Ch N+1:\ Position LSB]])
  
  rect((0.2, -0.2), (2.4, -1.2), ..ch-style, name: "ch3")
  content("ch3", align(center)[#text(8.5pt)[Ch N+2:\ Speed]])
  
  rect((2.8, -0.2), (5, -1.2), ..ch-style, name: "ch4")
  content("ch4", align(center)[#text(8.5pt)[Ch N+3:\ Command]])
  
  // Stepper Controller
  rect((-3.5, -2.5), (3.5, -3.5), ..hw-style, name: "stepper")
  content("stepper", [*Core 1 Stepper Task (Pins: STEP, DIR, EN)*])
  
  // Connect DMX to channels
  line((-3.9, 1.2), "ch1.north", mark: (end: "stealth"))
  line((-1.3, 1.2), "ch2.north", mark: (end: "stealth"))
  line((1.3, 1.2), "ch3.north", mark: (end: "stealth"))
  line((3.9, 1.2), "ch4.north", mark: (end: "stealth"))
  
  // Connect channels to stepper
  line("ch1.south", (-2, -2.5), mark: (end: "stealth"))
  line("ch2.south", (-0.7, -2.5), mark: (end: "stealth"))
  line("ch3.south", (0.7, -2.5), mark: (end: "stealth"))
  line("ch4.south", (2, -2.5), mark: (end: "stealth"))
})
]
#v(1em)

== Resource \& Compute Scoring System

To guarantee dual-core task stability under heavy configurations, the firmware uses a combined resource and compute score limit.
The total budget is capped at *109 points*.

$ "Total Score" = "Resource Score" + "Compute Score" <= 109 $

The resource score reflects hardware peripherals used, and the compute score reflects CPU ticks based on active output channels and FPS.

=== Resource Weight Multipliers

#align(center)[
#table(
  columns: (2.5fr, 1fr, 4.5fr),
  inset: 8pt,
  align: (left, center, left),
  stroke: 0.5pt + gray,
  [*Peripheral*], [*Weight*], [*Usage Detail*],
  [ESP32 GPIO Pin], [0.5], [Standard digital input or output pin],
  [LEDC (PWM Channel)], [2.5], [Direct hardware PWM channel (max 16)],
  [RMT Channel], [3.0], [RGB LED strips or DMX RMT fallbacks (max 8)],
  [UART Port], [8.0], [Serial DMX output or DFPlayer MP3 module (max 2)],
  [DAC Channel], [2.0], [Internal ESP32 DAC (unavailable on WT32-ETH01)],
  [PCA9685 Channel], [0.25], [I2C-based PWM expander channel],
  [I2C Expander Pin], [0.125], [MCP23017, TCA9555, or PCF857x expander pin],
)
]

== Output Type Reference

#align(center)[
#table(
  columns: (0.5fr, 2.5fr, 1fr, 2fr, 2fr),
  inset: 6pt,
  align: (center, left, center, left, left),
  stroke: 0.5pt + gray,
  [*ID*], [*Type Name*], [*Bytes*], [*Compatible Sources*], [*Primary Resource*],
  [0], [AC Dimmer], [1--2], [GPIO Only], [GPIO 1 + ZC Timer],
  [1], [DMX Serial Output], [512], [GPIO Only (UART/RMT)], [UART (fallback RMT)],
  [2], [Relay (On/Off)], [1], [GPIO, PCA, Expander], [GPIO / PCA / Exp 1],
  [3], [RGB/RGBW LED Strip], [Var], [GPIO Only (RMT)], [RMT 1 ch],
  [4], [Single Color LED], [1--2], [GPIO, PCA], [LEDC 1 / PCA 1],
  [5], [Analog RGB/RGBW], [3--4], [GPIO, PCA (Hybrid)], [LEDC 3-4 / PCA 3-4],
  [6], [DC Motor (H-Bridge)], [1--2], [GPIO, PCA (Hybrid)], [LEDC 1-3 / PCA 1-3],
  [7], [Stepper Motor], [3--5], [STEP:GPIO; DIR:Any], [FastAccelStepper],
  [8], [RC Servo], [1], [GPIO, PCA], [LEDC 1 (50Hz) / PCA 1],
  [9], [Passive Buzzer], [2], [GPIO Only], [LEDC 1],
  [10], [DFPlayer MP3 Module], [3], [GPIO Only (UART)], [UART 1],
  [11], [7-Segment (TM1637)], [2--4], [GPIO Only], [GPIO 2],
  [12], [7-Segment DD 7-Pin], [1--2], [GPIO, PCA, Expander], [Seg-level routing],
  [13], [7-Segment DD 8-Pin], [1--2], [GPIO, PCA, Expander], [Seg-level routing],
  [14], [DAC (Analog Out)], [1], [MCP4725, DAC757x], [I2C Bus],
  [15], [PWM DAC (RC Filter)], [1], [GPIO, PCA], [LEDC 1 / PCA 1],
  [16], [Function Generator], [5], [GPIO Only], [LEDC 1 + timer],
  [17], [Solenoid Trigger], [1], [GPIO, PCA, Expander], [GPIO / PCA / Exp 1],
  [18], [Smoke Shooter], [1], [GPIO, PCA, Expander], [GPIO / PCA / Exp 2],
)
]

== Pin Interlocks

To maintain hardware safety, configuration rules enforce the following constraints:
- *No GPIO Duplication:* An ESP32 GPIO pin cannot be used by more than one output channel or system setting simultaneously.
- *System Reservation:* GPIO pins configured as Status LED, Zero-Crossing Input, or I2C SDA/SCL are blocked from output selection.
- *PCA9685 Shared Frequency:* All channels on a single PCA9685 chip share one output frequency. Selecting an RC Servo (Type 8) on a PCA9685 chip automatically forces all channels on that chip to 50 Hz.
- *Stepper STEP Pin:* The STEP signal for Type 7 (Stepper Motor) must use a hardware-capable ESP32 GPIO pin due to high frequency toggle requirements. Enable, DIR, and Limit pins can route to digital expanders.
- *Motor EN Pin:* DC Motors operating in Mode 2 (IN1+IN2+EN) require speed modulation on the EN pin. Therefore, the EN pin must route to an ESP32 GPIO or a PCA9685, never a digital expander.

== PWM DAC Duty Calibration

For Type 15 (PWM DAC) outputs, you can map the digital 0--255 DMX range to specific hardware duty cycles. This is configured via:
- *Mode:* Custom, 0--10V output converter, or 4--20mA transmitter.
- *Min Duty:* Duty cycle percentage multiplied by 100 (e.g. 500 = 5.0%) mapped to DMX 0.
- *Max Duty:* Duty cycle percentage multiplied by 100 (e.g. 9500 = 95.0%) mapped to DMX 255.
Linear interpolation mapping runs inside Core 1's update loop to set the LEDC duty register correctly.
