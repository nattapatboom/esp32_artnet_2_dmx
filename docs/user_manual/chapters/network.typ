= Network Modes \& System Identity

The WT32-ETH01 firmware supports multiple network modes and automated fallbacks to maintain network responsiveness under varying installation conditions.

== Network Modes

- *Art-Net Ethernet (Wired):* The main operating mode. The device obtains an IP address via DHCP or static configuration and listens for Art-Net UDP packets on the default port `6454`.
- *ESP-NOW Master:* Receives DMX inputs via Art-Net/sACN and encodes the stream into custom wireless packets sent to remote slave nodes.
- *ESP-NOW Slave:* Receives wireless DMX streams directly from an ESP-NOW Master and drives local output peripherals.

== Network Fallback System

During boot in Art-Net Ethernet mode, the device executes a priority-based fallback logic to ensure communication remains accessible if cables are disconnected or signals are weak. The flowchart below outlines this sequence.

#v(1em)
#import "@preview/fletcher:0.5.8" as fletcher: diagram, node, edge
#import fletcher.shapes: diamond, rect

#align(center)[
#diagram(
  spacing: (15mm, 12mm),
  node-stroke: 1pt + rgb("#0288d1"),
  node-fill: rgb("#e1f5fe"),
  edge-stroke: 1pt + rgb("#555555"),
  
  node((0,0), [*Start Boot*]),
  edge("-|>"),
  node((0,1), [Ethernet Cable\ Connected?], shape: diamond, fill: rgb("#fff9c4"), stroke: 1pt + rgb("#fbc02d")),
  edge("-|>", label: [Yes], label-side: left),
  node((1,1), [Activate Ethernet], fill: rgb("#e8f5e9"), stroke: 1pt + rgb("#388e3c")),
  
  edge((0,1), (0,2), "-|>", label: [No], label-side: left),
  node((0,2), [Wi-Fi Client\ Fallback?], shape: diamond, fill: rgb("#fff9c4"), stroke: 1pt + rgb("#fbc02d")),
  edge("-|>", label: [Yes], label-side: left),
  node((1,2), [Connect Wi-Fi STA], shape: rect),
  edge("-|>"),
  node((2,2), [STA Connected?], shape: diamond, fill: rgb("#fff9c4"), stroke: 1pt + rgb("#fbc02d")),
  edge("-|>", label: [Yes], label-side: left),
  node((3,2), [Activate Wi-Fi], fill: rgb("#e8f5e9"), stroke: 1pt + rgb("#388e3c")),
  
  edge((0,2), (0,3), "-|>", label: [No], label-side: left),
  edge((2,2), (2,3), "-|>", label: [No], label-side: left),
  node((0,3), [Open Setup AP\ (192.168.4.1)], fill: rgb("#ffebee"), stroke: 1pt + rgb("#d32f2f")),
  node((2,3), [Open Setup AP\ (192.168.4.1)], fill: rgb("#ffebee"), stroke: 1pt + rgb("#d32f2f"))
)
]
#v(1em)

== System Identity \& MAC Suffix
To prevent network clashes when multiple lighting nodes are deployed on the same subnet, the device generates distinct identifiers based on its unique MAC address:
- *Setup AP SSID:* `ESP32-ArtNet-Setup-XXXX` (where `XXXX` is the last 4 hex characters of the MAC address).
- *mDNS Hostname:* `http://[hostname]-[xxxx].local` (where the suffix is lowercase).
- *ArtPollReply Names:* Short Name: `CHAL Node-XXXX`; Long Name incorporates the board type and suffix.

== Boot Resiliency \& Recovery Mode

If invalid configurations or networking issues cause the firmware to crash or boot loop, the device provides a hardware and software-based *Recovery Mode*:

1. *5-Power Cycle Trigger:* Powering the device on and off consecutively 5 times (rebooting within 15 seconds of boot) forces the board into Recovery Mode. A watchdog timer resets this count to 0 after 15 seconds of stable runtime.
2. *Physical Button Trigger:* Hold the physical *BOOT button (GPIO0)* low during power-up.

When Recovery Mode is activated:
- *Dual Network Access:* The node spins up both Ethernet and an open (no password) Wi-Fi AP named `ESP32-ArtNet-Recovery-XXXX`.
- *mDNS Resolution:* The recovery interface resolves at `http://[hostname]-recovery.local` (with no MAC suffix, simplifying access).
- *Low Power State:* All physical output signals and DMX listening loops are suspended to avoid brownout conditions.

== Status LED Blink Codes

The Status LED (default GPIO 5) communicates the system state through distinct repeating flash sequences:

#align(center)[
#table(
  columns: (1.5fr, 3.5fr),
  inset: 10pt,
  align: (center, left),
  stroke: 0.5pt + gray,
  [*Blinks*], [*Device State*],
  [1 Blink], [Disconnected or network interface idle],
  [2 Blinks], [SoftAP configuration portal active],
  [3 Blinks], [Ethernet link established and active],
  [4 Blinks], [Wi-Fi STA connection established and active],
  [5 Fast Blinks], [ESP-NOW Slave receiving recent valid packets],
  [6 Fast Blinks], [Manual Output Test mode active (overriding DMX stream)],
)
]
