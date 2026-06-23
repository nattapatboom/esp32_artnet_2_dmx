# Test Run Record

- Date/time: 2026-06-24 06:32:51 +07:00
- Firmware commit: `428ebb1` (`docs: add test run record workflow`)
- Firmware version/build: PlatformIO `wt32-eth01` release build, RAM 17.3%, Flash 63.6%
- Tester: AI assistant
- Device/IP: Not connected for OTA/manual hardware testing
- Hardware setup: Local workstation automated checks only
- Test scope: Automated build/static/Web UI smoke subset
- Result summary: Partial

## Incomplete Or Failed Items

- [ ] Manual hardware checks were not run in this pass.
- [ ] Network boot, protocol receive, ESP-NOW, OTA, and physical output runtime checks were not run in this pass.
- [ ] Settings/output validation API negative tests were not run against real firmware in this pass.

## Additional Test Items Found

- [ ] Add an automated API validation regression script for `/api/settings` and `/api/outputs` negative cases.
- [ ] Add a scripted helper to generate timestamped test run records from `docs/test_checklist.md`.

## Checklist

## 1. Build And Static Checks

- [x] Run `& "C:\Users\natta\.platformio\penv\Scripts\platformio.exe" run` and confirm success. Result: PASS.
- [x] Confirm flash and RAM usage remain within expected limits. Result: PASS, RAM 17.3%, Flash 63.6%.
- [x] Run `git diff --check` before committing code changes. Result: PASS.
- [ ] If Web UI or generated metadata changed, run `& "C:\Users\natta\.platformio\penv\Scripts\python.exe" tools/build_web.py`. Result: Not tested; no Web UI source change in this pass.
- [x] If Web UI changed, run `node tools/web_ui_smoke_test.mjs`. Result: PASS as a general smoke check.
- [ ] Confirm generated files such as `include/web_pages.h` are committed when required. Result: Not applicable; no generated Web UI files changed.

## 2. Boot And Network Startup

- [ ] Power-cycle the WT32-ETH01 and confirm normal boot. Result: Not tested.
- [ ] Confirm Ethernet link detection works with LAN connected. Result: Not tested.
- [ ] Confirm DHCP mode receives a valid IP address. Result: Not tested.
- [ ] Confirm static Ethernet IP settings apply after reboot. Result: Not tested.
- [ ] Confirm Wi-Fi STA fallback connects when Ethernet is unavailable and Wi-Fi is enabled. Result: Not tested.
- [ ] Confirm setup AP starts when Ethernet/Wi-Fi fallback is unavailable and AP fallback is enabled. Result: Not tested.
- [ ] Confirm mDNS name is reachable with the expected MAC suffix. Result: Not tested.
- [ ] Confirm status LED boot patterns are visible and match the active network mode. Result: Not tested.

## 3. Web UI Smoke Test

- [x] Open the Web UI root page and confirm status/settings/outputs load. Result: PASS via `node tools/web_ui_smoke_test.mjs` mock server.
- [x] Open Settings and confirm all network, protocol, mode, I2C, display, and output FPS fields render. Result: PASS via smoke test coverage.
- [x] Open Outputs and confirm all output types `0..18` can be selected. Result: PASS, 19 types rendered.
- [x] Confirm output-device fields are metadata-driven and irrelevant fields stay hidden. Result: PASS for smoke-tested type groups.
- [ ] Confirm Web UI displays firmware/API validation errors without client-side blocking logic. Result: Not tested in this pass.
- [x] Confirm GPIO12 warning appears when GPIO12 is selected. Result: PASS via smoke test.
- [ ] Confirm AC Dimmer zero-crossing warning appears when ZC pin is disabled. Result: Not tested in this pass.
- [ ] Confirm resource/load display is informational and save behavior is controlled by firmware responses. Result: Not tested in this pass.

## 4. Settings API Validation

- [ ] Save valid settings and confirm they persist after reboot. Result: Not tested.
- [ ] Reject invalid `output_fps` outside `1..44`. Result: Not tested.
- [ ] Reject invalid I2C SDA/SCL/status/ZC pin overlaps. Result: Not tested.
- [ ] Reject display address that does not match selected display type. Result: Not tested.
- [ ] Reject both Art-Net and sACN disabled at the same time. Result: Not tested.
- [ ] Confirm changing network-critical settings shows the expected reboot requirement. Result: Not tested.

## 5. Output Config API Validation

- [ ] Save a valid output layout and confirm it persists in `/outputs.json`. Result: Not tested.
- [ ] Reject output type IDs outside `0..18`. Result: Not tested.
- [ ] Reject unsupported source IDs outside `0..7`. Result: Not tested.
- [ ] Reject unsupported source/type combinations from `OUTPUT_MODES[]` metadata. Result: Not tested.
- [ ] Reject duplicate GPIO pins across all primary, hybrid, and segment routes. Result: Not tested.
- [ ] Reject output GPIOs that overlap Status LED, ZC, I2C SDA, or I2C SCL. Result: Not tested.
- [ ] Reject Ethernet-reserved GPIO pins. Result: Not tested.
- [ ] Reject input-only GPIO34, GPIO35, GPIO36, and GPIO39 as outputs. Result: Not tested.
- [ ] Reject duplicate expander/PCA source-address-channel routes. Result: Not tested.
- [ ] Reject invalid I2C source addresses for PCA9685, MCP23017, TCA9555, PCF857x, MCP4725, DAC7571, and DAC7573. Result: Not tested.
- [ ] Reject Type 14 DAC with ESP32 GPIO source on WT32-ETH01; only I2C DAC sources `5..7` are valid. Result: Not tested.
- [ ] Reject DMX address ranges that overflow universe channel 512. Result: Not tested.
- [ ] Confirm DMX Output and LED Strip start mapping at channel 1 of `start_universe`. Result: Not tested.
- [ ] Reject AC Dimmer output when ZC pin is disabled. Result: Not tested.
- [ ] Reject DFPlayer count above 2. Result: Not tested.
- [ ] Reject RMT usage above 8 channels, including LED strips, DMX RMT fallback, and Stepper runtime cost. Result: Not tested.
- [ ] Reject LEDC usage above 16 channels. Result: Not tested.
- [ ] Reject timer usage above 4, including AC Dimmer shared timer and Function Generator timer-like runtime slots. Result: Not tested.
- [ ] Reject CPU budget over-limit layouts. Result: Not tested.
- [ ] Reject RAM budget over-limit layouts. Result: Not tested.
- [ ] Reject 7-segment Direct Dim modes when segment routes use digital expanders instead of ESP32 GPIO or PCA9685. Result: Not tested.
- [ ] Reject Motor `IN1+IN2+EN` mode when EN is routed to a digital expander; EN must be ESP32 GPIO or PCA9685. Result: Not tested.
- [ ] Confirm Stepper configurations above available runtime/RMT capacity are rejected before runtime setup. Result: Not tested.

## 6. I2C Devices And Bus Safety

- [ ] Run I2C scan from Web UI and confirm known devices appear. Result: Not tested.
- [ ] Confirm I2C scan does not block the Web UI or output task for an extended period. Result: Not tested.
- [ ] Confirm display updates continue while other I2C devices are active. Result: Not tested.
- [ ] Confirm display recovery works after temporary display disconnect/reconnect. Result: Not tested.
- [ ] Confirm PCA9685 writes update only when duty changes. Result: Not tested.
- [ ] Confirm PCA9685 shared-frequency warning is logged when servo and non-servo PWM share one address. Result: Not tested.
- [ ] Confirm MCP23017 output routing works. Result: Not tested.
- [ ] Confirm TCA9555 output routing works. Result: Not tested.
- [ ] Confirm PCF857x output routing works. Result: Not tested.
- [ ] Confirm PCF857x input read works for stepper homing routes. Result: Not tested.
- [ ] Confirm digital expander routes are rejected for PWM-only requirements such as Motor EN and 7-segment Direct Dim. Result: Not tested.
- [ ] Confirm MCP4725 DAC output changes with DMX value. Result: Not tested.
- [ ] Confirm DAC7571 DAC output changes with DMX value. Result: Not tested.
- [ ] Confirm DAC7573 channel A-D selection works. Result: Not tested.
- [ ] Confirm all direct `Wire` transactions are protected by `I2cBus::Lock` or another explicit `i2cMutex` guard. Result: Not tested.

## 7. Protocol Input

- [ ] Receive Art-Net OpDmx and confirm output buffer updates. Result: Not tested.
- [ ] Receive ArtPoll and confirm ArtPollReply fields are valid. Result: Not tested.
- [ ] Reject Art-Net packets with protocol version below 14. Result: Not tested.
- [ ] Receive sACN unicast and confirm output buffer updates. Result: Not tested.
- [ ] Receive sACN multicast and confirm multicast groups are joined for active universes. Result: Not tested.
- [ ] Confirm sACN priority source selection works. Result: Not tested.
- [ ] Confirm sACN source timeout fallback works after primary loss. Result: Not tested.
- [ ] Confirm `networkFramePending.exchange(false)` triggers low-latency output updates. Result: Not tested.

## 8. ESP-NOW Master And Slave

- [ ] Master mode initializes ESP-NOW with Wi-Fi radio enabled. Result: Not tested.
- [ ] Master broadcasts when peer list is empty. Result: Not tested.
- [ ] Master sends only configured peer route ranges when peers are configured. Result: Not tested.
- [ ] Master chunks one full universe according to configured chunk size. Result: Not tested.
- [ ] Slave mode keeps setup AP available for field configuration. Result: Not tested.
- [ ] Slave rejects ESP-NOW packets with invalid header. Result: Not tested.
- [ ] Slave rejects packet payload lengths above compile-time maximum. Result: Not tested.
- [ ] Slave queues ESP-NOW packets from callback and processes them on Core 1. Result: Not tested.
- [ ] Slave maps packet `universe` and `offset` directly to output buffers. Result: Not tested.
- [ ] Confirm packet queue overflow behavior does not crash the firmware. Result: Not tested.

## 9. Output Runtime By Type

- [ ] Type 0 AC Dimmer follows DMX level with valid zero-crossing input. Result: Not tested.
- [ ] Type 1 DMX Output transmits UART DMX when UART is available. Result: Not tested.
- [ ] Type 1 DMX Output falls back to RMT when UARTs are reserved. Result: Not tested.
- [ ] Type 2 Relay switches through GPIO, PCA9685, and digital expander routes. Result: Not tested.
- [ ] Type 3 RGB/RGBW LED Strip updates pixels and skips frame when RMT/NeoPixelBus is busy. Result: Not tested.
- [ ] Type 4 Single LED dims through GPIO LEDC and PCA9685 routes. Result: Not tested.
- [ ] Type 5 Analog RGB/RGBW routes each color independently. Result: Not tested.
- [ ] Type 6 DC Motor supports PWM+PWM, PWM+DIR, and IN1+IN2+EN modes. Result: Not tested.
- [ ] Type 7 Stepper STEP remains GPIO-only and DIR/ENABLE/HOME hybrid routes work. Result: Not tested.
- [ ] Type 8 Servo outputs valid 50 Hz pulse timing. Result: Not tested.
- [ ] Type 9 Passive Buzzer changes frequency and duty from DMX values. Result: Not tested.
- [ ] Type 10 DFPlayer sends expected UART commands. Result: Not tested.
- [ ] Type 11 TM1637 displays numeric and ASCII/test modes. Result: Not tested.
- [ ] Type 12 7-seg 7-pin direct drive works in no-dim, direct-dim, and common-dim modes. Result: Not tested.
- [ ] Type 13 7-seg 8-pin direct drive works in no-dim, direct-dim, and common-dim modes. Result: Not tested.
- [ ] Type 14 I2C DAC outputs correct analog range for selected DAC family. Result: Not tested.
- [ ] Type 15 PWM DAC applies calibration range correctly. Result: Not tested.
- [ ] Type 16 Function Generator changes waveform, frequency, amplitude, and offset. Result: Not tested.
- [ ] Type 17 Solenoid fires one bounded pulse per trigger condition. Result: Not tested.
- [ ] Type 18 Smoke Shooter follows smoke, settle, shoot, cooldown, and lockout timing. Result: Not tested.

## 10. Output Test Commands

- [ ] Test command endpoint accepts valid commands for each output type. Result: Not tested.
- [ ] Test command endpoint rejects invalid channel indexes. Result: Not tested.
- [ ] Test command timeout clears outputs safely. Result: Not tested.
- [ ] Clearing an active test restores DMX-driven behavior. Result: Not tested.
- [ ] DFPlayer test commands do not block the output loop. Result: Not tested.
- [ ] Motor, solenoid, and smoke shooter tests remain time-bounded. Result: Not tested.

## 11. Recovery And OTA

- [ ] Five consecutive early resets enter Recovery Mode. Result: Not tested.
- [ ] Holding BOOT/GPIO0 during startup enters Recovery Mode. Result: Not tested.
- [ ] Recovery Mode disables outputs and lighting stream tasks. Result: Not tested.
- [ ] Recovery Ethernet and open AP are available. Result: Not tested.
- [ ] Recovery mDNS registers with the recovery suffix. Result: Not tested.
- [ ] OTA upload succeeds through `/update`. Result: Not tested.
- [ ] Firmware reboots into normal mode after successful OTA. Result: Not tested.
- [ ] Failed OTA does not corrupt the running firmware. Result: Not tested.

## 12. Hardware Safety Checks

- [ ] Verify all inductive loads have flyback diodes or appropriate suppression. Result: Not tested.
- [ ] Verify relay, solenoid, motor, and smoke shooter drivers use external power stages. Result: Not tested.
- [ ] Verify GPIO outputs connected to external boards have appropriate pull-down behavior at boot. Result: Not tested.
- [ ] Verify GPIO12-connected circuits do not pull GPIO12 HIGH during reset. Result: Not tested.
- [ ] Verify Ethernet-reserved pins are not connected to outputs. Result: Not tested.
- [ ] Verify I2C pull-ups are present and bus voltage matches connected devices. Result: Not tested.
- [ ] Verify common ground between ESP32 and external drivers where required. Result: Not tested.
- [ ] Verify DMX physical output uses an RS-485 transceiver and correct termination. Result: Not tested.

## 13. Long-Run Stability

- [ ] Run at target `output_fps` for at least 30 minutes with representative DMX traffic. Result: Not tested.
- [ ] Monitor free heap for leaks during Web UI polling and output updates. Result: Not tested.
- [ ] Monitor output frame timing for stalls above 50 ms. Result: Not tested.
- [ ] Confirm web requests do not disrupt DMX/LED output timing. Result: Not tested.
- [ ] Confirm repeated save/apply cycles do not exhaust LEDC/RMT/UART resources. Result: Not tested.
- [ ] Confirm I2C display and scan operations do not starve output updates. Result: Not tested.

## 14. Release Checklist

- [x] Build passes from a clean working tree. Result: PASS before this test record file was created.
- [x] Web UI smoke test passes. Result: PASS.
- [ ] Manual hardware smoke test passes for the deployed output types. Result: Not tested.
- [ ] Documentation is updated for any behavior changes. Result: Not applicable; no firmware behavior changed.
- [ ] Firmware binary is archived or uploaded through the normal OTA process. Result: Not tested.
- [ ] Git commit contains only intentional files. Result: Pending at time of record creation.
