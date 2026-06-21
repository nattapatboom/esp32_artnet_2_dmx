= ESP-NOW Wireless DMX Bridge

The ESP-NOW Wireless Bridge allows wireless forwarding of DMX universes from a network-connected Master board to one or more remote Slave boards.

== Recommended Setup Flow

1. *Configure Slave:* Set the remote board to ESP-NOW Slave mode. Access its Web UI, navigate to the ESP-NOW tab, and copy its physical Wi-Fi MAC Address.
2. *Configure Master:* Set the central network-connected board to ESP-NOW Master mode. Under the ESP-NOW Peer List, click *Add Peer*.
3. *Define Routing Range:* Paste the MAC Address and input the start/end universe and channel ranges mapping to this slave.
4. *Save and Sync:* Save settings. The Master starts encoding and transmitting matching DMX packets.

== Universe Chunking \& Custom Protocol

ESP-NOW frames have a physical payload limit of 250 bytes. To transmit a full 512-channel DMX universe, the Master splits the universe data into chunks using a configurable chunk size (16--230 bytes, default 200). Each chunk is prefixed with a 12-byte custom header followed by DMX data bytes.

#v(1em)
#import "@preview/cetz:0.5.2": canvas, draw

#align(center)[
#canvas(length: 1cm, {
  import draw: *
  
  // Styles
  let block-style = (fill: rgb("#e1f5fe"), stroke: 1pt + rgb("#0288d1"), radius: 0.1)
  let ch-style = (fill: rgb("#fff9c4"), stroke: 1pt + rgb("#fbc02d"), radius: 0.1)
  let hw-style = (fill: rgb("#e8f5e9"), stroke: 1pt + rgb("#388e3c"), radius: 0.1)
  
  // Master Buffer
  rect((-4, 2), (4, 1.0), ..block-style, name: "master")
  content("master", [*Master Universe Buffer (512 Bytes)*])
  
  // Packets
  rect((-6, -0.6), (-2.5, -1.8), ..ch-style, name: "pkt1")
  content("pkt1", align(center)[#text(8.5pt)[*Packet 1* \ Offset: 0 \ Len: 200 (Ch 1-200)]])
  
  rect((-1.75, -0.6), (1.75, -1.8), ..ch-style, name: "pkt2")
  content("pkt2", align(center)[#text(8.5pt)[*Packet 2* \ Offset: 200 \ Len: 200 (Ch 201-400)]])
  
  rect((2.5, -0.6), (6, -1.8), ..ch-style, name: "pkt3")
  content("pkt3", align(center)[#text(8.5pt)[*Packet 3* \ Offset: 400 \ Len: 112 (Ch 401-512)]])
  
  // Slave Buffer
  rect((-5.5, -3.2), (5.5, -4.2), ..hw-style, name: "slave")
  content("slave", [*Slave Universe Buffer (Reassembled)*])
  
  // Connect master to packets
  line((-2.5, 1.0), "pkt1.north", mark: (end: "stealth"))
  line((0, 1.0), "pkt2.north", mark: (end: "stealth"))
  line((2.5, 1.0), "pkt3.north", mark: (end: "stealth"))
  
  // Connect packets to slave
  line("pkt1.south", (-3.5, -3.2), mark: (end: "stealth"))
  line("pkt2.south", (0, -3.2), mark: (end: "stealth"))
  line("pkt3.south", (3.5, -3.2), mark: (end: "stealth"))
})
]
#v(1em)

== Payload Layout

The custom firmware packet header consists of:
1. `Header` (4 bytes): ASCII `"DMX\0"`
2. `Universe` (2 bytes): 16-bit unsigned integer (`0..32767`)
3. `Offset` (2 bytes): 16-bit unsigned integer (`0..511`)
4. `Total Length` (2 bytes): 16-bit unsigned integer (`1..512`)
5. `Length` (2 bytes): 16-bit unsigned integer (`1..230`) — number of DMX data bytes in this chunk

== Network Stability \& Planning

To maximize RF efficiency and reliability, design your layouts around these operational constraints:
- *Narrow Peer Ranges:* Map only the specific channels a slave requires. If a slave only controls a single relay at channel 10, define its peer range on the Master as `U0:CH10` to `U0:CH10`. This prevents transmitting unused DMX bytes.
- *Broadcast Fallback:* If no peers are defined on the Master, it falls back to broadcasting packets to `FF:FF:FF:FF:FF:FF`. Use this only for testing, as broadcast packets consume high airtime and increase collision rates in crowded RF environments.
- *Queue Deferral:* Under the hood, the ESP-NOW callback on Core 0 pushes incoming frames to a FreeRTOS queue. Core 1's output task handles the de-queueing and physical routing. This prevents blocking Core 0's Wi-Fi radio stack.
