# Project Handover v1.11.0 (Summary of Feature Planning & Architecture)

เอกสารฉบับนี้เป็นข้อมูลสรุปแผนงานออกแบบและการเตรียมพัฒนาฟีเจอร์สำหรับเฟิร์มแวร์เวอร์ชัน **v1.11.0** โดยสร้างขึ้นใหม่เพื่อสรุปประเด็นการประเมินทรัพยากร ขนาดหน่วยความจำ และโครงสร้างสถาปัตยกรรมระบบ โดยมีการอ้างอิงเปรียบเทียบกับบิลด์ที่ใช้งานอยู่ปัจจุบันในเวอร์ชัน **v1.10.0** อย่างชัดเจน

---

## 1. ข้อมูลอ้างอิงไฟล์แผนงาน (Document References)

*   **เฟิร์มแวร์เวอร์ชันใช้งานจริงปัจจุบัน:** [handover_v1.10.0.md](file:///c:/Users/natta/Documents/bar_program/esp32_eth01_artnet_device/handover/handover_v1.10.0.md) (ล่าสุด ณ วันที่ 17 มิถุนายน 2026 บิลด์ `firmware_20260617_020449.bin`)
*   **ไฟล์แบบร่างแผนงานดิบ v1.11.0:** [handover_v1.11.0.md](file:///c:/Users/natta/Documents/bar_program/esp32_eth01_artnet_device/handover/handover_v1.11.0.md) (ประกอบด้วยคำถามเชิงออกแบบ บันทึกการตัดสินใจ และขั้นตอนแบบร่างข้อเสนอแนะ)
*   **เอกสารคำนวณทรัพยากรบอร์ด:** [resource_calculator.md](file:///c:/Users/natta/Documents/bar_program/esp32_eth01_artnet_device/docs/resource_calculator.md)
*   **สคริปต์คำนวณอัตโนมัติ (Python CLI):** [load_calculator.py](file:///c:/Users/natta/Documents/bar_program/esp32_eth01_artnet_device/tools/load_calculator.py)

---

## 2. ตารางเปรียบเทียบการเปลี่ยนแปลง (Comparison Matrix: v1.10.0 vs v1.11.0)

| ฟีเจอร์ / หัวข้อ | สถานะใน v1.10.0 (ตัวเก่า) | แผนพัฒนาใน v1.11.0 (ตัวใหม่) | อ้างอิงหัวข้อในไฟล์แผนงานดิบ | จุดที่ต้องแก้ไขในโค้ดหลัก |
| :--- | :--- | :--- | :--- | :--- |
| **WiZ Smart Bulbs** | 🟢 เปิดใช้งานผ่าน UDP mapped DMX | ❌ **ลบออกถาวร** (ถอนโค้ดและย้ายไปคุมผ่าน PC เช่น Node-RED/QLC+) เพื่อประหยัดขนาดแฟลชและ Heap RAM | [handover_v1.11.0.md: L704-L715](file:///c:/Users/natta/Documents/bar_program/esp32_eth01_artnet_device/handover/handover_v1.11.0.md#L704-L715) | ลบฟิลด์ใน [config.h](file:///c:/Users/natta/Documents/bar_program/esp32_eth01_artnet_device/include/config.h) และ [output_control.h](file:///c:/Users/natta/Documents/bar_program/esp32_eth01_artnet_device/include/output_control.h) |
| **Bluetooth / BLE** | ❌ ไม่มีการเปิดใช้งาน | ❌ **มติตัดออกถาวร** ป้องกันการชนกับความปลอดภัยของ OTA และป้องกันกระแสไฟกระชากตอนบูต | [handover_v1.11.0.md: L717-L728](file:///c:/Users/natta/Documents/bar_program/esp32_eth01_artnet_device/handover/handover_v1.11.0.md#L717-L728) | ไม่มีการนำมาใช้ในบิลด์ |
| **โครงสร้างสถาปัตยกรรม Pin** | ❌ ต่อตรงผ่าน ESP32 GPIO เท่านั้น | 🟢 **Unified Expander Layer** เพิ่มฟิลด์ `source` เพื่อรองรับ Virtual Pin ทั้งบน ESP32 และ I2C Expander เช่น PCA9685/MCP23017 | [handover_v1.11.0.md: L253-L284](file:///c:/Users/natta/Documents/bar_program/esp32_eth01_artnet_device/handover/handover_v1.11.0.md#L253-L284) และ [L561-L571](file:///c:/Users/natta/Documents/bar_program/esp32_eth01_artnet_device/handover/handover_v1.11.0.md#L561-L571) | แก้ไข `struct OutputChannel` ใน [output_control.h](file:///c:/Users/natta/Documents/bar_program/esp32_eth01_artnet_device/include/output_control.h) |
| **การใช้โหมดพินแบบผสม (Mixed-Mode)** | ❌ ไม่รองรับพินข้ามอุปกรณ์ | 🟢 **สลับพินอิสระ** เช่น Stepper ใช้พัลส์ความเร็วสูง (STEP) บน ESP32 และย้ายขาช้า (DIR/ENABLE) ไปลง I2C Expander | [handover_v1.11.0.md: L550-L559](file:///c:/Users/natta/Documents/bar_program/esp32_eth01_artnet_device/handover/handover_v1.11.0.md#L550-L559) | ปรับปรุงระบบตรวจสอบและแยก Driver ไดนามิก |
| **โหมดเครื่องทำควันสเตจ** | ❌ ต้องใช้ Relay แยก 2 ช่องคุมอิสระเอง | 🟢 **โหมด Sequential Smoke Shooter** ใช้ DMX 1 ช่องทริกเกอร์ลำดับเวลาเปิดควันรอสะสมตัวแล้วจึงสั่งยิงลมแบบอัตโนมัติ | [handover_v1.11.0.md: L440-L469](file:///c:/Users/natta/Documents/bar_program/esp32_eth01_artnet_device/handover/handover_v1.11.0.md#L440-L469) | เพิ่ม State Machine ใน [output_control.h](file:///c:/Users/natta/Documents/bar_program/esp32_eth01_artnet_device/include/output_control.h) |
| **~~มอเตอร์ดีซี Closed-Loop~~** | ~~❌ มีเฉพาะโหมด Open-Loop คุมแรงดันผ่าน PWM~~ | ❌ **ยกเลิกแผนพัฒนา** (เนื่องจากความซับซ้อนในการจูน PID และการใช้ซอฟต์แวร์เฉพาะตัว จึงเลือกใช้ไดรเวอร์ภายนอกที่รองรับ Step/Dir ในโหมด Stepper แทน) | [handover_v1.11.0.md: Section 9](file:///c:/Users/natta/Documents/bar_program/esp32_eth01_artnet_device/handover/handover_v1.11.0.md#L1037-L1107) | - |
| **หน้าจอ 7-Segment** | ❌ ไม่รองรับ | 🟢 **โหมด TM1637 (2 พิน) & โหมดไดเรกต์ 7-8 พิน** รองรับทั้งชิปสำเร็จรูป และการขับเซกเมนต์ตรง (CC/CA) ผ่านขาต่อเนื่อง | [handover_v1.11.0.md: L606-L633](file:///c:/Users/natta/Documents/bar_program/esp32_eth01_artnet_device/handover/handover_v1.11.0.md#L606-L633) และ [L633-L702](file:///c:/Users/natta/Documents/bar_program/esp32_eth01_artnet_device/handover/handover_v1.11.0.md#L633-L702) | เพิ่มโมดูล 7-Segment Font ในโฟลเดอร์ `include/` และโค้ดขับขา CC/CA |
| **การผสมสีหลอดไฟแอนะล็อก** | ❌ สร้างช่องหรี่ไฟแยกกัน 3-4 แถว | 🟢 **โหมด Analog RGB / RGBW** รวมศูนย์พินสีในสล็อตเดียว ผูก DMX ต่อเนื่องอัตโนมัติ และแสดง Color Picker บน Web UI | [handover_v1.11.0.md: L616-L631](file:///c:/Users/natta/Documents/bar_program/esp32_eth01_artnet_device/handover/handover_v1.11.0.md#L616-L631) | แก้ไขใน [web_pages.h](file:///c:/Users/natta/Documents/bar_program/esp32_eth01_artnet_device/include/web_pages.h) และ DMX mapping loop |
| **เสียงสัญญาณแจ้งเตือน** | ❌ ไม่รองรับ | 🟢 **Passive Buzzer** คุมย่านความถี่ (100Hz-5kHz) ร่วมกับระดับความดัง (Volume 0-50% Duty) ผ่านสัญญาณ DMX 2 ช่อง | [handover_v1.11.0.md: L729-L761](file:///c:/Users/natta/Documents/bar_program/esp32_eth01_artnet_device/handover/handover_v1.11.0.md#L729-L761) | เพิ่มโค้ดควบคุม LEDC tone generator |
| **ระบบรับอินพุตเซนเซอร์** | ❌ รองรับเฉพาะ Zero-Crossing input | ❌ **ยกเลิกแผนพัฒนา** (เนื่องจากข้อจำกัดขนาดแฟลชเมมโมรี่ในการทำ OTA อัปเกรดเฟิร์มแวร์) | [handover_v1.11.0.md: Section 8](file:///c:/Users/natta/Documents/bar_program/esp32_eth01_artnet_device/handover/handover_v1.11.0.md#L802-L907) | - |
| **โหลดเน็ตเวิร์กและการส่งกลับ** | ❌ มีเฉพาะการรับแพ็กเก็ตคอนโทรล | ❌ **ยกเลิกแผนพัฒนา** (เนื่องจากการยกเลิกระบบรับอินพุตเซนเซอร์) | [handover_v1.11.0.md: Section 8.5](file:///c:/Users/natta/Documents/bar_program/esp32_eth01_artnet_device/handover/handover_v1.11.0.md#L907-L947) | - |

---

## 3. รายละเอียดและข้อมูลคำนวณเฉพาะด้าน (Key Planning Insights)

### 3.1 การคำนวณพื้นที่แฟลชและเสถียรภาพระบบ (Flash Memory Size Calculations)
*   **ขนาดไฟล์ Bin ปัจจุบัน (v1.10.0):** `1.11 MB` (คิดเป็น 59.3% ของความจุแอปพลิเคชัน 1.90 MB)
*   **พื้นที่แฟลชว่างเหลือ:** **~780 KB** (เพียงพอสำหรับการสำรองข้อมูลขณะทำ OTA ปลอดภัยสูงมาก)
*   **ขนาดโค้ดรวมไลบรารีใหม่ที่จะเพิ่มเข้าใน v1.11.0:**
    *   *ToF Sensor (VL53L0X):* ~10 KB (ใช้ไลบรารีขนาดเบาของ Pololu)
    *   *Ultrasonic (HC-SR04 / A02YYUW):* <1 KB (คำนวณคลื่นพัลส์ตรง หรือดึงค่าผ่านซีเรียลอินเทอร์เฟซ UART1 ในตัว)
    *   *Analog Mic (MAX4466):* <1 KB (ดึง API ตรงจาก ADC)
    *   *Modbus TCP/RTU:* ~15 KB (ใช้ไลบรารี `ModbusMaster` และ Ethernet socket ในตัว)
    *   *ระบบเซนเซอร์รวม:* ใช้พื้นที่แฟลชเพิ่มรวมกันประมาณ **15 KB - 50 KB** เท่านั้น (เหลือพื้นที่ว่างอีกกว่า 730 KB สำหรับอนาคต)

### 3.2 สถาปัตยกรรมบัส I2C ร่วมกัน (I2C Bus Coexistence Architecture)
พอร์ตเชื่อมต่อบัส I2C (SDA/SCL) สามารถพ่วงอุปกรณ์ร่วมกันได้ทั้งหมดโดยไม่มีการรบกวนสัญญาณ เนื่องจากทุกตัวใช้ Hardware Address ต่างกันอย่างสิ้นเชิง:
*   **บัส SDA/SCL ร่วมกัน:** กำหนดให้ใช้พินปลอดภัย **SDA = GPIO14** และ **SCL = GPIO15** ของบอร์ด WT32-ETH01
*   **การกระจายแอดเดรสบนบัส:**
    *   `0x20` $\rightarrow$ MCP23017 หรือ TCA9555 (พอร์ตอินพุต/เอาต์พุตดิจิทัลรีเลย์)
    *   `0x29` $\rightarrow$ ToF VL53L0X (เซนเซอร์ระยะทางเลเซอร์)
    *   `0x40` $\rightarrow$ PCA9685 (พอร์ตเซอร์โวและเอาต์พุตดีมเมอร์ PWM)
*   *ข้อควรระวัง:* หากใช้เซนเซอร์ ToF (VL53L0X) มากกว่า 1 ตัว แอดเดรสที่เหมือนกันจะชนกัน ต้องต่อพินขยายเข้าไปที่ขา `XSHUT` ของแต่ละโมดูล เพื่อให้ซอฟต์แวร์สามารถสั่งเปิดบูตบอร์ดทีละตัวและทำการเปลี่ยนแอดเดรสชั่วคราวเป็นแอดเดรสอื่น (เช่น `0x30`, `0x31`, ...) ในช่วงเริ่มต้น setup ระบบ หรือเลือกใช้ชิปสวิตช์บัส I2C (TCA9548A)
*   *อินเทอร์เฟซแยกสำหรับ A02YYUW:* เนื่องจากตัวเซนเซอร์ A02YYUW เป็นแบบสายอนุกรม (UART) จึงแยกสายสัญญาณออกจากบัส I2C ไปต่อเข้าพินอินพุตตรง (เช่น GPIO34 RX1) และเชื่อมต่อผ่านช่องสัญญาณซีเรียล UART1 ในตัวชิป ESP32

### 3.3 การคำนวณและประเมินโหลดของเน็ตเวิร์ก (Network Load Optimization Rules)
ระบบส่งข้อมูลตอบสนองระยะและพื้นที่ (Sensor Feedback) ต้องไม่ไปเพิ่มทราฟฟิกจนเครือข่าย WiFi สะดุดหรือทำให้หน่วยความจำบอร์ดทำงานหนักเกินไป:
1.  **Rate Limiting:** จำกัดความถี่การส่งสตรีมข้อมูล ToF/Ultrasonic (HC-SR04 และ A02YYUW) กลับไปยัง PC ที่อัตรา **30 Hz ถึง 50 Hz** เท่านั้น เพื่อให้สอดคล้องกับอัตราเฟรมเรตการแสดงผลของโปรแกรม Visual (30fps/60fps) และความสามารถในการวัดระยะทางจริงของชิป ToF ซึ่งเพียงพอสำหรับระบบโต้ตอบที่ลื่นไหลระดับสิบมิลลิวินาที
2.  **Audio Compression:** ไม่สตรีมคลื่นเสียงแอนะล็อกดิบผ่านเน็ตเวิร์ก ให้ ESP32 คำนวณหาค่าความดังเฉลี่ย (RMS Peak) แล้วยิงค่าระดับความดัง 1 ช่อง (8-bit) ที่ความถี่ 45Hz แทน
3.  **Report-on-Change:** ปุ่มกดหรือเซนเซอร์ประเภททริกเกอร์สถานะ (PIR/ม่านแสง) จะส่งข้อมูลเฉพาะเมื่อเปลี่ยนสถานะเท่านั้น พร้อมระบบ Debounce ป้องกันข้อมูลรบกวน
4.  **Target IP Unicast:** บังคับให้ตั้งค่า IP ของเป้าหมาย (TouchDesigner Host PC) บนหน้า Web UI เพื่อให้บอร์ดส่งข้อมูลตรงไปยังเครื่องปลายทาง ไม่ใช้ Broadcast (`255.255.255.255`) ซึ่งเป็นสาเหตุให้สัญญาณ WiFi หน่วงและรบกวนบอร์ดอื่น
5.  **Task Pinning:** บังคับให้การวนลูปการอ่านค่าฮาร์ดแวร์เซนเซอร์ (I2C/ADC/Pulse) ไปประมวลผลอยู่บน **CPU Core 1** และแยกการประมวลผลเครือข่ายและการรับสัญญาณไฟเอาต์พุตหลักให้อยู่บน **CPU Core 0** เพื่อป้องกันไม่ให้การหน่วงเวลาการประมวลผลของเซนเซอร์ไปทำให้จังหวะการเปิด/ปิดไฟสเตจเกิดอาการกระตุก

### 3.4 การประยุกต์ใช้โหมด Stepper กับ AC Servo (AC Servo Application)
*   **อินเทอร์เฟซควบคุม:** AC Servo Driver ทั่วไปใช้อินเทอร์เฟซแบบ Step/Dir (Pulse/Direction) ซึ่งสามารถทำงานร่วมกับโหมด Stepper (FastAccelStepper) ได้ทันที
*   **การเลือกจำนวนไบต์พิกัดตำแหน่ง:** 
    *   **แนะนำให้เลือกโหมด 32-bit (4 ไบต์) สำหรับช่องสัญญาณเป้าหมายตำแหน่ง (Target Position)** 
    *   *เหตุผล:* เพื่อป้องกันการหมุนพิกัดตำแหน่งล้น (Position Overflow) เนื่องจากมอเตอร์ AC Servo มีความเร็วรอบสูง (3000 RPM) และมีความละเอียดสูง (เช่น ตั้งค่ารับพัลส์ที่ 10,000 pulses/rev) ทำให้ได้ช่วงพิกัดตำแหน่งวิ่งสูงสุดถึง 429,496 รอบ
*   **แผนผังแชนเนล DMX ที่เหมาะสม (7 ช่องสัญญาณต่อมอเตอร์):**
    *   `DMX 1-4` (4 ไบต์) $\rightarrow$ พิกัดเป้าหมาย 32-bit (Target Position)
    *   `DMX 5-6` (2 ไบต์) $\rightarrow$ สปีดความเร็วพัลส์ 16-bit (Travel Speed)
    *   `DMX 7` (1 ไบต์) $\rightarrow$ คำสั่งทริกควบคุมระบบ (Command: Run, Stop, Home, Reset, Disable)
*   **ข้อแนะนำเรื่องสปีด:** ESP32 ขับพัลส์ได้เสถียรที่ ~200kHz-300kHz เพื่อให้ไดรฟ์เซอร์โวหมุนสปีดสูงสุดได้ที่ 3000 RPM แนะนำให้สเกลตั้งเฟืองอิเล็กทรอนิกส์ (Electronic Gear Ratio) ในตัวไดรฟ์เวอร์เซอร์โวให้อยู่ในช่วง **4,000 - 5,000 pulses/rev**
*   **ระบบตัวคูณอัตราทด (Software Scale Factor):** รองรับการป้อนตัวคูณทศนิยม (เช่น `Scale Factor = 80.0`) เพื่อแปลงค่า DMX ให้จ่ายพัลส์ทวีคูณออกไป เหมาะสำหรับงานเกียร์ทดรอบสูง หรือการแปลงหน่วย DMX เป็นระยะทางความยาวทางกายภาพมิลลิเมตร (mm) ตรงตัวเป๊ะ เช่น ตั้งตัวคูณเป็น 80.0 เมื่อผู้ใช้ออกคำสั่ง DMX = 150 บอร์ดจะหมุน 12,000 steps เลื่อนแท่นสไลด์ไป 150 mm ทันทีโดยไม่ต้องคำนวณซับซ้อนในโปรแกรมต้นทาง

### 3.5 การรองรับแถบไฟ LED Strip หลากหลายรุ่น (LED Strip Multi-Chip & Protocol Compatibility)
เพื่อตอบสนองการใช้งานร่วมกับไฟ LED Strip หลายประเภทในตลาด (เช่น WS2811, WS2812B, WS2813, WS2815, SK6812, GS1903) ในแผนออกแบบ v1.11.0 มีมาตรการการรองรับและกำหนดโครงสร้างดังนี้:

1. **การจำแนกตามโปรโตคอลและระดับแรงดัน (Physical & Signal Specifications):**
   *   **กลุ่ม 800kHz (Standard High-Speed):** ครอบคลุม **WS2812B, WS2813, WS2815, SK6812, GS1903** ซึ่งใช้การสื่อสารความเร็วเดียวกัน สามารถใช้สัญญาณขับ `NeoEsp32RmtXWs2812xMethod` ของไลบรารี NeoPixelBus ขับได้ทันที
   *   **กลุ่ม 400kHz (Low-Speed):** ครอบคลุม **WS2811 รุ่นเก่า** (โดยเฉพาะแบบไฟ 12V/24V บางรุ่น) ซึ่งต้องการความกว้างของพัลส์ที่ยาวกว่า ในอนาคตควรเพิ่มตัวเลือกโปรโตคอล `LED_CHIP_TYPE` เพื่อให้ซอฟต์แวร์สลับไปใช้ `NeoEsp32RmtXWs2811Method` ได้
   *   **กลุ่มไฟ RGBW (4 แชนเนล):** ครอบคลุม **SK6812 RGBW** ใช้โครงสร้างหน่วยความจำ 4 ไบต์ต่อพิกเซล และรองรับแอดเดรสสีขาวแยกต่างหาก

2. **การสลับลำดับสี (Color Order Support):**
   แต่ละเบอร์ของ LED Strip มีการเรียงลำดับพิกเซลสีภายในชิปต่างกัน จึงต้องรองรับการกำหนดผ่าน Web UI อย่างละเอียด:
   *   `GRB`: เป็นค่าเริ่มต้นสำหรับ WS2812B, WS2813, WS2815, SK6812 RGB
   *   `RGB`: นิยมใช้ใน WS2811 บางแบรนด์
   *   `BRG` หรือ `RBG`: นิยมใช้ในชิปสัญชาติจีน เช่น GS1903, UCS1903, SM16703
   *   `RGBW` / `GRBW` / `BRGW` / `WRGB`: สำหรับไฟประเภท RGBW (SK6812)

3. **ข้อควรระวังในการจ่ายไฟและเชื่อมต่อสัญญาณ:**
   *   **ไฟเลี้ยง (Power):** WS2812B/SK6812 ใช้ไฟ 5V, WS2811 ใช้ไฟ 12V, WS2815 ใช้ไฟ 12V (มีสาย Backup data) ต้องต่อกราวด์ (GND) ร่วมกับบอร์ด WT32-ETH01 เสมอ
   *   **ระดับสัญญาณลอจิก (Logic Level):** ชิป LED Strip ส่วนใหญ่ต้องการลอจิก Data ที่ 5V แต่ ESP32 ปล่อยสัญญาณที่ 3.3V แม้การต่อตรงอาจทำงานได้ในระยะสั้น แต่เพื่อความเสถียรในงานติดตั้งจริง **จำเป็นต้องใช้ไอซี Level Shifter (เช่น 74AHCT125 หรือ TXS0108E)** แปลงสัญญาณ 3.3V เป็น 5V ก่อนเข้าสาย Data ของไฟเส้น เพื่อป้องกันปัญหาไฟกระพริบเมื่อเดินสายสัญญาณยาวเกิน 1-2 เมตร
   *   **ขารองรับการสำรองข้อมูล (Backup Data Pin):** ไฟอย่าง WS2813 และ WS2815 จะมีสายสัญญาณ 4 เส้น (V+, GND, DI, BI) โดยขา BI (Backup Input) ให้ต่อลงกราวด์ (GND) ของจุดเริ่มต้นไฟ เพื่อให้สัญญาณ DI ทำงานเป็นหลัก

---

## 4. แผนงานขั้นตอนถัดไป (Next Steps Timeline)

เมื่อได้รับอนุมัติแผนงานออกแบบในเวอร์ชัน v1.11.0 นี้แล้ว จะเริ่มดำเนินการดังนี้:
1.  **สร้างและปรับปรุงโครงสร้างข้อมูลอินพุต/เอาต์พุต:**
    *   อัปเดตโมเดลพินผสมใน `struct OutputChannel` และเพิ่มเมนู Expanders บันทึก I2C pins ลงบน NVS
2.  **เขียนระบบขับอินพุตเซนเซอร์:**
    *   สร้างไฟล์ไลบรารีตัวแปรเบาสำหรับอ่าน ToF VL53L0X, สัญญาณ Echo ของ HC-SR04, และการอ่าน ADC ของไมโครโฟน
3.  **เขียนโมดูลการส่งกลับเครือข่าย (Feedback Module):**
    *   สร้างระบบแปลงสัดส่วนข้อมูลส่งออกผ่าน UDP Art-Net (ArtDmx) และรูปแบบ OSC
4.  **ปรับปรุงหน้าเว็บอินเทอร์เฟส (Web UI):**
    *   เพิ่มสล็อตเมนูตั้งค่าสำหรับอินพุตเซนเซอร์และตัวแปลงความถี่ของ I2C บัส
5.  **คอมไพล์ประเมินผลขนาดและจัดทำ Walkthrough การทดสอบ**
6.  **วางแผนศึกษาและเตรียมโครงสร้างการพัฒนาหน้าจอแสดงสถานะ Shared I2C Diagnostics Display (OLED & Character LCD 16x2/20x4) ในเฟสถัดไป (ระดับความสำคัญ: ต่ำมาก / Low Priority)**


