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

  let wire-color = rgb("#1a5fb4")
  let s-wire = (stroke: 1.2pt + wire-color)
  let s-comp = (stroke: 1.2pt + rgb("#333333"))

  // GPIO connection terminal pin
  circle((-5.5, 0.6), radius: 0.1, fill: rgb("#333333"), stroke: none)
  content((-5.5, 0.9), [#text(8pt, weight: "bold")[ESP32 GPIO]])

  // Resistor R1
  rect((-3.0, 0.8), (-1.8, 0.4), ..s-comp, name: "r1")
  content("r1", [#text(8pt)[220 Ω]])
  
  // PPTC Fuse
  rect((-0.5, 0.8), (0.7, 0.4), ..s-comp, name: "pptc")
  content("pptc", [#text(7pt)[PPTC]])
  content((0.1, 1.0), [#text(7pt)[< 100 mA]])

  // Output terminal pin
  circle((2.0, 0.6), radius: 0.1, fill: rgb("#333333"), stroke: none)
  content((2.0, 0.9), [#text(8pt, weight: "bold")[To Device]])

  // Zener Diode Symbol
  // Cathode bar
  line((-4.55, 0.0), (-3.95, 0.0), ..s-comp)
  // Zener bends
  line((-4.55, 0.0), (-4.55, -0.1), ..s-comp)
  line((-3.95, 0.0), (-3.95, 0.1), ..s-comp)
  // Triangle pointing down
  line((-4.55, 0.0), (-4.25, -0.4), ..s-comp)
  line((-3.95, 0.0), (-4.25, -0.4), ..s-comp)
  content((-3.0, -0.2), [#text(8pt)[3.3V Zener]])

  // GND Symbol
  line((-4.65, -0.9), (-3.85, -0.9), ..s-comp)
  line((-4.5, -1.05), (-4.0, -1.05), ..s-comp)
  line((-4.35, -1.2), (-4.15, -1.2), ..s-comp)
  content((-4.25, -1.5), [#text(7pt)[GND]])

  // Wires (connecting components)
  line((-5.4, 0.6), (-3.0, 0.6), ..s-wire) // GPIO to R1
  line((-1.8, 0.6), (-0.5, 0.6), ..s-wire) // R1 to PPTC
  line((0.7, 0.6), (1.9, 0.6), ..s-wire)  // PPTC to Out
  
  // Fuse line through rectangle
  line((-0.5, 0.6), (0.7, 0.6), ..s-wire)

  // Zener tap and GND connections
  line((-4.25, 0.6), (-4.25, 0.0), ..s-wire)
  line((-4.25, -0.4), (-4.25, -0.9), ..s-wire)
})
]
#v(0.8em)

=== Buffered Connection (5V)

When using a 5V buffer IC (e.g., 74HCT245), use a 5.1V Zener diode on the buffer output side.

#v(0.8em)
#align(center)[
#canvas(length: 1cm, {
  import draw: *

  let wire-color = rgb("#1a5fb4")
  let s-wire = (stroke: 1.2pt + wire-color)
  let s-comp = (stroke: 1.2pt + rgb("#333333"))

  // GPIO connection terminal pin
  circle((-6.5, 0.6), radius: 0.1, fill: rgb("#333333"), stroke: none)
  content((-6.5, 0.9), [#text(8pt, weight: "bold")[ESP32 GPIO]])

  // Buffer Gate Symbol (Triangle pointing right)
  line((-5.0, 1.1), (-5.0, 0.1), ..s-comp)
  line((-5.0, 1.1), (-3.8, 0.6), ..s-comp)
  line((-5.0, 0.1), (-3.8, 0.6), ..s-comp)
  content((-4.4, -0.2), [#text(7pt)[74HCT245]])

  // Zener Diode Symbol (5.1V)
  // Cathode bar
  line((-2.8, 0.0), (-2.2, 0.0), ..s-comp)
  // Zener bends
  line((-2.8, 0.0), (-2.8, -0.1), ..s-comp)
  line((-2.2, 0.0), (-2.2, 0.1), ..s-comp)
  // Triangle pointing down
  line((-2.8, 0.0), (-2.5, -0.4), ..s-comp)
  line((-2.2, 0.0), (-2.5, -0.4), ..s-comp)
  content((-1.2, -0.2), [#text(8pt)[5.1V Zener]])

  // GND Symbol
  line((-2.9, -0.9), (-2.1, -0.9), ..s-comp)
  line((-2.75, -1.05), (-2.25, -1.05), ..s-comp)
  line((-2.6, -1.2), (-2.4, -1.2), ..s-comp)
  content((-2.5, -1.5), [#text(7pt)[GND]])

  // Resistor R2
  rect((-1.2, 0.8), (0.0, 0.4), ..s-comp, name: "r2")
  content("r2", [#text(8pt)[220 Ω]])

  // PPTC Fuse 2
  rect((1.3, 0.8), (2.5, 0.4), ..s-comp, name: "pptc2")
  content("pptc2", [#text(7pt)[PPTC]])
  content((1.9, 1.0), [#text(7pt)[< 100 mA]])

  // Output terminal pin
  circle((3.8, 0.6), radius: 0.1, fill: rgb("#333333"), stroke: none)
  content((3.8, 0.9), [#text(8pt, weight: "bold")[To Device]])

  // Wires (connecting components)
  line((-6.4, 0.6), (-5.0, 0.6), ..s-wire) // GPIO to Buffer in
  line((-3.8, 0.6), (-1.2, 0.6), ..s-wire) // Buffer out to R2
  line((0.0, 0.6), (1.3, 0.6), ..s-wire)  // R2 to PPTC2
  line((2.5, 0.6), (3.7, 0.6), ..s-wire)  // PPTC2 to Out

  // Fuse line through rectangle
  line((1.3, 0.6), (2.5, 0.6), ..s-wire)

  // Zener tap and GND connections
  line((-2.5, 0.6), (-2.5, 0.0), ..s-wire)
  line((-2.5, -0.4), (-2.5, -0.9), ..s-wire)
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
  [Series Resistor], [47 Ω to 220 Ω (see below)], [Limit maximum current on signal line],
  [PPTC Resettable Fuse], [30 mA to 100 mA (see below)], [Resettable overcurrent protection on output line],
)
]

=== Component Selection for 12V/24V Fault Tolerance

If the board operates alongside 12VDC lighting or 24VDC motor power supplies, accidental short-circuits can inject these higher voltages back into the signal lines. To prevent damage, the series resistor ($R$) and PPTC fuse must be selected so that the fault current ($I_("fault")$) exceeds the PPTC trip current ($I_("trip")$) to trigger a shutdown, while limiting the transient power dissipation on the Zener diode.

The fault current is calculated as:
$ I_("fault") = (V_("fault") - V_("Zener")) / (R + R_("PPTC")) $

==== Recommended Configurations for 12V and 24V Coexistence

1. *50 mA Hold / 100 mA Trip PPTC (Recommended for general use & pixel strips):*
  - *Resistor Value ($R$):* *47 $Omega$ to 82 $Omega$* (Use a 0.5W or 1W pulse-withstanding resistor, e.g. size 1206 or 1210).
  - *At 12V Fault ($V_("fault") = 12"V"$):* $I_("fault") approx 82"mA" - 146"mA"$, which successfully trips the 100 mA fuse.
  - *At 24V Fault ($V_("fault") = 24"V"$):* $I_("fault") approx 220"mA" - 350"mA"$, causing an instantaneous trip.
  - *Note:* Low series resistance maintains high rise/fall times for WS2812B pixels (800 kHz) and DMX512 (250 kbps).

2. *30 mA Hold / 60 mA Trip PPTC (Recommended for slow signal lines & relays):*
  - *Resistor Value ($R$):* *100 $Omega$ to 120 $Omega$*.
  - *At 12V Fault:* $I_("fault") approx 60"mA" - 85"mA"$, which trips the 60 mA fuse.
  - *At 24V Fault:* $I_("fault") approx 150"mA" - 200"mA"$, causing a rapid trip.

3. *20 mA Hold / 40 mA Trip PPTC (For high-impedance inputs only):*
  - *Resistor Value ($R$):* *150 $Omega$ to 220 $Omega$*.
  - *Note:* Not recommended for high-speed pixel strips due to signal rounding from higher RC time constants.


== Inductive Load Protection (Flyback Diode)

Inductive loads (solenoid valves, relay coils, DC motors) generate a high reverse voltage spike (flyback voltage) when suddenly switched off, which can disrupt the radio system or damage the driving transistor.

*Mitigation:* Always connect a fast or general-purpose diode (e.g., 1N4007) in reverse-biased parallel across the inductive load to safely discharge the reverse voltage.

#v(0.8em)
#align(center)[
#canvas(length: 1cm, {
  import draw: *

  let wire-color = rgb("#1a5fb4")
  let s-wire = (stroke: 1.2pt + wire-color)
  let s-comp = (stroke: 1.2pt + rgb("#333333"))

  // Inductive Load (Represented as a solenoid/relay coil box)
  rect((1.0, 1.5), (2.5, 0.5), ..s-comp, name: "load")
  content("load", align(center)[#text(8pt)[*Inductive Load* \ Solenoid/Motor]])

  // Transistor NPN Symbol
  // Base lead
  line((0.8, -0.5), (1.3, -0.5), ..s-wire)
  // Base bar
  line((1.3, -0.1), (1.3, -0.9), ..(stroke: 1.8pt + rgb("#333333")))
  // Collector
  line((1.3, -0.3), (1.75, 0.0), ..s-comp)
  line((1.75, 0.0), (1.75, 0.5), ..s-wire)
  // Emitter
  line((1.3, -0.7), (1.75, -1.0), ..s-comp)
  line((1.75, -1.0), (1.75, -1.5), ..s-wire)
  // Emitter arrow pointing out
  line((1.5, -0.85), (1.7, -0.95), ..s-comp)
  line((1.65, -0.8), (1.7, -0.95), ..s-comp)
  
  content((0.3, -0.2), [#text(7pt)[Control Input]])

  // GND Symbol
  line((1.35, -1.8), (2.15, -1.8), ..s-comp)
  line((1.5, -1.95), (2.0, -1.95), ..s-comp)
  line((1.65, -2.1), (1.85, -2.1), ..s-comp)
  content((1.75, -2.4), [#text(7pt)[GND]])

  // Diode 1N4007 (pointing UP - reverse biased)
  // Cathode bar
  line((-0.8, 1.2), (-0.2, 1.2), ..s-comp)
  // Triangle pointing UP
  line((-0.8, 0.8), (-0.2, 0.8), ..s-comp)
  line((-0.8, 0.8), (-0.5, 1.2), ..s-comp)
  line((-0.2, 0.8), (-0.5, 1.2), ..s-comp)
  content((-0.5, 0.4), [#text(8pt)[1N4007]])
  content((-1.4, 1.0), [#text(7pt, fill: gray)[reverse \ biased]])

  // Positive Supply Rail (Node A)
  line((-0.5, 2.0), (1.75, 2.0), ..s-wire)
  line((1.75, 2.0), (1.75, 1.5), ..s-wire) // VCC to Load
  line((-0.5, 2.0), (-0.5, 1.2), ..s-wire) // VCC to Cathode
  content((1.75, 2.3), [#text(8pt)[+VCC]])

  // Switching Node (Node B)
  // Diode anode to Collector node
  line((-0.5, 0.8), (-0.5, 0.25), ..s-wire)
  line((-0.5, 0.25), (1.75, 0.25), ..s-wire)
  // Load bottom to Collector node
  line((1.75, 0.5), (1.75, 0.0), ..s-wire)
  
  // Ground Node connection
  line((1.75, -1.5), (1.75, -1.8), ..s-wire)
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
