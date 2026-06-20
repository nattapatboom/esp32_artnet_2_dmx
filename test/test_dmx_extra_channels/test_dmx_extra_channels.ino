/*
 * test_dmx_extra_channels.ino
 * ─────────────────────────────────────────────────────────────────────────────
 * Test: DMX Output Channels 3 & 4 via Hardware UART GPIO Remapping
 *
 * CONCLUSION FROM RESEARCH:
 *   SoftwareSerial at 250kbaud is NOT reliable for DMX on ESP32.
 *   Bit time = 4µs → FreeRTOS/WiFi interrupt jitter causes framing errors.
 *
 * CORRECT APPROACH (tested here):
 *   Use esp_dmx library which uses:
 *     - Hardware UART (UART1 or UART2) for 250kbaud data
 *     - RMT peripheral for precise break/MAB timing
 *     - GPIO Matrix to remap TX pin to any available GPIO
 *
 * TEST SETUP:
 *   DMX Ch 3 → UART1 remapped to GPIO 32
 *   DMX Ch 4 → UART2 remapped to GPIO 33
 *   Verify with logic analyzer: Break ≥88µs, MAB ≥8µs, data at 250kbaud 8N2
 *
 * HOW TO USE:
 *   1. Create a new PlatformIO project or sketch
 *   2. Add dependency: someweisguy/esp_dmx @ ^3.1.0
 *   3. Flash this sketch
 *   4. Monitor Serial at 115200 for results
 *   5. Connect logic analyzer to GPIO 32 and GPIO 33
 *
 * IMPORTANT: Do NOT use GPIO 16 (LAN8720 power control on WT32-ETH01)
 * ─────────────────────────────────────────────────────────────────────────────
 */

#include <Arduino.h>
#include <esp_dmx.h>

// ─── GPIO Pin Assignments for Test ───────────────────────────────────────────
// Channels 1 & 2 normally use UART1=GPIO17, UART2=GPIO17 — both hardware
// Channels 3 & 4 test: remap UART1 and UART2 to free GPIOs
#define DMX_CH3_TX_PIN  32   // GPIO 32 — free on WT32-ETH01
#define DMX_CH4_TX_PIN  33   // GPIO 33 — free on WT32-ETH01

// DMX ports (esp_dmx uses DMX_NUM_0, DMX_NUM_1, DMX_NUM_2)
// UART0 = debug serial — DO NOT use for DMX in production
// Here we test UART1 and UART2 with remapped pins
#define DMX_PORT_CH3  DMX_NUM_1   // UART1 remapped to GPIO 32
#define DMX_PORT_CH4  DMX_NUM_2   // UART2 remapped to GPIO 33

// ─── Test DMX Data ────────────────────────────────────────────────────────────
uint8_t dmxDataCh3[DMX_PACKET_SIZE] = {0};  // Channel 3 output buffer
uint8_t dmxDataCh4[DMX_PACKET_SIZE] = {0};  // Channel 4 output buffer

// ─── Timing Test Variables ────────────────────────────────────────────────────
static unsigned long lastToggle = 0;
static uint8_t testValue = 0;
static bool ch3_ok = false;
static bool ch4_ok = false;

// ─── Setup ───────────────────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("\n=== DMX Extra Channel Test (GPIO Remap via esp_dmx) ===");
    Serial.println("Testing DMX channels 3 & 4 via HW UART GPIO Matrix remapping");
    Serial.println("Logic analyzer target: GPIO 32 (CH3), GPIO 33 (CH4)");
    Serial.println("Expected: Break ≥88µs LOW, MAB ≥8µs HIGH, data 250kbaud 8N2\n");

    // ─── Initialize DMX Port for Channel 3 (UART1 → GPIO 32) ────────────────
    dmx_config_t config = DMX_CONFIG_DEFAULT;

    // Install UART1 driver and remap TX → GPIO 32, no RX, no RTS
    esp_err_t err3 = dmx_driver_install(DMX_PORT_CH3, &config, DMX_INTR_FLAGS_DEFAULT);
    if (err3 == ESP_OK) {
        dmx_set_pin(DMX_PORT_CH3, DMX_CH3_TX_PIN, -1, -1);
        ch3_ok = true;
        Serial.printf("✅ DMX CH3 initialized: UART%d → GPIO%d\n", DMX_PORT_CH3, DMX_CH3_TX_PIN);
    } else {
        Serial.printf("❌ DMX CH3 FAILED (UART%d): %s\n", DMX_PORT_CH3, esp_err_to_name(err3));
    }

    // ─── Initialize DMX Port for Channel 4 (UART2 → GPIO 33) ────────────────
    esp_err_t err4 = dmx_driver_install(DMX_PORT_CH4, &config, DMX_INTR_FLAGS_DEFAULT);
    if (err4 == ESP_OK) {
        dmx_set_pin(DMX_PORT_CH4, DMX_CH4_TX_PIN, -1, -1);
        ch4_ok = true;
        Serial.printf("✅ DMX CH4 initialized: UART%d → GPIO%d\n", DMX_PORT_CH4, DMX_CH4_TX_PIN);
    } else {
        Serial.printf("❌ DMX CH4 FAILED (UART%d): %s\n", DMX_PORT_CH4, esp_err_to_name(err4));
    }

    Serial.println("\n--- Sending DMX frames. Monitor GPIO 32 & 33 ---");
    Serial.println("Pattern: CH3 = ramp up, CH4 = ramp down (inverse)");
}

// ─── Loop ─────────────────────────────────────────────────────────────────────
void loop() {
    // Update test values every 100ms (10 Hz — easy to observe)
    if (millis() - lastToggle >= 100) {
        lastToggle = millis();
        testValue++;   // 0→255 cycle

        // CH3: ramp UP — first 3 addresses
        if (ch3_ok) {
            dmxDataCh3[0] = 0;            // Start code = 0
            dmxDataCh3[1] = testValue;    // Addr 1
            dmxDataCh3[2] = testValue;    // Addr 2
            dmxDataCh3[3] = testValue;    // Addr 3

            dmx_write(DMX_PORT_CH3, dmxDataCh3, DMX_PACKET_SIZE);
            dmx_send(DMX_PORT_CH3, DMX_PACKET_SIZE);
        }

        // CH4: ramp DOWN (inverse) — first 3 addresses
        if (ch4_ok) {
            uint8_t inv = 255 - testValue;
            dmxDataCh4[0] = 0;            // Start code = 0
            dmxDataCh4[1] = inv;          // Addr 1
            dmxDataCh4[2] = inv;          // Addr 2
            dmxDataCh4[3] = inv;          // Addr 3

            dmx_write(DMX_PORT_CH4, dmxDataCh4, DMX_PACKET_SIZE);
            dmx_send(DMX_PORT_CH4, DMX_PACKET_SIZE);
        }

        // Print status every 25 ticks (~2.5s)
        if (testValue % 25 == 0) {
            Serial.printf("[tick %3d] CH3(GPIO%d)=%3d  CH4(GPIO%d)=%3d\n",
                testValue,
                DMX_CH3_TX_PIN, testValue,
                DMX_CH4_TX_PIN, 255 - testValue);
        }
    }

    // Wait for DMX send to complete before next write
    if (ch3_ok) dmx_wait_sent(DMX_PORT_CH3, DMX_TIMEOUT_DEFAULT);
    if (ch4_ok) dmx_wait_sent(DMX_PORT_CH4, DMX_TIMEOUT_DEFAULT);

    delay(1);
}

/*
 * ─── EXPECTED RESULTS ────────────────────────────────────────────────────────
 *
 * ✅ SUCCESS CRITERIA (verify with logic analyzer):
 *   GPIO 32 (CH3):
 *     - Break: ≥88µs LOW at start of each frame
 *     - MAB: ≥8µs HIGH after break
 *     - Data: 250kbaud 8N2 (start bit + 8 data + 2 stop bits)
 *     - Frame rate: ~44fps (DMX max) or constrained by dmx_wait_sent()
 *
 *   GPIO 33 (CH4):
 *     - Same protocol, inverse data values
 *
 * ❌ FAILURE CRITERIA:
 *   - esp_err != ESP_OK on driver install → UART already in use or invalid port
 *   - Framing errors → incorrect baud rate or break timing
 *   - GPIO pin shows no activity → GPIO Matrix remap failed
 *
 * ─── INTEGRATION INTO MAIN PROJECT ──────────────────────────────────────────
 *
 * If this test passes, the approach to integrate into output_control.h:
 *   1. Raise MAX_DMX_CHANNELS from 2 to 4 (or more, if hardware permits)
 *   2. Allow ch 3→4 to use UART1/UART2 with different GPIO pins
 *   3. Note: UART0 is NOT available in production (used for Serial debug)
 *      In production builds, UART0 could theoretically be freed if Serial
 *      debug is disabled, giving a 3rd DMX channel.
 *
 * ─── UART AVAILABILITY SUMMARY ───────────────────────────────────────────────
 *
 *   UART | Port ID | Production Use | DMX Possible?
 *   -----|---------|---------------|----------------
 *   0    | UART0   | Serial debug  | ⚠️ Only if debug disabled
 *   1    | UART1   | Free          | ✅ Remap to any GPIO
 *   2    | UART2   | Free          | ✅ Remap to any GPIO
 *
 *   CONCLUSION: Max 2 reliable DMX channels (UART1 + UART2) with GPIO remap.
 *   For 3-4 channels: use additional ESP32 board via ESP-NOW, or SC16IS742.
 */
