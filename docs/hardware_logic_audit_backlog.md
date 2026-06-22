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

10. **Web UI exposes Ethernet RMII pins as output choices**
    - Files: `web/index.html`
    - Status: ✅ Completed (`fix(ui): block WT32 Ethernet GPIO choices`)
    - Issue: PWM DAC and Function Generator dropdowns expose GPIO25/26/27 even though they are WT32-ETH01 Ethernet RMII pins.
    - Fix direction: remove GPIO25/26/27 from Web UI output dropdowns for WT32-ETH01 and block RMII/PHY pins in add/edit validation. Keep GPIO12 selectable with warning-only behavior because existing hardware may still use it.

11. **Web UI 7-segment common-dim resource scoring overcounts LEDC**
    - Files: `web/index.html`, `include/scoring.h`
    - Status: ✅ Completed (common-dim modes count only the COM GPIO LEDC path)
    - Issue: type 12/13 common-dim modes (`mc_mode` 6-9) count every GPIO segment as LEDC, but only the COM pin needs PWM; segments are digital. Valid configs can be blocked by false LEDC exhaustion.
    - Fix direction: update JS `channelHardware()` and C++ scoring to count one LEDC for the COM GPIO path only in common-dim modes; keep direct-dim modes counting per PWM segment.

14. **Web UI inversion fields mix unrelated semantics**
    - Files: `web/index.html`, `include/output_control.h`
    - Status: ✅ Completed (`fix(ui): invert field OR-mixing`)
    - Issue: `no_pin_invert`, `no_pin2_invert`, and `no_pin3_invert` are saved both as generic `pin*_invert` fields and stepper/motion fields (`mc_step_invert`, `mc_dir_invert`, `mc_enable_active_high`). Editing can OR unrelated values and save unintended motion behavior.
    - Fix direction: split UI controls by output type or map one canonical persisted field per semantic; avoid OR-ing generic invert state into motion-specific fields.

12. **Expander/Non-GPIO `pca_addr` lost on edit**
    - Files: `web/index.html`
    - Status: ✅ Completed (`fix(ui): expander addr restore on edit + DAC single-row layout`)
    - Issue: `editOutput()` calls `toggleOutFields()` which calls `renderPinRows()` — this rebuilds DOM before source-dependent values are set. If previous render was for GPIO (source=0), `no_pca_addr` element didn't exist, so `saved` never captured it. After re-render with correct expander source, the restored `no_pca_addr` gets the first option (0x20) instead of the persisted value. Only DAC type 14 had the workaround (3rd toggle + explicit set).
    - Fix direction: re-apply `o.pca_addr` and `o.pca_channel` for all non-zero sources after the final `toggleOutFields()`, not just for DAC type 14.

13. **DAC type 14 renders a non-standard two-row layout**
    - Files: `web/index.html`
    - Status: ✅ Completed (`fix(ui): expander addr restore on edit + DAC single-row layout`)
    - Issue: DAC UI used a specialized 2-row pin layout (Row 1: DAC + addr; Row 2: DAC IC + CH selector/CH A badge), unlike every other type which uses a single standard Source | Address | Pin | Invert row.
    - Fix direction: collapse to one row; pin column shows channel dropdown (DAC7573 only) or "CH A" badge (MCP4725/DAC7571); address column shows I2C addr dropdown; invert column hidden.

## P2 / Follow-Up

15. **PCF857x 8-bit vs 16-bit handling**
    - Files: `include/i2c_gpio_expander.h`, validation in `src/main.cpp`/`web/index.html`
    - Issue: PCF857x write path sends two bytes, but PCF8574/PCF8574A are 8-bit devices. Channel 8-15 can be accepted for 8-bit addresses.
    - Fix direction: distinguish PCF8574/PCF8574A (8-bit) from PCF8575 (16-bit) or restrict channels by model/address contract.

16. **PCA/expander channel upper-bound validation incomplete**
    - Files: `src/main.cpp`, `web/index.html`
    - Issue: many routes only reject missing `255`, not channels `>15`, and 7-seg base routing does not validate `base + numSeg - 1 <= 15`.
    - Fix direction: centralize route validation helper for source/address/channel and apply to primary, hybrid, and segment routes.

17. **I2C scan/display can block output I2C timing**
    - Files: `src/main.cpp`, `include/display_driver.h`
    - Issue: I2C scan holds `i2cMutex` across all addresses; display update holds mutex across full clear/print operations. Core 1 I2C outputs can wait up to 100 ms.
    - Fix direction: scan one address per mutex acquisition or use shorter time-sliced scans; reduce display critical section frequency/duration.

18. **Output test path bypasses Core 1 ownership**
    - Files: `src/main.cpp`
    - Issue: `/api/output-test` mutates `dmxBuffer` and invokes output updates from AsyncWebServer callback context.
    - Fix direction: queue test commands to output task instead of direct mutation/rendering.

19. **Recovery mode is Ethernet-only**
    - Files: `src/main.cpp`, docs recovery section
    - Issue: recovery disables Wi-Fi/AP, so recovery UI is unavailable if Ethernet is broken. Docs currently describe dual Ethernet + open AP recovery.
    - Fix direction: either implement recovery AP or update docs if Ethernet-only is intentional.

20. **Web UI reserved-pin validation parity gaps**
    - Files: `web/index.html`, `src/main.cpp`
    - Issue: add/edit validation checks conflicts with Status LED, ZC, and I2C pins, but does not give immediate Web UI feedback for Ethernet RMII/PHY reserved pins; C++ rejects these later. I2C SDA/SCL changes also do not trigger `autoAssignOutputPins()` even though they affect reserved pins.
    - Fix direction: add the same reserved-pin list used by firmware validation to Web UI add/edit validation; include `i2c_sda` and `i2c_scl` in auto-assign change listeners.

21. **Web UI settings fields need stronger validation and state feedback**
    - Files: `web/index.html`
    - Issue: static IPv4 fields are plain text without client-side format validation; OTA upload has no file-size guard; ESP-NOW peer edits have no dirty/unsaved indicator; resource bars can be stale when `device_mode` changes.
    - Fix direction: add IPv4 validation in `saveSettings()`, OTA file-size guard, peer dirty state, and score-bar refresh on device mode/ESP-NOW settings changes.

22. **Web UI diagnostics/header labels are inconsistent**
    - Files: `web/index.html`
    - Issue: the telemetry card label says `Mode`, but `updateTelemetry()` writes `d.time` to `tel-time`. The value should either be uptime/time with a matching label, or the UI should show actual device mode.
    - Fix direction: rename the label to `Uptime`/`Time`, or map `device_mode` to a readable mode string.

23. **Web UI usability polish backlog**
    - Files: `web/index.html`
    - Issue: 7-segment direct-drive routing is shown as an unlabeled GPIO list; save buttons are not disabled during in-flight saves; alert areas lack accessible roles/semantic grouping.
    - Fix direction: add per-segment labels in `gpioLabel()`, disable save actions while pending, and add minimal accessibility semantics (`role="alert"`, field grouping) where practical.

## Verification Notes

- Always run `& "C:\Users\natta\.platformio\penv\Scripts\platformio.exe" run` after firmware changes.
- If editing `web/index.html`, run `& "C:\Users\natta\.platformio\penv\Scripts\python.exe" tools/build_web.py`, then `node tools/web_ui_smoke_test.mjs`, then firmware build.
- For hardware-sensitive fixes, prefer small isolated commits and preserve C++/Web UI/docs parity.
