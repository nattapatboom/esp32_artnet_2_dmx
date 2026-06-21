# Hardware Logic Audit Backlog

Created after a runtime/hardware logic review. This is a next-session fix list; treat `docs/domain_model.md` and `docs/resource_calculator.md` as the contract when implementing.

## P0 / P1 Fix First

1. **PCA9685 register writes may be broken**
   - Files: `include/pca9685_control.h`
   - Issue: `writeChannel()` writes four LEDn registers in one I2C transaction, but MODE1 Auto-Increment (`0x20`) is not enabled. `_lastDuty[channel]` is also updated before mutex/I2C success, suppressing retries after NACK or mutex timeout.
   - Fix direction: enable Auto-Increment in `begin()`/`setFrequency()`, and only update `_lastDuty` after successful `Wire.endTransmission() == 0`.

2. **7-segment direct PWM LEDC allocation overlaps later outputs**
   - Files: `include/motion_control.h`, `include/scoring.h`, `web/index.html`
   - Issue: direct-drive 7-seg modes use `baseChan + s` for 7/8 segments but call `allocateLedc()` only once. Later outputs can reuse those LEDC channels.
   - Fix direction: reserve a contiguous LEDC block or allocate per segment; sync scoring and Web UI LEDC counts.

3. **7-segment PCA/expander routed outputs can pass config but not update**
   - Files: `include/motion_control.h`, `src/main.cpp`, `web/index.html`
   - Issue: `MotionControl::begin()` skips most `source != 0` channels, and `update()` handles `ch.source == 1` before 7-seg logic then `continue`s.
   - Fix direction: route type 12/13 through the dedicated 7-seg update path before generic PCA branches, and ensure setup runs for PCA/expander/common-dim modes.

4. **DC Motor hybrid routing ignored when primary source is PCA9685**
   - Files: `include/motion_control.h`
   - Issue: `ch.source == 1` motor update uses only `pca_channel2/3`, ignoring `pin2_source`, `pin3_source`, `pin2_addr`, `pin3_addr`, and `pinN_channel`.
   - Fix direction: use `writeOutputPin(ch, n, state/duty)` helpers or route-aware helper functions for DIR/IN2/EN; preserve PWM semantics for EN.

5. **Hybrid route persistence missing for Analog RGB and Motor**
   - Files: `include/output_control.h`
   - Issue: `pin2_source`, `pin3_source`, `pin4_source`, addresses, and channels are saved only for stepper type 7, so analog RGB/motor hybrid routes can be lost after reboot.
   - Fix direction: persist hybrid source/address/channel fields for types 5, 6, 7, 18 and any other type using those fields.

6. **Core 0/Core 1 DMX buffer race**
   - Files: `include/artnet_control.h`, `include/sacn_control.h`, `include/output_control.h`, `src/main.cpp`
   - Issue: Art-Net/sACN map directly into `OutputChannel::dmxBuffer` on Core 0 while Core 1 reads the same buffers for LEDs, DMX output, relays, motion, solenoids, and smoke.
   - Fix direction: use a queue/double-buffer handoff similar to ESP-NOW, or protect buffer swaps with a short critical section. Avoid long locks around pixel mapping.

7. **sACN property count can exceed packet bytes**
   - Files: `include/sacn_control.h`
   - Issue: `dmxLen` from `SACN_DMP_PROP_COUNT` is clamped to 512 but not checked against actual received packet length/DMP PDU length, allowing stale bytes from `rxBuf`.
   - Fix direction: reject packets where `SACN_DMP_DATA + propertyCount > len`; account for the start-code byte.

8. **ESP-NOW peer list race while saving peers**
   - Files: `include/espnow_control.h`, `src/main.cpp`
   - Issue: web callback can call `loadPeers()` and mutate `peerList` while network processing iterates it in `sendDmx()`.
   - Fix direction: protect peer list with a mutex or defer reload to the network task; copy peer list snapshot before iteration if needed.

9. **ESP-NOW receive queue silently drops chunks**
   - Files: `include/espnow_control.h`, `src/main.cpp`
   - Issue: queue depth is 16 but `espnow_chunk_size=16` requires 32 packets for one full universe; `xQueueSend()` return value is ignored.
   - Fix direction: either raise minimum chunk size or queue depth, check/drop-count on send failure, and expose a diagnostic counter.

## P2 / Follow-Up

10. **PCF857x 8-bit vs 16-bit handling**
    - Files: `include/i2c_gpio_expander.h`, validation in `src/main.cpp`/`web/index.html`
    - Issue: PCF857x write path sends two bytes, but PCF8574/PCF8574A are 8-bit devices. Channel 8-15 can be accepted for 8-bit addresses.
    - Fix direction: distinguish PCF8574/PCF8574A (8-bit) from PCF8575 (16-bit) or restrict channels by model/address contract.

11. **PCA/expander channel upper-bound validation incomplete**
    - Files: `src/main.cpp`, `web/index.html`
    - Issue: many routes only reject missing `255`, not channels `>15`, and 7-seg base routing does not validate `base + numSeg - 1 <= 15`.
    - Fix direction: centralize route validation helper for source/address/channel and apply to primary, hybrid, and segment routes.

12. **I2C scan/display can block output I2C timing**
    - Files: `src/main.cpp`, `include/display_driver.h`
    - Issue: I2C scan holds `i2cMutex` across all addresses; display update holds mutex across full clear/print operations. Core 1 I2C outputs can wait up to 100 ms.
    - Fix direction: scan one address per mutex acquisition or use shorter time-sliced scans; reduce display critical section frequency/duration.

13. **Output test path bypasses Core 1 ownership**
    - Files: `src/main.cpp`
    - Issue: `/api/output-test` mutates `dmxBuffer` and invokes output updates from AsyncWebServer callback context.
    - Fix direction: queue test commands to output task instead of direct mutation/rendering.

14. **Recovery mode is Ethernet-only**
    - Files: `src/main.cpp`, docs recovery section
    - Issue: recovery disables Wi-Fi/AP, so recovery UI is unavailable if Ethernet is broken. Docs currently describe dual Ethernet + open AP recovery.
    - Fix direction: either implement recovery AP or update docs if Ethernet-only is intentional.

## Verification Notes

- Always run `& "C:\Users\natta\.platformio\penv\Scripts\platformio.exe" run` after firmware changes.
- If editing `web/index.html`, run `& "C:\Users\natta\.platformio\penv\Scripts\python.exe" tools/build_web.py`, then `node tools/web_ui_smoke_test.mjs`, then firmware build.
- For hardware-sensitive fixes, prefer small isolated commits and preserve C++/Web UI/docs parity.
