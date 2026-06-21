= Quick Start

To begin using the WT32-ETH01 DMX Lighting Node, follow these steps:

1. *Power the Node:* Connect a stable 5V power supply to the WT32-ETH01 board. Ensure the supply can deliver sufficient current for the ESP32, Ethernet transceiver, and any connected outputs.
2. *Initial Setup AP:* If Ethernet is disconnected and Wi-Fi client mode is not yet configured, the board automatically starts a configuration Access Point named `ESP32-ArtNet-Setup-XXXX`.
3. *Access Configuration:* Connect your PC or mobile device to the setup AP and navigate to #link("http://192.168.4.1")[http://192.168.4.1] in a web browser.
4. *Configure Settings:* Navigate through the Network, Outputs, and Protocol tabs to define your system parameters.
5. *Save and Apply:* Click *Save*. The board will validate the settings against hardware constraints and reboot.

Ethernet is strongly recommended for live Art-Net/sACN streaming. Wi-Fi client mode is useful for setup or light-duty/non-critical streaming.

== System Architecture

The system architecture utilizes the ESP32's dual-core processor to separate high-priority output timing from network protocol processing. The diagram below illustrates this runtime task split and signal flow.

#v(1em)
#import "@preview/cetz:0.5.2": canvas, draw

#align(center)[
#canvas(length: 1cm, {
  import draw: *
  
  // Style definitions
  let block-style = (fill: rgb("#e1f5fe"), stroke: 1pt + rgb("#0288d1"), radius: 0.1)
  let core-style = (fill: rgb("#fff3e0"), stroke: (dash: "dashed", paint: rgb("#f57c00"), thickness: 1.5pt), radius: 0.1)
  let hw-style = (fill: rgb("#e8f5e9"), stroke: 1pt + rgb("#388e3c"), radius: 0.1)
  
  // Inputs
  rect((-6, 1.3), (-2, 0.1), ..block-style, name: "net")
  content("net", align(center)[#text(9pt)[*Art-Net / sACN* \ (Ethernet / Wi-Fi)]])
  
  rect((-6, -0.6), (-2, -1.8), ..block-style, name: "espnow")
  content("espnow", align(center)[#text(9pt)[*ESP-NOW DMX* \ (Wireless Bridge)]])
  
  // Core 0
  rect((-1, 2.5), (3.5, -2.5), ..core-style, name: "core0")
  content("core0.north", anchor: "north", y: -0.2)[#text(10pt)[*Core 0 (Network Task)*]]
  content((1.25, -0.1), align(left)[#text(8.5pt)[
    - UDP Listeners \
    - Web Server / API \
    - mDNS / Fallbacks \
    - OLED/LCD Display
  ]])
  
  // Core 1
  rect((5, 2.5), (9.5, -2.5), ..core-style, name: "core1")
  content("core1.north", anchor: "north", y: -0.2)[#text(10pt)[*Core 1 (Output Task)*]]
  content((7.25, -0.1), align(left)[#text(8.5pt)[
    - Frame Sync \& Queue \
    - LED RMT / UART DMX \
    - Motion / Stepper \
    - Interlocks
  ]])
  
  // Outputs
  rect((10.5, 2.2), (14.5, 1.0), ..hw-style, name: "gpios")
  content("gpios", align(center)[#text(9pt)[*GPIO / LEDC / RMT* \ (LEDs, Servos, Stepper)]])
  
  rect((10.5, 0.6), (14.5, -0.6), ..hw-style, name: "uarts")
  content("uarts", align(center)[#text(9pt)[*UART1 / UART2* \ (DMX, DFPlayer)]])
  
  rect((10.5, -1.0), (14.5, -2.2), ..hw-style, name: "i2c")
  content("i2c", align(center)[#text(9pt)[*I2C Bus (Wire)* \ (PCA9685, Expanders)]])
  
  // Arrows
  line("net.east", (-1, 0.7), mark: (end: "stealth"))
  line("espnow.east", (-1, -1.2), mark: (end: "stealth"))
  
  // Shared RAM (double arrow)
  line((3.5, 0), (5, 0), mark: (start: "stealth", end: "stealth"), stroke: 2pt + rgb("#37474f"))
  content((4.25, 0.4), [#text(9pt)[*RAM*]])
  
  // Outputs arrows
  line((9.5, 1.6), "gpios.west", mark: (end: "stealth"))
  line((9.5, 0), "uarts.west", mark: (end: "stealth"))
  line((9.5, -1.6), "i2c.west", mark: (end: "stealth"))
})]
#v(1em)
