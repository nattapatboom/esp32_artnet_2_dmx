# WT32-ETH01 Art-Net / sACN Node User Manual

Firmware for a WT32-ETH01 based lighting node. The node receives Art-Net or sACN (E1.31) over Ethernet/Wi-Fi and drives LED strips, DMX outputs, relays, dimmers, motors, steppers, servos, and solenoids.

## 1. Quick Start

1. Power the WT32-ETH01.
2. If Ethernet or Wi-Fi is not connected, the board starts a setup Access Point.
3. Connect to the setup AP and open `http://192.168.4.1`.
4. Configure Network, Outputs, and protocol options.
5. Press Save. The board reboots when hardware/network settings change.

Ethernet is strongly recommended for live Art-Net/sACN streaming. Wi-Fi client mode is useful for setup or light-duty streaming.

## 2. Network Modes

### Art-Net Ethernet

Use this as the main wired lighting node mode. The device listens for Art-Net DMX on the configured UDP port. The default Art-Net port is `6454`.

### ESP-NOW Master

The board receives Art-Net/sACN from the network and forwards DMX universes wirelessly to ESP-NOW slave boards.

### ESP-NOW Slave

The board receives DMX data from an ESP-NOW master and drives its local outputs. In slave mode, the board can open a temporary setup AP; if no client connects, the AP is disabled after the timeout to reduce RF noise.

## 3. IP, Wi-Fi, and mDNS

Network tab settings:
- Ethernet DHCP or static IP.
- Wi-Fi client SSID/password.
- Wi-Fi client DHCP or static IP.
- AP SSID/password.
- mDNS hostname, for example `http://artnet.local`.

If both Ethernet and Wi-Fi are available, Ethernet is treated as the preferred active IP in status reporting.

In Art-Net Ethernet mode, the firmware starts Ethernet first. If Ethernet link is down and Wi-Fi STA fallback is enabled, it tries the saved Wi-Fi client network. If Wi-Fi does not connect within the startup timeout and AP fallback is enabled, it opens the setup AP.

Wireless fallback settings:
1. Connect by Ethernet.
2. Open Network settings.
3. Set Wi-Fi SSID/password.
4. Enable `Allow Wi-Fi STA in Art-Net Ethernet mode` to try Wi-Fi client fallback when Ethernet link is down.
5. Enable `Allow setup AP fallback in Art-Net Ethernet mode` to open the setup AP if Ethernet and Wi-Fi are unavailable.
6. Save and reboot.

ESP-NOW Slave mode always keeps the setup AP active so the slave can be configured in the field.

## 4. Output Concepts

The board supports up to 16 configured output channels. Each output has:
- Type (0-18)
- Source (GPIO, PCA9685, MCP23017, TCA9555, PCF857x)
- Start Universe
- Start Address
- Type-specific settings

For protocol data:
- `start_universe` is the Art-Net/sACN universe used by the output.
- `start_address` is the 1-based DMX channel inside that universe.


Small output devices do not need to reserve a whole universe. A relay can use only 1 byte, and a motor can use 1 to 4 bytes depending on mode. These outputs can share the same universe by using different Start Address values.

Examples:
- Relay A: Universe `0`, Start Address `1`, uses channel 1.
- Relay B: Universe `0`, Start Address `2`, uses channel 2.
- Stepper 16-bit: Universe `0`, Start Address `20`, uses channels 20-23.

LED strip outputs are different: they always start at DMX channel 1 of `start_universe` and can consume multiple consecutive universes. DMX Serial outputs also output a full 512-channel universe.

When adding a new output in the web UI, the GPIO field is auto-filled with the first unused suitable pin. Multi-pin devices such as DC motors and steppers get the next free pins automatically. Stepper homing/limit sensor pins prefer input-only GPIOs first so output-capable pins remain available for drivers.

GPIO safety rules:
- Output GPIOs cannot duplicate another output channel GPIO.
- Output GPIOs cannot use the configured Status LED GPIO.
- Output GPIOs cannot use the configured Zero-Crossing GPIO.
- Status LED GPIO and Zero-Crossing GPIO cannot be the same pin.

## 5. Long LED Strips Across Multiple Universes

One physical LED output can drive a long strip that is larger than one DMX universe. This is intended for real installations where a controller cannot be placed at every short segment.

Universe usage:
- RGB strip: 3 DMX channels per LED, 170 LEDs per universe.
- RGBW strip: 4 DMX channels per LED, 128 LEDs per universe.

Examples:
- 170 RGB LEDs = 1 universe.
- 500 RGB LEDs = 1500 channels = 3 universes.
- 500 RGB LEDs with `start_universe = 0` consumes universes `0`, `1`, and `2`.
- 300 RGBW LEDs = 1200 channels = 3 universes.

Your Art-Net/sACN controller must send all consecutive universes used by the strip. If only the first universe is sent, only the first section of the strip will update.

The Outputs table shows LED count and universe usage, for example `500 LEDs / 3U`.

## 6. Output Test Tool

The Outputs tab includes a Test button on each configured output row. Use it to verify wiring and driver hardware without sending Art-Net or sACN.

Supported tests:
- LED Strip: send a solid RGB/RGBW color to the whole strip.
- DMX Serial Output: send one DMX channel value while other channels are zero.
- Relay: ON/OFF.
- AC Dimmer and DC PWM Dimmer: level test from 0-255.
- DC Motor: forward, stop, reverse with speed value.
- Stepper Motor: move to a raw position value, stop command, or home command. Position width follows the configured resolution up to 32-bit.
- RC Servo: position value from 0-255.
- Solenoid Trigger: pulse trigger.
- Analog RGB/RGBW: color test per channel or solid color.
- Passive Buzzer: tone frequency and volume test.
- Sequential Smoke Shooter: manual trigger test.
- 7-Segment Display: number, ASCII text, or single character test.

During output testing, the selected output is temporarily overridden so it can be checked without live Art-Net/sACN input. Each test has an auto-stop duration, and the Stop Test button clears all test values.

## 7. Output Types

### LED Strip

For WS281x-style pixels using ESP32 RMT.

Settings:
- GPIO pin
- Start Universe
- Start Address is ignored; LED strips start at channel 1.
- LED Count
- Color Order: RGB, GRB, BRG, RBG, RGBW, GRBW, BRGW, WRGB

Notes:
- More than 1,111 LEDs on one GPIO may drop below 30 FPS.
- More than 4,000 total LEDs approaches ESP32 memory and timing limits.
- Long strips are supported via consecutive universes.

### DMX Serial Output

Outputs DMX512 data from the selected universe.

Notes:
- Start Address is ignored; DMX Serial outputs a complete 512-channel universe.
- First 2 DMX outputs use hardware UART.
- Additional DMX outputs use RMT fallback.
- Use an RS-485 transceiver such as MAX485/SN75176 between ESP32 GPIO and DMX wiring.

### Relay

Uses one DMX channel.

Behavior:
- Channel N: value `0-127` = OFF, value `128-255` = ON.

### AC Dimmer

Uses one DMX channel and a global zero-crossing input pin.

Behavior:
- Channel N controls phase-angle firing.
- Requires suitable TRIAC/SSR hardware and isolation.
- Configure the global ZC pin in Network settings.

### DC PWM Dimmer

Uses one GPIO with LEDC PWM. Intended for MOSFET dimming of DC loads.

DMX mapping:
- 8-bit resolution: Channel N = PWM level.
- 10/12/16-bit resolution: Channel N = MSB, N+1 = LSB. The firmware treats these modes as a 16-bit MSB-first value, then writes it to the selected PWM resolution.

### DC Motor

Supports H-bridge control.

Sub-modes:
- PWM + PWM
- PWM + DIR
- IN1 + IN2 + EN

DMX midpoint is stop. Values above/below midpoint move in opposite directions. Deadband and brake options are configurable.

DMX mapping:
- 8-bit resolution: Channel N = signed-center motor value.
- 10/12/16-bit resolution: Channel N = MSB, N+1 = LSB signed-center motor value.
- Midpoint = stop. Above midpoint = forward unless Invert Direction is enabled. Below midpoint = reverse unless inverted.
- Speed is the distance from midpoint. There is no separate speed channel for DC Motor.
- Center Deadband creates a stop zone around midpoint.

### Stepper Motor

Uses Step/Dir pins and optional Enable pin.

DMX channels:
- 8-bit resolution: Channel N = position, N+1 = speed, N+2 = command.
- 10/12/16-bit resolution: Channel N = position MSB, N+1 = position LSB, N+2 = speed, N+3 = command.
- 24-bit resolution: Channel N..N+2 = position MSB first, N+3 = speed, N+4 = command.
- 32-bit resolution: Channel N..N+3 = position MSB first, N+4 = speed, N+5 = command.

Command channel:
- `0-99`: normal position mode
- `100-199`: stop command, held for more than 1 second
- `200-255`: homing command, held for more than 1 second

Homing modes:
- Sensor mode: run until active-low limit input triggers
- Time/Stall mode: run for configured timeout, then set position to zero

### RC Servo

Uses 50 Hz LEDC PWM.

Settings:
- Min pulse width in microseconds
- Max pulse width in microseconds
- Resolution

DMX mapping:
- 8-bit resolution: Channel N = servo position.
- 10/12/16-bit resolution: Channel N = MSB, N+1 = LSB.
- The value maps between Min Pulse and Max Pulse.

### Solenoid Trigger

Uses one GPIO and one DMX channel.

Modes:
- Threshold: trigger when DMX is above configured threshold.
- Frame Sync: trigger each output frame while DMX is above 127.

DMX mapping:
- Channel N = trigger level.

Settings:
- Threshold
- Pulse width
- Pre-delay
- Post-delay/cooldown

Use suitable driver hardware. Do not drive a solenoid directly from the ESP32 pin.

### PWM Dimmer

Uses LEDC PWM (ESP32) or PCA9685 for DC load dimming.

DMX mapping:
- 8-bit: Channel N = PWM level.
- 10/12/16-bit: Channel N = MSB, N+1 = LSB.

### Analog RGB / RGBW Strip

Drives analog RGB or RGBW LED strips via 3-4 PWM channels.

Settings:
- Color Order: RGB or RGBW
- PWM frequency
- Uses LEDC (ESP32) or PCA9685 (shared frequency per chip address)

DMX mapping:
- RGB: 3 channels (R, G, B).
- RGBW: 4 channels (R, G, B, W).

### Passive Buzzer

Uses LEDC PWM with variable frequency. Two DMX channels: frequency and volume.

### Sequential Smoke Shooter

State-machine controlled smoke machine. One DMX channel triggers a sequence: Smoke → Settle → Shoot → Cooldown.

### 7-Segment Display

Supports TM1637 (2-wire protocol) or direct-drive (7-8 pins via expander).

Modes:
- TM1637 Numeric (2 Ch): 4-digit numeric display.
- TM1637 ASCII (4 Ch): 4-character ASCII display.
- Direct Drive 7-pin (A-G): 1 character, 7 segment pins.
- Direct Drive 8-pin (A-G+DP): 1 character, 8 segment pins.

## 8. Art-Net and sACN

Art-Net:
- Default UDP port is `6454`.
- The Art-Net UDP port can be changed in Network > Protocol Settings when the sender/controller must use a custom port.
- Universe value follows the Art-Net packet universe field.

sACN:
- Default UDP port is `5568`.
- The sACN UDP port can be changed in Network > Protocol Settings when the sender/controller must use a custom port.
- Supports unicast and multicast.
- Multicast groups are joined for configured output universes.
- Supports source priority tracking.

Both Art-Net and sACN use the same output mapping logic, so all output types receive data consistently.

## 9. ESP-NOW Bridge

Use ESP-NOW when Ethernet is not practical at the output location.

Recommended setup:
1. Configure the network-connected board as ESP-NOW Master.
2. Configure remote board(s) as ESP-NOW Slave.
3. Open ESP-NOW tab on the slave and copy its MAC address.
4. Add that MAC address to the master's peer list.
5. Set the DMX range for that peer, for example `U0:CH1` to `U2:CH170`.
6. Save peers.

If no peers are configured, the master falls back to broadcast. Peer entries are saved as route objects with MAC address and DMX range fields; old MAC-only entries are not used by current firmware.

Each ESP-NOW peer can have its own Universe/Channel range. The master sends only overlapping chunks to that peer. This is recommended because sending every universe to every peer wastes ESP-NOW airtime.

DMX universes are split into 200-byte ESP-NOW chunks and reassembled by offset on the slave. This allows full 512-channel universe forwarding when needed.

## 10. OTA Update

Use the Update tab to upload a firmware `.bin`.

Notes:
- OTA writes only the application partition.
- NVS settings and LittleFS files such as output channel configuration are preserved.
- After upload, the board reboots automatically.

To create a firmware `.bin` from Windows, double-click `build_firmware_bin.bat` in the project folder. It runs PlatformIO and copies the result to `dist/firmware_YYYYMMDD_HHMMSS.bin`.

The project also includes `scripts/build_ota.ps1` to build/copy firmware artifacts from PowerShell.

To flash a `.bin` through USB serial, double-click `flash_firmware_serial.bat`.

Options:
- Press Enter to use the latest `dist/firmware_*.bin`.
- Press Enter to use the default port `COM12`, or type another COM port.
- Press Enter to use baud `460800`, or type `115200` if flashing is unstable.

If auto-reset does not enter bootloader mode, hold GPIO0/BOOT, press RESET, release RESET, then release GPIO0/BOOT before continuing.

## 11. Recovery Mode

Recovery mode provides a minimal AP + web server for OTA upload.

Ways to enter recovery:
- Hold GPIO0 low during boot.
- Or trigger 3 consecutive failed/fast reboots; boot-count recovery will activate.

On this firmware, GPIO0 recovery uses Ethernet-only web recovery. Wi-Fi/AP recovery is disabled to avoid brownout on boards or power supplies that cannot handle Wi-Fi radio startup current.

In recovery mode:
1. Connect Ethernet.
2. Open the device web page at its DHCP/static Ethernet IP.
3. Upload a known-good `.bin` firmware.

## 12. Status Indicators

Status LED behavior:
- All normal modes use repeated blink-count patterns with a pause gap; the LED should not stay solid.
- 1 blink: Idle or disconnected.
- 2 blinks: Configuration AP active.
- 3 blinks: Ethernet connected.
- 4 blinks: Wi-Fi STA connected.
- 5 fast blinks: ESP-NOW slave receiving recent data.
- 6 fast blinks: Output Test override active.
- The LED always uses blink patterns in normal firmware operation; it should not stay solid.

The web status area shows IP address, packet counts, free heap, CPU frequency, current mode, and network state.

## 13. GPIO Notes

Avoid GPIO16 for user outputs. It is used by LAN8720A Ethernet power/control on WT32-ETH01 and can crash Ethernet.

Common output GPIO choices:
- 4, 12, 14, 15, 2, 17, 32, 33

Input-only pins useful for sensors/ZC:
- 36, 39, 34, 35

GPIO12 is a boot strap pin. Use proper buffering/high-Z behavior during boot.

## 14. Capacity Planning

Use `tools/load_calculator.py` to estimate node count and load.

Practical limits:
- Max configured output channels: 16
- LED strips: max 8 RMT channels
- DMX: first 2 UART, extras use RMT fallback
- Flash should stay below roughly 75% to keep OTA comfortable
- Very long LED strips can work, but FPS drops as LED count grows

For large installations, prefer distributing long runs across multiple controllers when voltage drop, refresh rate, or cabling allows it.

## 15. Troubleshooting

No web page:
- Connect to setup AP and open `http://192.168.4.1`.
- Check Ethernet link.
- Try recovery mode.

Only first part of LED strip works:
- Confirm LED count.
- Confirm controller sends all consecutive universes.
- Check RGB vs RGBW color order.

Removed WiZ channel appears in Outputs:
- WiZ support was removed from the controller firmware in v1.11 planning/cleanup.
- Delete the removed WiZ channel and save outputs.
- Control WiZ or other smart bulbs from a host system such as Node-RED, QLC+, or Home Assistant instead of the ESP32 controller.

ESP-NOW slave does not update:
- Confirm slave MAC is in master peer list, or use broadcast fallback.
- Confirm both boards are on compatible Wi-Fi/ESP-NOW channel conditions.
- Test with fewer universes first, then full load.

Stepper does not home:
- Confirm command channel is held in homing range for more than 1 second.
- Confirm sensor pin and active-low wiring.
- Confirm homing direction and speed.

AC dimmer flickers:
- Confirm zero-cross input.
- Confirm TRIAC/SSR hardware is suitable for phase dimming.
- Confirm load type supports dimming.

Brownout detector resets during boot:
- Use a stable 5 V supply with enough current for the WT32-ETH01, Wi-Fi transmit spikes, and any connected output modules.
- Power LED strips, motors, solenoids, and relays from a suitable external supply instead of the ESP32 board regulator.
- Tie external power supply ground to the controller ground.
- Test boot with output loads disconnected; if brownout disappears, reconnect loads one type at a time.

## 17. Resource & Compute Scoring System (v3)

To ensure system stability, the firmware uses a scoring system to prevent overloading the ESP32 CPU and hardware resources. Each output channel consumes "points" based on the hardware it uses and the CPU processing required.

**Total Score Limit: ≈ 109 points**

### 17.1 Resource Score
Points are calculated based on the hardware source selected:
- **GPIO:** 0.5 pts per pin
- **LEDC (PWM):** 2.5 pts per channel
- **RMT:** 3.0 pts per channel
- **UART:** 8.0 pts per port
- **DAC:** 2.0 pts per channel
- **PCA9685:** 0.25 pts per channel
- **I2C Expander:** 0.125 pts per pin

### 17.2 Compute Score
CPU load depends on the output type and the global output FPS:
- **High Load Types:** Function Generator, Steppers, and 7-Segment Direct Drive consume more CPU points.
- **LED Strips:** Compute cost increases based on the number of pixels.
- **FPS Factor:** Higher output FPS (e.g., 60 FPS vs 40 FPS) increases the overall compute score.

### 17.3 PCA9685 Frequency Interlock
- Each PCA9685 chip (by I2C address) shares a single PWM frequency.
- **Servo Priority:** If any channel on a PCA chip is set as an **RC Servo**, the entire chip is forced to **50 Hz**.
- Other PWM types on the same chip will use the frequency set by the first configured channel, provided it is compatible.

The total score is displayed in the Web UI. If the score exceeds the limit, the system may warn you that stability cannot be guaranteed.

