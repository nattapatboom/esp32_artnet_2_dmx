#import "@preview/cetz:0.5.2": canvas, draw

= Hardware Protection & Verification

When the board drives various loads for lighting and effect control in the field, current surges or noise feedback can cause brownout resets or port damage. This chapter documents the recommended protection circuits and device verification guidelines.

== Output Port ESD & Overcurrent Protection

=== Direct GPIO Connection (3.3V)

Use a 3.3V Zener diode to prevent the ESP32 signal pin voltage from exceeding the 3.3V rating when driving a load directly.

#v(0.8em)
#align(center)[
#canvas(length: 1cm, {
  import draw: *

  let box = (fill: rgb("#e1f5fe"), stroke: 1pt + rgb("#0288d1"))
  let zener = (fill: rgb("#fff9c4"), stroke: 1pt + rgb("#fbc02d"))
  let pass = (fill: rgb("#e8f5e9"), stroke: 1pt + rgb("#388e3c"))
  let gnd = (fill: rgb("#ffebee"), stroke: 1pt + rgb("#d32f2f"))

  rect((-6, 1), (-3.2, 0.2), ..box, name: "gpio")
  content("gpio", align(center)[#text(8pt)[*ESP32 GPIO*]])

  rect((-2.8, 1), (-0.5, 0.2), ..pass, name: "r1")
  content("r1", align(center)[#text(8pt)[220 Ω]])

  rect((0, 1), (2.3, 0.2), ..pass, name: "pptc")
  content("pptc", align(center)[#text(8pt)[PPTC < 100 mA]])

  rect((2.8, 1), (5.5, 0.2), ..box, name: "out")
  content("out", align(center)[#text(8pt)[*To Device*]])

  rect((-4.1, -0.3), (-1.9, -1.1), ..zener, name: "zen")
  content("zen", align(center)[#text(8pt)[3.3V Zener]])

  rect((-4.0, -1.5), (-2.0, -2.3), ..gnd, name: "gnd1")
  content("gnd1", align(center)[#text(8pt)[GND]])

  line((-3.2, 0.6), (-2.8, 0.6), mark: (end: "stealth"))
  line((-0.5, 0.6), (0, 0.6), mark: (end: "stealth"))
  line((2.3, 0.6), (2.8, 0.6), mark: (end: "stealth"))
  line((-3.0, 0.6), (-3.0, -0.3), mark: (end: "stealth"))
  line((-3.0, -1.1), (-3.0, -1.5), mark: (end: "stealth"))
})
]
#v(0.8em)

=== Buffered Connection (5V)

When using a 5V buffer IC (e.g., 74HCT245), use a 5.1V Zener diode on the buffer output side.

#v(0.8em)
#align(center)[
#canvas(length: 1cm, {
  import draw: *

  let box = (fill: rgb("#e1f5fe"), stroke: 1pt + rgb("#0288d1"))
  let zener = (fill: rgb("#fff9c4"), stroke: 1pt + rgb("#fbc02d"))
  let pass = (fill: rgb("#e8f5e9"), stroke: 1pt + rgb("#388e3c"))
  let gnd = (fill: rgb("#ffebee"), stroke: 1pt + rgb("#d32f2f"))
  let buf = (fill: rgb("#f3e5f5"), stroke: 1pt + rgb("#7b1fa2"))

  rect((-7, 1.5), (-4.2, 0.5), ..box, name: "gpio2")
  content("gpio2", align(center)[#text(8pt)[*ESP32 GPIO*]])

  rect((-3.5, 1.5), (-0.5, 0.5), ..buf, name: "buf")
  content("buf", align(center)[#text(8pt)[5V Buffer \ 74HCT245]])

  rect((0, 1.5), (2.3, 0.5), ..pass, name: "r2")
  content("r2", align(center)[#text(8pt)[220 Ω]])

  rect((2.8, 1.5), (5.1, 0.5), ..pass, name: "pptc2")
  content("pptc2", align(center)[#text(8pt)[PPTC < 100 mA]])

  rect((5.6, 1.5), (8.3, 0.5), ..box, name: "out2")
  content("out2", align(center)[#text(8pt)[*To Device*]])

  rect((-1.35, -0.3), (0.85, -1.1), ..zener, name: "zen2")
  content("zen2", align(center)[#text(8pt)[5.1V Zener]])

  rect((-1.25, -1.5), (0.75, -2.3), ..gnd, name: "gnd2")
  content("gnd2", align(center)[#text(8pt)[GND]])

  line((-4.2, 1), (-3.5, 1), mark: (end: "stealth"))
  line((-0.5, 1), (0, 1), mark: (end: "stealth"))
  line((2.3, 1), (2.8, 1), mark: (end: "stealth"))
  line((5.1, 1), (5.6, 1), mark: (end: "stealth"))
  line((-0.25, 1.0), (-0.25, -0.3), mark: (end: "stealth"))
  line((-0.25, -1.1), (-0.25, -1.5), mark: (end: "stealth"))
})
]
#v(0.8em)

=== Component Details

#align(center)[
#table(
  columns: (2.5fr, 1.5fr, 4fr),
  inset: 8pt,
  align: (left, center, left),
  stroke: 0.5pt + gray,
  [*Component*], [*Value*], [*Purpose*],
  [Zener Diode (ESD Clamp)], [3.3V (GPIO) / 5.1V (Buffered)], [Clamp excess voltage; protect against ESD or reverse voltage],
  [Series Resistor], [220 Ω], [Limit maximum current on signal line],
  [PPTC Resettable Fuse], [< 100 mA], [Resettable overcurrent protection on output line],
)
]

== Inductive Load Protection (Flyback Diode)

Inductive loads (solenoid valves, relay coils, DC motors) generate a high reverse voltage spike (flyback voltage) when suddenly switched off, which can disrupt the radio system or damage the driving transistor.

*Mitigation:* Always connect a fast or general-purpose diode (e.g., 1N4007) in reverse-biased parallel across the inductive load to safely discharge the reverse voltage.

#v(0.8em)
#align(center)[
#canvas(length: 1cm, {
  import draw: *

  let diode = (fill: rgb("#fff9c4"), stroke: 1pt + rgb("#fbc02d"))
  let load = (fill: rgb("#e1f5fe"), stroke: 1pt + rgb("#0288d1"))
  let driver = (fill: rgb("#e8f5e9"), stroke: 1pt + rgb("#388e3c"))
  let gnd = (fill: rgb("#ffebee"), stroke: 1pt + rgb("#d32f2f"))

  rect((-1.5, 1.5), (0.5, 0.3), ..diode, name: "fd")
  content("fd", align(center)[#text(7pt)[1N4007]])

  rect((1, 2), (4.5, 1), ..load, name: "load")
  content("load", align(center)[#text(8pt)[*Inductive Load* \ Solenoid / Relay / Motor]])

  rect((1, 0), (4.5, -0.8), ..driver, name: "drv")
  content("drv", align(center)[#text(8pt)[Switching Transistor]])

  rect((1.5, -1.2), (4, -2), ..gnd, name: "gnd3")
  content("gnd3", align(center)[#text(8pt)[GND]])

  // Positive Supply Rail (Node A)
  line((-0.5, 2.5), (2.75, 2.5))
  line((2.75, 2.5), (2.75, 2.0), mark: (end: "stealth"))
  line((-0.5, 2.5), (-0.5, 1.5), mark: (end: "stealth"))
  content((2.75, 2.7), [#text(8pt)[+VCC]])

  // Switching Node (Node B)
  line((2.75, 1.0), (2.75, 0.0), mark: (end: "stealth"))
  line((-0.5, 0.3), (-0.5, 0.15))
  line((-0.5, 0.15), (2.75, 0.15), mark: (end: "stealth"))

  // Ground Node
  line((2.75, -0.8), (2.75, -1.2), mark: (end: "stealth"))

  content((-2.3, 0.9), text(7pt, fill: gray)[reverse biased])
})
]
#v(0.8em)

== Device Reference

All external peripheral devices must have reference datasheets for driver code verification.

#align(center)[
#table(
  columns: (0.5fr, 2.5fr, 1fr, 3.5fr),
  inset: 6pt,
  align: (center, left, center, left),
  stroke: 0.5pt + gray,
  [#"\#"], [*Device*], [*Bus*], [*Purpose / Verify*],
  [1], [PCA9685], [I2C], [16-channel PWM controller; register map and frequency limits],
  [2], [MCP23017 / TCA9555], [I2C], [16-bit I/O expander; IODIR and output bitmap writes],
  [3], [PCF8574], [I2C], [8-bit I/O expander; quasi-bidirectional I/O and active-low behavior],
  [4], [MCP4725 / DAC7571 / DAC7573], [I2C], [12-bit DACs; byte-level commands and I2C address config],
  [5], [TM1637], [GPIO], [7-segment LED driver; CLK/DIO timing constraints],
  [6], [DFPlayer Mini / YX5200], [UART], [Audio player; 9600 bps command packet format],
  [7], [LAN8720A], [RMII], [Ethernet PHY; bootstrap pins and 50 MHz RMII clock GPIO impact],
  [8], [SSD1306 / SH1106], [I2C], [OLED display; I2C address and initialization sequence],
)
]

=== I2C Bus Speed Compatibility

#align(center)[
#table(
  columns: (2.5fr, 2.5fr),
  inset: 8pt,
  align: (left, center),
  stroke: 0.5pt + gray,
  [*Device*], [*Max I2C Speed*],
  [PCA9685], [1 MHz (Fast-mode Plus)],
  [MCP23017], [400 kHz (Fast-mode)],
  [PCF8574], [100 kHz (Standard-mode)],
)
]

*Runtime Rule:* The overall bus speed must not exceed the maximum supported by the slowest device on the bus, to avoid bus lockup or data corruption.

=== Logic Level Tolerances

- ESP32 operates at 3.3V logic levels; many relay modules and output drivers operate at 5V.
- Verify from the datasheet whether the 5V device accepts a 3.3V input signal as logic HIGH ($V_("IH") <= 2.0 "V"$), or whether a logic level shifter is required.

=== Timing & Start-up Delays

- Check the power-on reset / boot time for each IC.
- Some expander chips require a settling delay before accepting the first command, to prevent startup configuration errors.
