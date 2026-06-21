= Troubleshooting \& Integration Guide

This section details standard debug procedures for hardware integrations, power issues, and software setup.

== Common Issues

=== Device Configuration Portal is Unreachable
- *Symptoms:* Unable to access the Web UI at `192.168.4.1` or the device's Ethernet IP.
- *Solutions:*
  1. Ensure Ethernet activity lights are blinking on the WT32-ETH01 RJ45 jack.
  2. Verify that your PC/phone is connected to the SoftAP SSID `ESP32-ArtNet-Setup-XXXX`.
  3. Trigger Recovery Mode by holding down the *BOOT (GPIO0)* pin during boot, then access the recovery interface.

=== Only the First Part of an LED Strip Lights Up
- *Symptoms:* Pixels at the start of the strip function correctly, but remaining sections stay dark or freeze.
- *Solutions:*
  1. Check that the *LED Count* field in the channel's output configuration matches the number of physically wired pixels.
  2. Verify that your DMX controller/media server is sending all required universes. For example, a strip with 300 RGB LEDs requires 2 consecutive universes (e.g. Universe 0 and Universe 1).

=== Brownout Detector Resets (Boot Loops)
- *Symptoms:* The board boot loops immediately after powering up, with console prints showing `Brownout detector was triggered`.
- *Solutions:*
  1. *Separate Power Buses:* Do not draw power for high-current loads (LED strips, motors, relays, or solenoids) directly from the WT32-ETH01's 3.3V or 5V regulator pins.
  2. *Ground Ties:* Run a dedicated external power supply for high-current loads, and ensure its Ground (`GND`) terminal is connected to the WT32-ETH01 Ground pin.
  3. *Capacitive Filtering:* Add a $100 mu upright("F")$ to $1000 mu upright("F")$ capacitor across the power lines near high-load devices (like servo or stepper drivers) to buffer transient current spikes.

== Integration Warnings

- *GPIO 12 Bootstrapping Warning:* GPIO 12 (MTDI) is a bootstrapping pin that controls the internal flash voltage regulator state during boot. If an external peripheral pulls this pin High or Low incorrectly during startup, the ESP32 will fail to read its flash chip and fail to boot. Ensure any circuit connected to GPIO 12 is in a high-impedance (high-Z) state during power-up.
- *GPIO 16 Reservation:* GPIO 16 must not be selected for user outputs under any configuration. It controls the LAN8720 Ethernet transceiver power state. Overriding it will break network connectivity.
