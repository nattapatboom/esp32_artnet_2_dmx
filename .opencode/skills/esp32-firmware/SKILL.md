---
name: esp32-firmware
description: Full-stack ESP32 Art-Net firmware project (WT32-ETH01) — PlatformIO, 16 output types, DMX/Art-Net control, Web UI, OTA deploy
metadata:
  board: wt32-eth01
  framework: arduino
  platform: espressif32
  build_cmd: pio run
  ota_url: http://192.168.1.93/update
  ota_cmd: curl.exe -F "update=@.pio\build\wt32-eth01\firmware.bin" http://192.168.1.93/update
  language: th
---

## Project Overview

ESP32-based Art-Net to DMX/Pixel/Relay/Motion controller for stage lighting.
รับ Art-Net ผ่าน Ethernet (LAN) → ควบคุม output หลายประเภทผ่าน GPIO, PCA9685, I2C expander

## Board: WT32-ETH01
- **CPU:** ESP32 dual-core (Core 0 = network/display, Core 1 = application)
- **Flash:** 4MB (partition: app 1.9MB, LittleFS 190KB) — ปัจจุบัน ~66.6%
- **RAM:** 320KB — ปัจจุบัน ~17.5%
- **Usable GPIO:** 4, 12, 14, 15, 2, 17, 32, 33 (+ input-only 36, 39, 34, 35)
- **Built-in:** Ethernet (LAN8720), no WiFi needed in Ethernet mode

## Output Types (v3 — 0-18)

| Type | Name | DMX Bytes | Source Support | Resource |
|------|------|-----------|----------------|----------|
| 0 | AC Dimmer (TRIAC) | 1-2 | GPIO only | Timer + ZC |
| 1 | DMX Serial Output | 512 | GPIO (UART1/2 or RMT) | UART 1-2 |
| 2 | Relay (On/Off) | 1 | GPIO, PCA, Expander | GPIO 1 |
| 3 | RGB LED (NeoPixel) | 3-4/led | GPIO only (RMT) | RMT 1ch |
| 4 | Single Color LED (PWM) | 1-2 | GPIO, PCA | LEDC 1ch |
| 5 | Analog RGB/RGBW | 3-4 | GPIO, PCA | LEDC 3-4ch |
| 6 | DC Motor (H-Bridge) | 1-2 | GPIO, PCA, Expander | LEDC 1-3ch |
| 7 | Stepper Motor | 3-5 | GPIO+Expander hybrid | FastAccelStepper |
| 8 | RC Servo | 1 | GPIO, PCA | LEDC 1ch |
| 9 | Passive Buzzer | 2 | GPIO only | LEDC 1ch |
| 10 | DFPlayer MP3 Module | 3 | GPIO only (UART) | UART |
| 11 | 7-Segment 2-Pin (TM1637) | 2-4 | GPIO, Expander | GPIO 2 |
| 12 | 7-Segment DD 7-Pin PWM | 1 (Direct) | GPIO only (LEDC) | LEDC 1-7ch |
| 13 | 7-Segment DD 8-Pin PWM | 1 (Direct) | GPIO only (LEDC) | LEDC 1-8ch |
| 14 | DAC (Analog Out) | 1 | GPIO only (GPIO25/26) | DAC |
| 15 | PWM DAC (RC Filter) | 1-2 | GPIO, PCA | LEDC 1ch |
| 16 | Function Generator | 5 | GPIO only | LEDC 1ch + Timer |
| 17 | Solenoid Trigger | 1 | GPIO, Expander | GPIO 1 |
| 18 | Sequential Smoke Shooter | 1 | GPIO, Expander | GPIO 2 |

## Architecture

### Key Files
- `include/output_control.h` — Core: `OutputChannel` struct, `setupChannels()`, `loadChannels()`, `saveChannels()`, DMX processing
- `include/motion_control.h` — `update()` loop for all motion/PWM/DFPlayer types
- `include/web_pages.h` — Embedded HTML/CSS/JS for Web UI (~2350 lines)
- `include/scoring.h` — Resource scoring system (limit 100pts)
- `include/dfplayer_control.h` — DFPlayer MP3 protocol driver
- `include/dimmer_control.h` — AC Dimmer with ZC interrupt
- `include/config.h` — System configuration struct
- `src/main.cpp` — Entry point, HTTP API routes, validation, network tasks

### OutputChannel Struct (output_control.h:98-189)
- `type` (0-18 v3), `source` (0=GPIO, 1=PCA9685, 2=MCP23017, 3=TCA9555, 4=PCF857x)
- `pin`-`pin4` with `_source`/`_addr`/`_channel` for expander hybrid
- `dmxBuffer`, `bufferSize` (512 bytes for all non-LED types)
- `pixelStrip` (LED), `dmxPort`/`rmtDmx` (DMX), `dfPlayer` (MP3)
- Source compatibility (v3): PCA (2,4,6,8,5,18), Digital (2,6,17,18)

### Hardware Resource Limits
- **UART:** 3 total. UART0=console. UART1 and UART2 are dynamically allocated. **DFPlayer has priority on UART** (assigns to UART2, then UART1) because DMX can dynamically fall back to RMT.
- **LEDC:** 8 channels max (PWM dimmer, motor, servo, RGB, buzzer)
- **RMT:** 8 channels max (LED strips, extra DMX fallback). Total RMT channels (LEDs + extra DMX fallback) cannot exceed 8.
- **Timer:** 4 total (AC dimmer uses 1)
- **I2C:** 1 bus (Wire), shared by all expanders + display
- **Dynamic UART Allocation:** DFPlayer is allocated first. DMX uses any remaining UART. If none are free, DMX falls back to RMT driver automatically.

### System Security & Stability
- **HTTP DoS Protection:** Rejects any incoming HTTP requests with bodies exceeding 64KB to prevent OOM attacks.
- **Settings Sanitization:** Validates `output_fps` to be between 1 and 100 on loading and saving, preventing division-by-zero crashes.
- **Memory Allocation Safety:** Performs NULL checks on all `calloc()` calls allocating DMX buffers.
- **Interrupt Storm Rate Limiting:** Enforces a 5ms rate limit on zero-cross interrupts to avoid lockups when the Zero-Cross pin floats.
- **Core-Safe Deferral:** ESP-NOW callback pushes packets into a FreeRTOS queue, deferring the thread-unsafe `mapDmxDataToChannels()` processing to the Core 1 `outputTask` thread.
- **Atomic Flags:** Uses `std::atomic<bool>` for cross-core signaling flags (`networkFramePending`, `newRxData`) to prevent thread-safety buffer race conditions.
- **I2C Hang Protection:** Replaces blocking `portMAX_DELAY` lockups with a 100ms timeout check (`xSemaphoreTake(i2cMutex, pdMS_TO_TICKS(100)) == pdTRUE`) on `i2cMutex` takes, safeguarding task operations if hardware faults freeze the I2C bus SCL/SDA lines.
- **Bootstrapping Pin warning:** Actively checks and warns the user in the Web UI if the critical bootstrapping pin GPIO 12 (MTDI) is configured, preventing bootloops.
- **Configuration Layout Versioning:** Enforces layout version 3 on config file loading and saving, with automated v1→v3 and v2→v3 migration paths during `loadChannels()`.

## Resource Scoring System (v3)

ระบบตรวจวัดคะแนนการใช้งานทรัพยากร (Resource Scoring) จำกัดคะแนนรวมทุกแชนเนลไม่เกิน **100 คะแนน** เพื่อป้องกันภาระงานของไมโครคอนโทรลเลอร์ (CPU/LEDC/RMT) สูงเกินไปจนทำให้สัญญาณแกว่ง (Jitter) หรือระบบดับเนื่องจาก OOM โดยมีคะแนนแบ่งตามประเภทช่องสัญญาณดังนี้:

- **RGB LED (Type 3):** `2.0` คะแนนพื้นฐาน + `1.0` คะแนนต่อทุก ๆ 100 เม็ดพิกเซล
- **DMX Output (Type 1):** `4.0` คะแนนต่อหนึ่งพอร์ตแชนเนล (เนื่องจากกินช่องสัญญาณ UART/RMT และบัฟเฟอร์ขนาด 512 ไบต์)
- **AC Dimmer (Type 0) / RC Servo (Type 8) / 7-Segment (Type 11/12/13) / DFPlayer MP3 (Type 10):** `2.0` คะแนนต่อช่องสัญญาณ
- **Single Color LED (Type 4) / DC Motor (Type 6) / Analog RGB (Type 5):** `3.0` คะแนนต่อช่องสัญญาณ (ใช้ทรัพยากร LEDC หลายแชนเนล)
- **Stepper Motor (Type 7):** `8.0` คะแนนต่อแชนเนล (ใช้ FastAccelStepper ขัดจังหวะความถี่สูง)
- **Passive Buzzer (Type 9) / Smoke Shooter (Type 18):** `1.5` คะแนนต่อช่องสัญญาณ
- **Function Generator (Type 16):** `3.0` คะแนนต่อช่องสัญญาณ (ใช้ LEDC + Timer)
- **PWM DAC (Type 15) / 7-Seg DD PWM (Type 12/13):** `3.0` คะแนนต่อช่องสัญญาณ (ใช้ LEDC หลายแชนเนล)
- **DAC (Type 14) / Relay (Type 2) / Solenoid (Type 17):** `0.5` คะแนนต่อช่องสัญญาณ (เอาต์พุตเปิด/ปิดทั่วไป)

---

## Hardware Module Integration & Reference Manuals

คู่มือและข้อกำหนดทางเทคนิคสำหรับโมดูลฮาร์ดแวร์ที่เชื่อมต่อกับบอร์ด WT32-ETH01:

### 1. PCA9685 (16-Channel PWM Expander)
- **Interface:** I2C (Address 0x40 - 0x47, ตั้งค่าผ่านแถบ Dip-switch บนโมดูล)
- **Usage:** เหมาะสำหรับ Relay, DC PWM Dimmer, DC Motor, Servo, Analog RGB
- **Note:** ชิปนี้แชร์ความถี่ร่วมกันทั้งชิป (Shared Frequency) หากช่องสัญญาณใดตั้งค่าเป็น RC Servo (50Hz) ชิปตัวนั้นจะทำงานที่ความถี่ 50Hz ทันที ซึ่งส่งผลต่อความถี่ของแชนเนลอื่นบนที่อยู่เดียวกัน

### 2. DFPlayer Mini (MP3 Module)
- **Interface:** Serial UART (ใช้ Serial2 หรือ Serial1 บนความเร็ว 9600 bps)
- **Pinout:** ขา TX ของบอร์ดเข้า RX ของโมดูล (ต้องมีตัวต้านทาน 1K โอห์มอนุกรมเพื่อลดเสียงรบกวน), ขา RX ของบอร์ดเข้า TX ของโมดูล
- **Control Flow:** ควบคุมผ่านแชนเนล DMX 3 ไบต์ (Byte 1 = คำสั่ง Play/Pause/Stop, Byte 2 = หมายเลขโฟลเดอร์/แทร็ก, Byte 3 = ระดับเสียง 0-30)

### 3. TM1637 (4-Digit 7-Segment Display)
- **Interface:** 2-wire serial (CLK, DIO)
- **Usage:** เหมาะสำหรับแสดงผลตัวเลขคะแนนหรือเวลานับถอยหลังของระบบ

### 4. MCP23017 / TCA9555 / PCF857x (I2C GPIO Expander)
- **Interface:** I2C (Address 0x20 - 0x27)
- **Usage:** ขยายช่องสัญญาณดิจิทัลเอาต์พุต (Relay, Solenoid, Smoke Shooter) และอินพุต (Limit Switch ของมอเตอร์สเต็ป) พร้อมระบบป้องกันบัสล็อกค้างด้วย I2C Hang Protection 100ms

### 5. PWM DAC (Type 15) — RC Low-Pass Filter
- **Interface:** GPIO → LEDC PWM → RC Low-Pass Filter
- **Usage:** แปลง PWM เป็นแรงดันแอนะล็อกเรียบสำหรับควบคุมไฟ DC, มอเตอร์ขนาดเล็ก, หรือเป็น CV (Control Voltage)
- **DMX:** 1-2 bytes (ขึ้นอยู่กับ resolution 8-16 bit), GPIO-only
- **PWM Carrier:** Configurable ผ่าน `mc_freq` (Hz) — ควรตั้งให้สูงกว่า fc ของ RC filter 3-5 เท่า
- **Resolution:** 8-16 bit ผ่าน `mc_resolution` (ความสัมพันธ์: `max freq ≈ 80MHz / 2^resolution`)
- **RC Filter Reference:** Web UI มีเครื่องคิดเลข fc ในหน้า channel config ให้ใส่ R/C → คำนวณ fc อัตโนมัติ

### 6. Function Generator Output (Type 16) — RC Low-Pass Filter Circuit

วงจร RC Low-Pass Filter สำหรับแปลง PWM จาก LEDC เป็นสัญญาณแอนะล็อกเรียบ:

```
GPIO (PWM Out) ──┬── R ──┬────── Analog Out
                 │       │
                GND     C │
                         │
                        GND
```

- **Single-pole (1st order):** rolloff 20dB/dec
- **2-pole (2nd order):** rolloff 40dB/dec (ต่อ R/C อีก stage อนุกรม)
- **Cutoff frequency:** `fc = 1 / (2π × R × C)`

**ค่าที่แนะนำ:**

| Freq Range | R | C | fc | Notes |
|-----------|----|----|-----|-------|
| 1-100 Hz | 10kΩ | 100nF | ~159 Hz | Sine wave smooth |
| 100-1kHz | 1kΩ | 100nF | ~1.6 kHz | General purpose |
| 1k-10kHz | 1kΩ | 10nF | ~15.9 kHz | Pass waveform, kill PWM carrier |
| Square/PWM | 1kΩ | 1nF | ~159 kHz | Minimal filtering (keep edges) |

**ข้อควรระวัง:**
- PWM carrier ความถี่ LEDC ตั้งไว้ที่ 5000Hz (duty resolution 13-bit) — ต้องเลือก fc ให้ต่ำกว่า carrier อย่างน้อย 3-5 เท่า
- DC Offset (DMX Byte 4): ถ้าต้องการ offset แบบ analog ให้ใช้ op-amp summing circuit หรือ AC-coupling ผ่าน capacitor + bias resistor
- โหลดอิมพีแดนซ์สูง (>10kΩ) เพื่อไม่ให้ filter cutoff เปลี่ยน

---

## Developer Tools

ระบบประกอบด้วยสคริปต์ Python ในโฟลเดอร์ `tools/` เพื่อช่วยพัฒนาและทดสอบ Web UI ได้อย่างรวดเร็ว:

- **[build_web.py](file:///c:/Users/natta/Documents/bar_program/esp32_eth01_artnet_device/tools/build_web.py)**
  - ทำการย่อขนาด (Minify) ไฟล์ HTML, CSS และ JavaScript จาก `web/index.html` แล้วแปลงเป็นตัวแปรข้อความดิบ (Raw String) ในไฟล์ `include/web_pages.h`
  - สคริปต์นี้จะถูกเรียกโดยอัตโนมัติในกระบวนการทำงานของ `build_firmware_bin.bat`
- **[extract_web.py](file:///c:/Users/natta/Documents/bar_program/esp32_eth01_artnet_device/tools/extract_web.py)**
  - เป็นสคริปต์เสริมสำหรับย้อนสกัดไฟล์ HTML/JS ออกมาจากไฟล์ `include/web_pages.h`
- **[web_mock_server.py](file:///c:/Users/natta/Documents/bar_program/esp32_eth01_artnet_device/tools/web_mock_server.py)**
  - จำลองระบบ API REST ของบอร์ด ESP32 บนพอร์ต local (`http://localhost:5000`)
  - ช่วยให้ผู้พัฒนาสามารถแก้ไข UI ใน `web/index.html` และทดสอบฟังก์ชันการทำงานบนหน้าเว็บได้ทันทีโดยไม่ต้องคอยคอมไพล์และอัปโหลดไปที่บอร์ดจริง

## Coding Conventions
- **No comments** in code unless necessary
- Follow existing patterns (look at neighboring code first)
- Use `std::vector<OutputChannel>` for channel storage
- JSON serialization with ArduinoJson 7.x
- Web UI in embedded HTML string in web_pages.h
- Thai language for communication

## Build & Deploy
```powershell
# Build
pio run

# OTA upload
curl.exe -F "update=@.pio\build\wt32-eth01\firmware.bin" http://192.168.1.93/update
```

## Handover System
- Docs in `handover/` folder, format `x.xx.xx.md`
- Current: [1.23.00.md](file:///c:/Users/natta/Documents/bar_program/esp32_eth01_artnet_device/handover/1.23.00.md)
- Rules: no editing old files, max 1000 lines per file
- SEE `handover/README.md` for full system rules

## When Modifying Code
1. Read existing files to understand patterns
2. Check resource limits (UART, LEDC, RMT, GPIO)
3. Ensure expander source compatibility
4. Build before OTA
5. Update handover if adding significant features
