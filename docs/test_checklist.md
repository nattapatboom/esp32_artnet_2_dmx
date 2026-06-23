# Test Checklist

Use this checklist before field deployment, after large refactors, and before OTA updates to shared hardware.

## 1. Build And Static Checks

- [ ] Run `& "C:\Users\natta\.platformio\penv\Scripts\platformio.exe" run` and confirm success.
- [ ] Confirm flash and RAM usage remain within expected limits.
- [ ] Run `git diff --check` before committing code changes.
- [ ] If Web UI or generated metadata changed, run `& "C:\Users\natta\.platformio\penv\Scripts\python.exe" tools/build_web.py`.
- [ ] If Web UI changed, run `node tools/web_ui_smoke_test.mjs`.
- [ ] Confirm generated files such as `include/web_pages.h` are committed when required.

## 2. Boot And Network Startup

- [ ] Power-cycle the WT32-ETH01 and confirm normal boot.
- [ ] Confirm Ethernet link detection works with LAN connected.
- [ ] Confirm DHCP mode receives a valid IP address.
- [ ] Confirm static Ethernet IP settings apply after reboot.
- [ ] Confirm Wi-Fi STA fallback connects when Ethernet is unavailable and Wi-Fi is enabled.
- [ ] Confirm setup AP starts when Ethernet/Wi-Fi fallback is unavailable and AP fallback is enabled.
- [ ] Confirm mDNS name is reachable with the expected MAC suffix.
- [ ] Confirm status LED boot patterns are visible and match the active network mode.

## 3. Web UI Smoke Test

- [ ] Open the Web UI root page and confirm status/settings/outputs load.
- [ ] Open Settings and confirm all network, protocol, mode, I2C, display, and output FPS fields render.
- [ ] Open Outputs and confirm all output types `0..18` can be selected.
- [ ] Confirm output-device fields are metadata-driven and irrelevant fields stay hidden.
- [ ] Confirm Web UI displays firmware/API validation errors without client-side blocking logic.
- [ ] Confirm GPIO12 warning appears when GPIO12 is selected.
- [ ] Confirm AC Dimmer zero-crossing warning appears when ZC pin is disabled.
- [ ] Confirm resource/load display is informational and save behavior is controlled by firmware responses.

## 4. Settings API Validation

- [ ] Save valid settings and confirm they persist after reboot.
- [ ] Reject invalid `output_fps` outside `1..44`.
- [ ] Reject invalid I2C SDA/SCL/status/ZC pin overlaps.
- [ ] Reject display address that does not match selected display type.
- [ ] Reject both Art-Net and sACN disabled at the same time.
- [ ] Confirm changing network-critical settings shows the expected reboot requirement.

## 5. Output Config API Validation

- [ ] Save a valid output layout and confirm it persists in `/outputs.json`.
- [ ] Reject output type IDs outside `0..18`.
- [ ] Reject unsupported source IDs outside `0..7`.
- [ ] Reject unsupported source/type combinations from `OUTPUT_MODES[]` metadata.
- [ ] Reject duplicate GPIO pins across all primary, hybrid, and segment routes.
- [ ] Reject output GPIOs that overlap Status LED, ZC, I2C SDA, or I2C SCL.
- [ ] Reject Ethernet-reserved GPIO pins.
- [ ] Reject input-only GPIO34, GPIO35, GPIO36, and GPIO39 as outputs.
- [ ] Reject duplicate expander/PCA source-address-channel routes.
- [ ] Reject invalid I2C source addresses for PCA9685, MCP23017, TCA9555, PCF857x, MCP4725, DAC7571, and DAC7573.
- [ ] Reject DMX address ranges that overflow universe channel 512.
- [ ] Confirm DMX Output and LED Strip start mapping at channel 1 of `start_universe`.
- [ ] Reject AC Dimmer output when ZC pin is disabled.
- [ ] Reject DFPlayer count above 2.
- [ ] Reject RMT usage above 8 channels.
- [ ] Reject LEDC usage above 16 channels.
- [ ] Reject CPU budget over-limit layouts.
- [ ] Reject RAM budget over-limit layouts.

## 6. I2C Devices And Bus Safety

- [ ] Run I2C scan from Web UI and confirm known devices appear.
- [ ] Confirm I2C scan does not block the Web UI or output task for an extended period.
- [ ] Confirm display updates continue while other I2C devices are active.
- [ ] Confirm display recovery works after temporary display disconnect/reconnect.
- [ ] Confirm PCA9685 writes update only when duty changes.
- [ ] Confirm PCA9685 shared-frequency warning is logged when servo and non-servo PWM share one address.
- [ ] Confirm MCP23017 output routing works.
- [ ] Confirm TCA9555 output routing works.
- [ ] Confirm PCF857x output routing works.
- [ ] Confirm PCF857x input read works for stepper homing routes.
- [ ] Confirm MCP4725 DAC output changes with DMX value.
- [ ] Confirm DAC7571 DAC output changes with DMX value.
- [ ] Confirm DAC7573 channel A-D selection works.
- [ ] Confirm all direct `Wire` transactions are protected by `I2cBus::Lock` or another explicit `i2cMutex` guard.

## 7. Protocol Input

- [ ] Receive Art-Net OpDmx and confirm output buffer updates.
- [ ] Receive ArtPoll and confirm ArtPollReply fields are valid.
- [ ] Reject Art-Net packets with protocol version below 14.
- [ ] Receive sACN unicast and confirm output buffer updates.
- [ ] Receive sACN multicast and confirm multicast groups are joined for active universes.
- [ ] Confirm sACN priority source selection works.
- [ ] Confirm sACN source timeout fallback works after primary loss.
- [ ] Confirm `networkFramePending.exchange(false)` triggers low-latency output updates.

## 8. ESP-NOW Master And Slave

- [ ] Master mode initializes ESP-NOW with Wi-Fi radio enabled.
- [ ] Master broadcasts when peer list is empty.
- [ ] Master sends only configured peer route ranges when peers are configured.
- [ ] Master chunks one full universe according to configured chunk size.
- [ ] Slave mode keeps setup AP available for field configuration.
- [ ] Slave rejects ESP-NOW packets with invalid header.
- [ ] Slave rejects packet payload lengths above compile-time maximum.
- [ ] Slave queues ESP-NOW packets from callback and processes them on Core 1.
- [ ] Slave maps packet `universe` and `offset` directly to output buffers.
- [ ] Confirm packet queue overflow behavior does not crash the firmware.

## 9. Output Runtime By Type

- [ ] Type 0 AC Dimmer follows DMX level with valid zero-crossing input.
- [ ] Type 1 DMX Output transmits UART DMX when UART is available.
- [ ] Type 1 DMX Output falls back to RMT when UARTs are reserved.
- [ ] Type 2 Relay switches through GPIO, PCA9685, and digital expander routes.
- [ ] Type 3 RGB/RGBW LED Strip updates pixels and skips frame when RMT/NeoPixelBus is busy.
- [ ] Type 4 Single LED dims through GPIO LEDC and PCA9685 routes.
- [ ] Type 5 Analog RGB/RGBW routes each color independently.
- [ ] Type 6 DC Motor supports PWM+PWM, PWM+DIR, and IN1+IN2+EN modes.
- [ ] Type 7 Stepper STEP remains GPIO-only and DIR/ENABLE/HOME hybrid routes work.
- [ ] Type 8 Servo outputs valid 50 Hz pulse timing.
- [ ] Type 9 Passive Buzzer changes frequency and duty from DMX values.
- [ ] Type 10 DFPlayer sends expected UART commands.
- [ ] Type 11 TM1637 displays numeric and ASCII/test modes.
- [ ] Type 12 7-seg 7-pin direct drive works in no-dim, direct-dim, and common-dim modes.
- [ ] Type 13 7-seg 8-pin direct drive works in no-dim, direct-dim, and common-dim modes.
- [ ] Type 14 I2C DAC outputs correct analog range for selected DAC family.
- [ ] Type 15 PWM DAC applies calibration range correctly.
- [ ] Type 16 Function Generator changes waveform, frequency, amplitude, and offset.
- [ ] Type 17 Solenoid fires one bounded pulse per trigger condition.
- [ ] Type 18 Smoke Shooter follows smoke, settle, shoot, cooldown, and lockout timing.

## 10. Output Test Commands

- [ ] Test command endpoint accepts valid commands for each output type.
- [ ] Test command endpoint rejects invalid channel indexes.
- [ ] Test command timeout clears outputs safely.
- [ ] Clearing an active test restores DMX-driven behavior.
- [ ] DFPlayer test commands do not block the output loop.
- [ ] Motor, solenoid, and smoke shooter tests remain time-bounded.

## 11. Recovery And OTA

- [ ] Five consecutive early resets enter Recovery Mode.
- [ ] Holding BOOT/GPIO0 during startup enters Recovery Mode.
- [ ] Recovery Mode disables outputs and lighting stream tasks.
- [ ] Recovery Ethernet and open AP are available.
- [ ] Recovery mDNS registers with the recovery suffix.
- [ ] OTA upload succeeds through `/update`.
- [ ] Firmware reboots into normal mode after successful OTA.
- [ ] Failed OTA does not corrupt the running firmware.

## 12. Hardware Safety Checks

- [ ] Verify all inductive loads have flyback diodes or appropriate suppression.
- [ ] Verify relay, solenoid, motor, and smoke shooter drivers use external power stages.
- [ ] Verify GPIO outputs connected to external boards have appropriate pull-down behavior at boot.
- [ ] Verify GPIO12-connected circuits do not pull GPIO12 HIGH during reset.
- [ ] Verify Ethernet-reserved pins are not connected to outputs.
- [ ] Verify I2C pull-ups are present and bus voltage matches connected devices.
- [ ] Verify common ground between ESP32 and external drivers where required.
- [ ] Verify DMX physical output uses an RS-485 transceiver and correct termination.

## 13. Long-Run Stability

- [ ] Run at target `output_fps` for at least 30 minutes with representative DMX traffic.
- [ ] Monitor free heap for leaks during Web UI polling and output updates.
- [ ] Monitor output frame timing for stalls above 50 ms.
- [ ] Confirm web requests do not disrupt DMX/LED output timing.
- [ ] Confirm repeated save/apply cycles do not exhaust LEDC/RMT/UART resources.
- [ ] Confirm I2C display and scan operations do not starve output updates.

## 14. Release Checklist

- [ ] Build passes from a clean working tree.
- [ ] Web UI smoke test passes.
- [ ] Manual hardware smoke test passes for the deployed output types.
- [ ] Documentation is updated for any behavior changes.
- [ ] Firmware binary is archived or uploaded through the normal OTA process.
- [ ] Git commit contains only intentional files.
