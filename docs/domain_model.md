# Domain Model: ESP32 Art-Net Firmware

เอกสารนี้เป็นผลลัพธ์จาก `/grilling` แบบ domain-modeling โดยสกัดจากไฟล์เดิมของโปรเจคเป็นหลัก: `include/output_control.h`, `include/scoring.h`, `include/config.h`, `src/main.cpp`, `web/index.html`, `docs/user_manual/main.typ`, และ `docs/resource_calculator.md`.

## Core Domain

โปรเจคนี้คือ firmware สำหรับ WT32-ETH01 ที่รับ DMX data จาก Art-Net, sACN หรือ ESP-NOW แล้วแปลงเป็น output ทางกายภาพหลายชนิด เช่น LED, DMX, relay, dimmer, motor, servo, audio trigger และ smoke shooter.

แกนกลางของระบบคือ `OutputChannel` หนึ่งรายการ ซึ่งแทน output ทางกายภาพหนึ่งชุด พร้อมข้อมูล routing, DMX address, type-specific settings, runtime state และ peripheral handle ที่จำเป็น.

## Ubiquitous Language

| Term | Meaning | Source of Truth |
| --- | --- | --- |
| Output Channel | แถว config หนึ่งรายการที่ควบคุมอุปกรณ์หนึ่งชุด | `OutputChannel` in `include/output_control.h` |
| Output Type | ชนิดอุปกรณ์ v3 type `0..18` | `outputTypeName()` in `include/scoring.h` |
| Source | ที่มาของ pin/channel: ESP32 GPIO, PCA9685, digital expander, MCP4725 | `source` fields in `OutputChannel` |
| DMX Start Universe | universe แรกที่ channel นี้อ่านข้อมูล | `start_universe` |
| DMX Start Address | DMX address แบบ 1-based ภายใน universe | `start_address` |
| Resource Score | คะแนนทรัพยากร hardware ที่ใช้ | Contract: Configuration Contract; implementation: `estimateResources()`, `resourceScore()` |
| Compute Score | คะแนนภาระ CPU/runtime | `channelComputeScore()`, `fpsComputeFactor()` |
| Interlock | กฎป้องกัน config ที่ชนกันหรือ hardware ใช้งานไม่ได้ | `validateOutputJson()`, `validateSettingsAndOutputs()` |
| Runtime State | state ชั่วคราว เช่น smoke, solenoid, stepper command | fields in `OutputChannel` |

## Bounded Contexts

### Configuration Context

หน้าที่:
- เก็บ system settings ใน NVS `Preferences`.
- เก็บ outputs layout ใน LittleFS `/outputs.json`.
- migration config layout เป็น version 3.
- sanitize values เช่น `output_fps`.

ไฟล์หลัก:
- `include/config.h`
- `include/output_control.h`
- `src/main.cpp`

กฎสำคัญ:
- `output_fps` ต้องอยู่ในช่วง `1..44`.
- layout version ปัจจุบันคือ `3`.
- `web/index.html` เป็น source ของ UI, `include/web_pages.h` เป็นไฟล์ generated.
- **ระบบระบุตัวตนเครือข่าย (Network Identity & Naming):**
  - ในโหมดปกติ SSID ของ SoftAP และชื่อ mDNS Responder จะถูกต่อท้ายด้วยตัวอักษร MAC Address 4 หลักสุดท้าย (เช่น `ESP32-ArtNet-Setup-XXXX` และ `[mdns_name]-[xxxx].local` เป็นตัวเล็ก)
  - ข้อมูลชื่ออุปกรณ์ส่งทางโปรโตคอล ArtPollReply (Short/Long Name) จะต่อท้ายด้วย MAC Suffix เช่นกัน (เช่น Short Name: `CHAL Node-XXXX`)
- **โหมดกู้คืน (Recovery Mode):**
  - เข้าด้วยการบูตล้มเหลวซ้ำซ้อน 5 ครั้ง (5 consecutive resets) หรือกดปุ่ม BOOT (GPIO0) ค้างไว้ขณะเปิดเครื่อง
  - จะเปิดการเชื่อมต่อแบบคู่ (Dual Network): Ethernet และ Wi-Fi AP พร้อมกัน โดย AP SSID จะเป็น `ESP32-ArtNet-Recovery-XXXX` และเปิดเป็น open AP (ไม่มีรหัสผ่าน)
  - mDNS ในโหมดกู้คืนจะลงทะเบียนเป็น `[mdns_name]-recovery.local` (ไม่มี MAC suffix) เพื่อความสะดวกในการจดจำ
  - ปิดฟังก์ชันการแสดงผลเอาต์พุตและทาสก์ส่งสตรีมไฟทั้งหมดในโหมดนี้เพื่อประหยัดพลังงาน
- **การเชื่อมต่อ Wi-Fi ใหม่แบบอัตโนมัติ (Wi-Fi Auto-Reconnection):**
  - ระบบพยายาม Reconnect Wi-Fi ทุกๆ 10 วินาทีเป็นอย่างน้อย (Rate-limited 10s) เพื่อไม่ให้ลูปเครือข่ายบล็อกการทำงานหลัก
  - จะข้ามการพยายามเชื่อมต่อ Wi-Fi Client เสมอหากอุปกรณ์รันในโหมด Ethernet และมีสายแลนเชื่อมต่ออยู่ (`ethConnected || ETH.linkUp()`) เพื่อลดสัญญาณขยะในเน็ตเวิร์กไร้สาย

### Output Routing Context

หน้าที่:
- แปลง DMX bytes เป็น signal ที่เหมาะกับอุปกรณ์.
- จัดการ source หลายชนิด: GPIO, PCA9685, MCP23017, TCA9555, PCF857x, MCP4725.
- รองรับ hybrid routing สำหรับอุปกรณ์หลายขา เช่น stepper, motor, RGB/RGBW, 7-segment, smoke shooter.

ไฟล์หลัก:
- `include/output_control.h`
- `include/motion_control.h`
- `include/pca9685_control.h`
- `include/i2c_gpio_expander.h`
- `include/funcgen_control.h`
- `include/dfplayer_control.h`

กฎสำคัญ:
- STEP pin ของ stepper ต้องเป็น ESP32 GPIO.
- Motor EN ใน mode `IN1+IN2+EN` ต้องเป็น ESP32 GPIO หรือ PCA9685 เพราะต้องใช้ PWM.
- PCA9685 แชร์ frequency ต่อ chip; servo บังคับ 50 Hz.
- digital expander เหมาะกับ digital output ไม่ใช่ PWM timing.

### Protocol Input Context

หน้าที่:
- รับ DMX frame จาก Art-Net, sACN หรือ ESP-NOW.
- ทำให้ output task เห็นข้อมูลใหม่แบบ core-safe.

ไฟล์หลัก:
- `include/artnet_control.h`
- `include/sacn_control.h`
- `include/espnow_control.h`
- `src/main.cpp`

กฎสำคัญ:
- Core 0 รับ network/display/web.
- Core 1 ประมวลผล output.
- atomic flags เช่น `networkFramePending` ต้องใช้แบบปลอดภัยกับ dual-core.
- ESP-NOW callback ต้อง defer ผ่าน queue ไม่ทำงานหนักใน callback.
- ESP-NOW master รับ Art-Net/sACN แล้วส่งต่อ DMX ไปยัง slave ตาม peer route ที่เก็บใน `/espnow_peers.json`.
- ESP-NOW slave รับ packet ผ่าน callback, queue ไปประมวลผลใน `outputTask`, แล้ว map ตาม universe/offset เข้าช่อง output.
- ESP-NOW DMX packet ใช้ header 12 bytes (`DMX`, universe, offset, totalLength, length) และ data payload ปัจจุบันสูงสุด 200 bytes (`ESPNOW_DMX_CHUNK_SIZE`).
- ถ้า peer route ใน universe เดียวกันยาวกว่า chunk size, master แบ่งส่งหลาย packet โดยเพิ่ม `offset` ทีละ chunk size และตั้ง `length` ตามจำนวน bytes ใน packet นั้น; slave map packet ทันทีตาม `universe`/`offset` โดยไม่ต้องรอ reassemble ครบ universe ยกเว้น buffer แสดงสถานะ universe 0.
- หนึ่ง DMX universe 512 channels ที่ chunk size 200 ถูกแบ่งเป็นสูงสุด 3 ESP-NOW frames: offset 0, 200, 400; frame ใหญ่สุดประมาณ 212 bytes รวม custom header.
- Peer route จำกัดช่วง universe `0..32767` และ DMX address `1..512`; ถ้าไม่มี peer master จะ broadcast ไป `FF:FF:FF:FF:FF:FF`.
- ใช้ peer route ให้แคบที่สุดเพื่อลด airtime เพราะ ESP-NOW ไม่เหมาะกับการ broadcast ทุก universe ไปทุก slave พร้อมกัน.
- ESP-NOW ต้องเปิด Wi-Fi radio (`WIFI_AP_STA` เมื่อจำเป็น); slave คง setup AP ไว้เพื่อ config ภาคสนาม.
- Requirement ต่อไป: ทำให้ ESP-NOW DMX chunk size เป็น system setting ที่ผู้ใช้ปรับได้จาก Web UI หน้า ESP-NOW/Network และบันทึกใน NVS; UI ต้องแสดง tradeoff ระหว่าง packet count, airtime, latency, และ reliability.
- เมื่อทำ chunk size แบบ user-configurable ให้แยก `configured_chunk_size` สำหรับการส่งออกจาก compile-time max receive buffer; packet มี `length` อยู่แล้ว ดังนั้น slave ไม่จำเป็นต้องตั้งค่า chunk size ตรงกับ master แต่ firmware ต้อง reject packet ที่ `length` เกินขนาด buffer สูงสุดที่รองรับ.

### Validation And Interlock Context

หน้าที่:
- ป้องกัน configuration ที่ทำให้ firmware crash, peripheral ชนกัน หรือ hardware ทำงานผิด.
- ต้อง validate ทั้งฝั่ง C++ API และ Web UI.

ไฟล์หลัก:
- `src/main.cpp`
- `web/index.html`

กฎสำคัญ:
- ห้าม GPIO duplicate.
- ห้าม output ใช้ pin เดียวกับ Status LED, Zero-Crossing, I2C SDA/SCL.
- จำกัด DFPlayer ได้สูงสุด 2 channel เพราะใช้ UART1/2.
- DFPlayer มี priority ในการจอง UART; DMX GPIO ใช้ UART ที่เหลือก่อน แล้ว fallback เป็น RMT.
- RMT รวม LED strip และ DMX fallback ต้องไม่เกิน 8.
- source ต้อง match output type.

### Capacity Scoring Context

หน้าที่:
- ประเมินความหนักของ configuration ก่อน save.
- รวม hardware resource score และ compute score.

ไฟล์หลัก:
- `include/scoring.h`
- `web/index.html`
- `docs/resource_calculator.md`

กฎสำคัญ:
- weight constants ต้องตรงกันระหว่าง C++ และ JS.
- resource scoring ต้องสะท้อน routing ที่เลือกจริง เช่น hybrid pin source, segment-level routing, และ DMX UART/RMT fallback.
- total score limit มาจาก resource max ของ WT32-ETH01 + compute budget.
- resource score ไม่แทน hard interlock ทั้งหมด; validation ยังคงต้องเช็ค peripheral limit จริง.

## Aggregates

### Device Configuration

ประกอบด้วย:
- `SystemConfig`: network, protocol, pins, display, output FPS.
- `outputs[]`: รายการ `OutputChannel`.

Invariant:
- global pins ต้องไม่ชนกันเองและไม่ชนกับ output pins.
- protocol/network settings ต้องมี fallback ที่ predictable.
- settings ที่เปลี่ยน hardware/network อาจต้อง reboot.

### Output Channel

ประกอบด้วย:
- identity โดยตำแหน่งใน outputs array.
- type/source/routing fields.
- DMX location.
- type-specific parameters.
- runtime-only state และ peripheral handles.

Invariant:
- field ที่เกี่ยวกับ source ต้องสอดคล้องกับ type.
- multi-pin output ต้องมี routing ครบตาม mode.
- runtime-only pointer/state ไม่ใช่ persisted domain data.

### Hardware Resource Budget

ประกอบด้วย:
- GPIO, LEDC, RMT, UART, DAC, PCA9685 channel, digital expander channel.
- compute budget จาก output type และ `output_fps`.

Invariant:
- RMT <= 8.
- UART usable for DMX/DFPlayer <= 2.
- LEDC <= 16.
- total combined score <= `SCORE_LIMIT`.

## Configuration Contract

ส่วนนี้เป็น contract กลางสำหรับ `/outputs.json`, Web UI, C++ validation, runtime setup, และ scoring. ถ้า behavior จริงในโค้ดไม่ตรงกับตารางนี้ ให้ถือว่าเป็น implementation drift ที่ต้องแก้หรือบันทึกไว้ชัดเจน.

### Source IDs

| ID | Source | Contract |
| ---: | --- | --- |
| 0 | ESP32 GPIO | direct ESP32 pin; ต้องไม่ชน global pins หรือ output GPIO อื่น |
| 1 | PCA9685 | PWM-capable I2C expander; 16 channel ต่อ address; shared frequency ต่อ chip |
| 2 | MCP23017 | digital-only I2C GPIO expander |
| 3 | TCA9555 | digital-only I2C GPIO expander |
| 4 | PCF857x | digital-only I2C GPIO expander |
| 5 | I2C DAC | I2C DAC source ใช้ได้เฉพาะ output type 14; model selected by `dac_model` |

### I2C Address Contract

| Device/source | Valid addresses | Notes |
| --- | --- | --- |
| PCA9685 (`source=1`) | `0x40..0x47` | PWM/servo expander, 16 channels per chip |
| MCP23017 (`source=2`) | `0x20..0x27` | Digital GPIO expander |
| TCA9555 (`source=3`) | `0x20..0x27` | Digital GPIO expander |
| PCF857x (`source=4`) | `0x20..0x27`, `0x38..0x3F` | PCF8574 and PCF8574A address families |
| MCP4725 (`source=5`, `dac_model=0`) | `0x60`, `0x61` | Single-channel 12-bit I2C DAC |
| DAC7571 (`source=5`, `dac_model=1`) | `0x4C`, `0x4D` | Single-channel 12-bit I2C DAC; TI lists address support for up to two devices |
| DAC7573 (`source=5`, `dac_model=2`) | `0x4C..0x5B` | Quad 12-bit I2C DAC; `pca_channel` selects channel A-D (`0..3`) |
| SSD1306/SH1106 display | `0x3C`, `0x3D` | System display setting, not an output source |
| PCF8574 LCD display backpack | `0x27`, `0x3F` | System display setting, not an output source |

### Output Type Source Contract

| Type | Output | Primary source | Hybrid routing | Notes |
| ---: | --- | --- | --- | --- |
| 0 | AC Dimmer | GPIO only | none | uses shared zero-crossing input |
| 1 | DMX Output | GPIO only | runtime UART/RMT allocation | DFPlayer reserves UARTs first; DMX uses remaining UARTs, then RMT fallback |
| 2 | Relay | GPIO, PCA9685, digital expander | none | digital output; PCA allowed as on/off driver but still shares chip frequency |
| 3 | RGB/RGBW LED Strip | GPIO only | none | uses one RMT channel per strip |
| 4 | Single Color LED | GPIO or PCA9685 | none | GPIO path uses LEDC; PCA path uses one PCA channel |
| 5 | Analog RGB/RGBW | GPIO or PCA9685 per color | R/G/B/W each route independently via pin source fields | digital expanders are not valid because color channels need PWM |
| 6 | DC Motor | GPIO or PCA9685 | IN2/DIR and EN may route separately | EN in `IN1+IN2+EN` mode must be GPIO or PCA9685, not digital expander |
| 7 | Stepper | STEP GPIO only | DIR, ENABLE, and HOME may route separately | STEP timing stays on ESP32 GPIO; digital/PCA hybrid is for slower pins only |
| 8 | RC Servo | GPIO or PCA9685 | none | any servo on a PCA chip forces that chip to 50 Hz |
| 9 | Passive Buzzer | GPIO only | none | uses LEDC tone/PWM timing |
| 10 | DFPlayer MP3 | GPIO only | TX/RX GPIO pins | max 2 channels because UART1/2 only |
| 11 | 7-Segment TM1637 | GPIO only | CLK/DIO GPIO pins | TM1637 is not expander-routed; direct-drive displays use type 12/13 |
| 12 | 7-Segment DD 7-Pin PWM | GPIO or PCA9685 for Direct Dim; GPIO, PCA9685, or digital expander for No Dim/Common Dim segments | segment-level routing via `seg_*` or base routing | Direct Dim modes need PWM per segment, so MCP23017/TCA9555/PCF857x are invalid there |
| 13 | 7-Segment DD 8-Pin PWM | GPIO or PCA9685 for Direct Dim; GPIO, PCA9685, or digital expander for No Dim/Common Dim segments | segment-level routing via `seg_*` or base routing | Direct Dim modes need PWM per segment, so MCP23017/TCA9555/PCF857x are invalid there |
| 14 | DAC | I2C DAC preferred; ESP32 DAC GPIO path is legacy/unsafe on WT32-ETH01 | MCP4725/DAC7571 single-channel, DAC7573 channel A-D via `pca_channel` | GPIO25/26 are occupied by Ethernet; supported I2C DAC models are MCP4725, DAC7571, and DAC7573 |
| 15 | PWM DAC | GPIO or PCA9685 | none | GPIO path uses LEDC; supports duty calibration for external 0-10V or 4-20mA interface circuits |
| 16 | Function Generator | GPIO only | none | uses LEDC/timer-like runtime load |
| 17 | Solenoid | GPIO, PCA9685, digital expander | none | trigger/pulse state machine |
| 18 | Smoke Shooter | GPIO, PCA9685, digital expander | smoke and shoot pins route together or by pin fields | two digital outputs controlled by sequence state machine |

### Persisted Output JSON Fields

Fields expected to be persisted in `/outputs.json`:
- identity by array index, not by stable ID.
- `type`, `source`, `start_universe`, `start_address`.
- DAC model selection for I2C DAC outputs: `dac_model`; DAC7573 channel uses `pca_channel` (`0..3`).
- primary routing: `pin`, `pca_addr`, `pca_channel`.
- multi-pin routing: `pin2`, `pin3`, `pin4`, `pin2_source`, `pin3_source`, `pin4_source`, `pin2_addr`, `pin3_addr`, `pin4_addr`, `pin2_channel`, `pin3_channel`, `pin4_channel`.
- PCA contiguous/legacy routing fields: `pca_channel2`, `pca_channel3`, `pca_channel4`.
- LED fields: `led_count`, `color_order`, `led_protocol`.
- motor/servo/stepper/function/PWM fields: `mc_mode`, `mc_resolution`, `mc_freq`, `mc_deadband`, `mc_invert`, `mc_brake`, `mc_min_us`, `mc_max_us`, `mc_steps_per_rev`, `mc_homing_*`, `mc_scale_factor`, `mc_unit_type`, `mc_enable_active_high`, `mc_dir_invert`, `mc_step_invert`.
- PWM DAC calibration fields: `pwm_dac_mode` (`0=Custom`, `1=0-10V`, `2=4-20mA`), `pwm_dac_min`, `pwm_dac_max` as duty percent times 100 (`0..10000`).
- inversion fields: `pin_invert`, `pin2_invert`, `pin3_invert`, `pin4_invert`, `seg_inverts`.
- 7-segment direct-drive routing: `seg_pins`, `seg_sources`, `seg_addrs`, `seg_channels`.
- solenoid/smoke fields: `solenoid_mode`, `solenoid_threshold`, `solenoid_pulse_ms`, `solenoid_pre_delay`, `solenoid_post_delay`, `smoke_duration_ms`, `settle_delay_ms`, `shoot_duration_ms`, `smoke_lockout_ms`; Smoke Shooter reuses `solenoid_threshold` as its trigger threshold.
- network-like output fields if used by a type: `dest_ip`, `dest_port`.

Fields that must not be treated as persisted domain config:
- runtime buffers and handles: `dmxBuffer`, `bufferSize`, `pixelStrip`, `dmxPort`, `rmtDmx`, `dfPlayer`, `funcGen`.
- transient state: `smoke_state`, `smoke_timer`, `smoke_prev_trigger`, `prev_7seg_val`, `prev_7seg_valid`, `stepper_cmd_state`, `stepper_cmd_start_time`, `stepper_homing_start_time`, `solenoid_pulse_start`, `solenoid_pulse_active`, `solenoid_last_trigger`.
- allocated LEDC bookkeeping: `ledc_chan2`, `ledc_chan3`, `ledc_chan4`.

### Validation Gates

Configuration must pass these gates before save/apply:
- type ID must be `0..18`.
- source must match the output type source contract above.
- global pins must not overlap each other: Status LED, Zero-Crossing, I2C SDA, I2C SCL.
- any output GPIO, including hybrid GPIO pins and segment GPIO pins, must not overlap global pins.
- output GPIO pins must not duplicate across outputs.
- expander channels must not duplicate for the same source/address/channel.
- every I2C-routed output address must be inside the valid range for that device/source/model.
- display I2C address must match the selected display type: OLED `0x3C/0x3D`, PCF8574 LCD `0x27/0x3F`.
- DFPlayer count must be `<= 2`.
- RMT use from LED strips plus DMX fallback must be `<= 8`.
- LEDC use must be `<= 16`.
- 7-segment Type 12/13 Direct Dim modes (`mc_mode` 4/5) must route segments to ESP32 GPIO or PCA9685, not digital expanders.
- PCA9685 shared frequency conflicts must be surfaced; servo forces 50 Hz per chip.
- combined score must be `<= SCORE_LIMIT`.

### Scoring Contract

Resource Score must be routing-accurate:
- Count GPIO only for actual ESP32 GPIO-routed pins.
- Count LEDC only for GPIO-routed PWM outputs that actually consume LEDC.
- Count RMT for LED strips and DMX outputs that fall back after UART allocation.
- Count UART after DFPlayer priority allocation; DFPlayer consumes UART before DMX.
- Count PCA9685 channels for actual PCA-routed channels.
- Count digital expander channels for actual digital expander-routed channels.
- Count Type 12/13 per segment using `seg_sources`, `seg_pins`, `seg_channels`, or base routing rules; Direct Dim modes can only score GPIO/LEDC or PCA segment routes because digital expanders are invalid.
- Compute Score is separate from Resource Score and includes per-type runtime cost plus `output_fps` factor.

Known implementation drift to fix later:
- C++ `totalOutputScoreFromJson()` may not copy every routing field needed for routing-accurate scoring.
- Web UI `channelScore()` still has stale assumptions for some types and must be audited against this contract.
- Web UI `channelScore()` uses global `outputs` for DMX/DFPlayer allocation even when scoring a candidate `newOutputs` array.
- Web UI reserved-pin validation may miss hybrid GPIO pins when the primary source is not GPIO.
- Web UI hardware warning counters are separate from score and may still use simplified counting.

## Grilling Results

คำถามที่ใช้ grill และคำตอบที่ infer จากโค้ดเดิม:

| Question | Answer from existing files | Consequence |
| --- | --- | --- |
| อะไรคือ entity หลักของระบบ? | `OutputChannel` | เอกสาร/validation ควรอ้างอิง channel เป็นศูนย์กลาง |
| ข้อมูลไหนเป็น persisted config และข้อมูลไหนเป็น runtime state? | `OutputChannel` รวมทั้งสองกลุ่มไว้ใน struct เดียว | ต้องระวังเวลาเพิ่ม field ใหม่ว่า save/load หรือ runtime-only |
| capacity คุมด้วยจำนวน channel หรือ resource score? | user manual บอก up to 16, code comment บอก no hard channel limit และใช้ score/hardware validation | เอกสาร resource เดิมที่เขียน max 8 ต้องแก้ |
| source compatibility อยู่ที่ไหน? | C++ validation และ Web UI validation | การเพิ่ม output/source ต้องแก้สองฝั่ง |
| score เป็น hard safety ทั้งหมดไหม? | ไม่ใช่, ยังมี UART/RMT/GPIO/PCA/I2C interlock | ห้ามลด validation เหลือแค่ score |
| Web UI source of truth อยู่ที่ไหน? | `web/index.html`; `include/web_pages.h` generated | แก้ UI ต้อง regenerate web pages |
| ความเสี่ยงหลักในการ refactor คืออะไร? | struct ใหญ่, routing hybrid, validation duplicated JS/C++ | ควร refactor แบบเล็กและมี build/test ทุกครั้ง |

## Current Tidy-Up Decisions

1. เอกสาร domain model นี้เป็น reference กลางของคำศัพท์และ invariant.
2. `docs/resource_calculator.md` ต้องสะท้อน v3 current firmware ไม่ใช่รายการ output ที่ถูกยกเลิกหรือยังไม่มีใน type `0..18`.
3. ยังไม่แยก `OutputChannel` ในโค้ด เพราะมีความเสี่ยงสูงต่อ save/load, Web UI JSON และ runtime pointer ownership.
4. การปรับปรุงโค้ดในอนาคตควรเริ่มจาก helper validation หรือ shared constants ที่ลด drift ระหว่าง C++/JS โดยไม่เปลี่ยน layout ก่อน.

## Open Risks

| Risk | Why It Matters | Mitigation |
| --- | --- | --- |
| C++/JS validation drift | ผู้ใช้อาจ save ผ่าน UI ไม่ได้ หรือ API รับ config ที่ UI ไม่รับ | เพิ่ม checklist ทุกครั้งที่แก้ output type/source |
| `OutputChannel` รวม persisted/runtime state | เพิ่ม field ผิดที่อาจทำให้ migration หรือ memory lifecycle พัง | document field group ก่อน refactor |
| Generated web file stale | firmware อาจ embed UI เก่า | run `python tools/build_web.py` หลังแก้ `web/index.html` |
| Hardware docs drift | ผู้ใช้ต่อวงจรผิดหรือเลือก pin ผิด | ใช้ `docs/domain_model.md` และ `docs/resource_calculator.md` เป็นคู่กัน |
| Resource scoring drift | C++/JS score อาจรับ config หนักเกินจริงหรือ reject config ที่ควรผ่าน | เมื่อแก้ routing field ต้อง sync `include/scoring.h` และ `web/index.html` ให้ routing-accurate |
| Current scoring implementation drift | domain rule ต้องการ routing-accurate score แต่บาง C++ JSON scoring path ยังอาจ copy routing fields ไม่ครบ | audit/fix `totalOutputScoreFromJson()` และเทียบกับ JS `channelScore()` |
