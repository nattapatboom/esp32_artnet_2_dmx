/*
 * test_softserial_dmx_1ch.ino
 * ─────────────────────────────────────────────────────────────────────────────
 * Test: Can SoftwareSerial handle 1 DMX channel at 250kbaud on ESP32?
 *
 * DMX512 Requirements:
 *   - Baud rate: 250,000 bps (bit time = 4µs)
 *   - Format: 8N2 (8 data bits, no parity, 2 stop bits)
 *   - Break: ≥88µs LOW before each frame
 *   - MAB (Mark After Break): ≥8µs HIGH after break
 *   - Idle: HIGH (marking)
 *
 * Approach A: EspSoftwareSerial library (Dirk Sarodnick)
 *   - Software interrupt-based bit-banging
 *   - Risk: FreeRTOS + WiFi ISR may cause jitter at 4µs bit time
 *
 * Approach B: Manual bit-bang with esp_timer (µs precision HW timer)
 *   - Manually drives GPIO for each bit using esp_timer_get_time()
 *   - More reliable than interrupt-based SoftSerial
 *   - Similar to what RMT peripheral does internally
 *
 * SETUP:
 *   - Test GPIO: 32 (free on WT32-ETH01)
 *   - Verify with logic analyzer at GPIO 32
 *   - Monitor Serial 115200 for pass/fail logs
 *
 * PLATFORMIO DEPENDENCY (add to platformio.ini for Approach A):
 *   lib_deps =
 *     plerup/EspSoftwareSerial @ ^8.2.0
 *
 * WARNING: Do NOT use GPIO 16 (LAN8720A power pin on WT32-ETH01)
 * ─────────────────────────────────────────────────────────────────────────────
 */

#include <Arduino.h>

// ─── Test Config ─────────────────────────────────────────────────────────────
#define TEST_GPIO     32      // Output GPIO (free on WT32-ETH01)
#define TEST_APPROACH  2      // 1 = EspSoftwareSerial, 2 = Manual bit-bang

// How many DMX addresses to fill with test data (1-512)
#define DMX_ADDR_COUNT  3

// ─── Test Data ────────────────────────────────────────────────────────────────
static uint8_t dmxData[513] = {0};  // dmxData[0] = start code, [1..512] = data

// ─────────────────────────────────────────────────────────────────────────────
// APPROACH A: EspSoftwareSerial
// ─────────────────────────────────────────────────────────────────────────────
#if TEST_APPROACH == 1

// Uncomment after adding plerup/EspSoftwareSerial to platformio.ini
// #include <SoftwareSerial.h>

// SoftwareSerial dmxSerial(-1, TEST_GPIO, false);  // RX=-1 (none), TX=GPIO32

void setupApproachA() {
    Serial.println("[Approach A] EspSoftwareSerial @ 250000 baud on GPIO " + String(TEST_GPIO));
    // dmxSerial.begin(250000, SWSERIAL_8N2);
    // Manually drive break (GPIO LOW ≥88µs) then MAB (HIGH ≥8µs)
    pinMode(TEST_GPIO, OUTPUT);
    digitalWrite(TEST_GPIO, HIGH);  // Idle HIGH (DMX idle = marking)
    Serial.println("[Approach A] UNCOMMENT library lines and rebuild to activate.");
    Serial.println("[Approach A] Currently testing pin drive only.");
}

void sendDmxApproachA(uint8_t* data, int len) {
    // 1. Break: pull LOW for ≥88µs (use 100µs for margin)
    digitalWrite(TEST_GPIO, LOW);
    delayMicroseconds(100);

    // 2. MAB: pull HIGH for ≥8µs (use 12µs for margin)
    digitalWrite(TEST_GPIO, HIGH);
    delayMicroseconds(12);

    // 3. Send data via SoftwareSerial
    // dmxSerial.write(data, len);
    // Without library: crude approximation (NOT accurate - for structure test only)
    for (int i = 0; i < len; i++) {
        // Start bit (LOW, 4µs)
        digitalWrite(TEST_GPIO, LOW);
        delayMicroseconds(4);
        // 8 data bits
        for (int b = 0; b < 8; b++) {
            digitalWrite(TEST_GPIO, (data[i] >> b) & 1);
            delayMicroseconds(4);
        }
        // 2 stop bits (HIGH, 8µs)
        digitalWrite(TEST_GPIO, HIGH);
        delayMicroseconds(8);
    }
}

#endif  // APPROACH A

// ─────────────────────────────────────────────────────────────────────────────
// APPROACH B: Manual bit-bang with esp_timer (hardware µs timer)
// ─────────────────────────────────────────────────────────────────────────────
#if TEST_APPROACH == 2

// Precise busy-wait using hardware timer (avoids delayMicroseconds() drift)
static inline void waitUntil(uint64_t target) {
    while (esp_timer_get_time() < target) { /* busy-wait */ }
}

void sendDmxBitBang(uint8_t* data, int len) {
    const uint32_t BIT_US = 4;   // 4µs per bit at 250kbaud

    // ── 1. Break: LOW for 100µs ───────────────────────────────────────────
    GPIO.out_w1tc = (1UL << TEST_GPIO);  // Fast GPIO clear (atomic)
    uint64_t t = esp_timer_get_time();
    waitUntil(t + 100);

    // ── 2. MAB: HIGH for 12µs ────────────────────────────────────────────
    GPIO.out_w1ts = (1UL << TEST_GPIO);  // Fast GPIO set (atomic)
    t = esp_timer_get_time();
    waitUntil(t + 12);

    // ── 3. Data bytes ─────────────────────────────────────────────────────
    for (int i = 0; i < len; i++) {
        t = esp_timer_get_time();

        // Start bit: LOW
        GPIO.out_w1tc = (1UL << TEST_GPIO);
        waitUntil(t += BIT_US);

        // 8 data bits (LSB first)
        for (int b = 0; b < 8; b++) {
            if ((data[i] >> b) & 1)
                GPIO.out_w1ts = (1UL << TEST_GPIO);
            else
                GPIO.out_w1tc = (1UL << TEST_GPIO);
            waitUntil(t += BIT_US);
        }

        // 2 stop bits: HIGH (8µs total)
        GPIO.out_w1ts = (1UL << TEST_GPIO);
        waitUntil(t += BIT_US * 2);
    }
}

void setupApproachB() {
    Serial.println("[Approach B] Manual bit-bang with esp_timer on GPIO " + String(TEST_GPIO));
    gpio_pad_select_gpio(TEST_GPIO);
    gpio_set_direction((gpio_num_t)TEST_GPIO, GPIO_MODE_OUTPUT);
    GPIO.out_w1ts = (1UL << TEST_GPIO);  // Start HIGH (DMX idle = marking)
    Serial.println("[Approach B] Ready. Sending DMX frames...");
}

#endif  // APPROACH B

// ─────────────────────────────────────────────────────────────────────────────
// Timing measurement helper
// ─────────────────────────────────────────────────────────────────────────────
void measureFrameTiming() {
    // Measure how long a full DMX frame takes
    // Theoretical: Break(100µs) + MAB(12µs) + 513 bytes × (start+8data+2stop)×4µs
    //            = 100 + 12 + (513 × 11 × 4) = 100 + 12 + 22572 = 22684µs ≈ 22.7ms
    // Max frame rate ≈ 44fps (DMX standard max)
    const float BYTE_US = 11.0f * 4.0f;  // 11 bits × 4µs/bit = 44µs/byte
    const float FRAME_US = 100.0f + 12.0f + (513.0f * BYTE_US);
    Serial.printf("Theoretical frame time: %.1f µs = %.2f ms = %.1f fps max\n",
        FRAME_US, FRAME_US / 1000.0f, 1000000.0f / FRAME_US);
}

// ─────────────────────────────────────────────────────────────────────────────
// Setup
// ─────────────────────────────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println("\n=== SoftwareSerial / BitBang DMX 1-Channel Test ===");
    Serial.printf("GPIO: %d  |  Approach: %d  |  Addresses: %d\n",
        TEST_GPIO, TEST_APPROACH, DMX_ADDR_COUNT);
    Serial.println("Connect logic analyzer to GPIO " + String(TEST_GPIO));
    Serial.println("Expected: Break ≥88µs LOW, MAB ≥8µs HIGH, 250kbaud 8N2\n");

    measureFrameTiming();

#if TEST_APPROACH == 1
    setupApproachA();
#elif TEST_APPROACH == 2
    setupApproachB();
#endif
}

// ─────────────────────────────────────────────────────────────────────────────
// Loop — send DMX and report frame timing
// ─────────────────────────────────────────────────────────────────────────────
static uint8_t testValue = 0;
static unsigned long lastReport = 0;
static uint32_t frameCount = 0;
static uint64_t totalFrameUs = 0;

void loop() {
    // Update test data (ramp pattern)
    testValue++;
    dmxData[0] = 0;  // Start code
    for (int i = 1; i <= DMX_ADDR_COUNT; i++) {
        dmxData[i] = testValue + (i * 30);  // Offset each channel
    }

    // Send frame and measure time
    uint64_t t0 = esp_timer_get_time();

#if TEST_APPROACH == 1
    sendDmxApproachA(dmxData, 1 + DMX_ADDR_COUNT);
#elif TEST_APPROACH == 2
    // Disable interrupts during critical bit-bang section for max accuracy
    portDISABLE_INTERRUPTS();
    sendDmxBitBang(dmxData, 1 + DMX_ADDR_COUNT);
    portENABLE_INTERRUPTS();
#endif

    uint64_t elapsed = esp_timer_get_time() - t0;
    frameCount++;
    totalFrameUs += elapsed;

    // Report every 5 seconds
    if (millis() - lastReport >= 5000) {
        lastReport = millis();
        uint64_t avgUs = totalFrameUs / frameCount;
        float fps = 1000000.0f / avgUs;
        Serial.printf("[Approach %d] Frames: %u | Avg frame: %llu µs | FPS: %.1f | Val: %d\n",
            TEST_APPROACH, frameCount, avgUs, fps, testValue);
        frameCount = 0;
        totalFrameUs = 0;
    }

    // ~40fps target (25ms per frame)
    delay(20);
}

/*
 * ─── EXPECTED RESULTS ────────────────────────────────────────────────────────
 *
 * Approach B (Bit-bang + portDISABLE_INTERRUPTS):
 *   ✅ LIKELY PASS for 1 channel, low address count
 *   - Frame time ~22.7ms for 512 addr, less for fewer addresses
 *   - portDISABLE_INTERRUPTS() prevents FreeRTOS preemption during frame
 *   ⚠️ RISK: Disabling interrupts blocks WiFi/ETH stack during frame
 *            With 3 addresses only: ~0.13ms blocked → acceptable
 *            With 512 addresses: ~22ms blocked → WiFi will drop packets!
 *
 * Approach A (EspSoftwareSerial):
 *   ⚠️ UNCERTAIN — interrupt-driven at 250kbaud
 *   - May work for slow/tolerant DMX fixtures
 *   - Will fail for strict DMX fixtures
 *
 * ─── INTEGRATION FEASIBILITY ─────────────────────────────────────────────────
 *
 * IF Approach B passes with limited addresses (≤16):
 *   → Can be used as "Soft DMX" fallback for up to 16 fixture addresses
 *   → Must send full 512-byte frame per DMX spec (even if only 16 used)
 *   → WiFi interrupt block = 22.7ms per frame = significant network impact
 *   → NOT recommended for Art-Net mode (needs low latency network RX)
 *   → MAY be acceptable for standalone/failsafe output only
 *
 * CONCLUSION:
 *   HardwareSerial GPIO remap (UART1/UART2) = always better
 *   SoftSerial / BitBang = emergency fallback only, with trade-offs
 *
 * ─── LOGIC ANALYZER CHECK ────────────────────────────────────────────────────
 *
 *   1. Break: should see ≥88µs LOW pulse
 *   2. MAB:   should see ≥8µs HIGH after break
 *   3. Start byte (0x00): start-bit LOW, 8 bits, 2 stop HIGH
 *   4. Each bit = exactly 4µs (±1µs tolerance for most fixtures)
 *
 * ─── PASS CRITERIA ───────────────────────────────────────────────────────────
 *   [ ] Break ≥ 88µs on logic analyzer
 *   [ ] Bit timing within 4µs ± 1µs
 *   [ ] DMX fixture responds to values
 *   [ ] No framing errors reported by analyzer
 */
