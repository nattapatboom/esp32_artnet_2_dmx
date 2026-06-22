#ifndef GPIO_CONTROL_H
#define GPIO_CONTROL_H

#include <Arduino.h>
#include "output_defs.h"

// ─────────────────────────────────────────────
//  GPIO Control
//  Single source of truth for available GPIOs,
//  reserved pins, input-only pins, and simple
//  pin validation helpers.
//  Mirrors web/js/_gpio.js data.
// ─────────────────────────────────────────────

namespace GpioControl {

// ── Available GPIOs on WT32-ETH01 ──
constexpr uint8_t OUTPUT_GPIO_PINS[] = {4, 12, 14, 15, 2, 17, 32, 33};
constexpr uint8_t OUTPUT_GPIO_COUNT = 8;

constexpr uint8_t INPUT_GPIO_PINS[] = {36, 39, 34, 35, 32, 33};
constexpr uint8_t INPUT_GPIO_COUNT = 6;

// Input-only pins (ESP32 hardware limit — no output driver)
constexpr uint8_t INPUT_ONLY_PINS[] = {34, 35, 36, 39};
constexpr uint8_t INPUT_ONLY_COUNT = 4;

// ── Ethernet RMII / PHY reserved pins ──
struct ReservedPinInfo {
    uint8_t pin;
    const char* reason;
};

constexpr ReservedPinInfo RESERVED_ETHERNET_PINS[] = {
    {0,  "Ethernet RMII clock / BOOT"},
    {16, "Ethernet PHY power"},
    {18, "Ethernet RMII MDIO"},
    {19, "Ethernet RMII TXD0"},
    {21, "Ethernet RMII TX_EN"},
    {22, "Ethernet RMII TXD1"},
    {23, "Ethernet RMII MDC"},
    {25, "Ethernet RMII RXD0"},
    {26, "Ethernet RMII RXD1"},
    {27, "Ethernet RMII CRS_DV"}
};
constexpr uint8_t RESERVED_ETHERNET_COUNT = 10;

// GPIO12 strapping pin (warning-only, not blocked)
constexpr uint8_t STRAPPING_GPIO12 = 12;

// ── System pin field IDs (used by validation + Web UI) ──
constexpr const char* SYS_PIN_STATUS_LED = "status_led_pin";
constexpr const char* SYS_PIN_ZC = "zc_pin";
constexpr const char* SYS_PIN_I2C_SDA = "i2c_sda";
constexpr const char* SYS_PIN_I2C_SCL = "i2c_scl";

// ── Pin validation helpers ──

inline bool isInputOnlyPin(uint8_t pin) {
    for (uint8_t i = 0; i < INPUT_ONLY_COUNT; i++) {
        if (INPUT_ONLY_PINS[i] == pin) return true;
    }
    return false;
}

inline bool isReservedEthernetPin(uint8_t pin) {
    for (uint8_t i = 0; i < RESERVED_ETHERNET_COUNT; i++) {
        if (RESERVED_ETHERNET_PINS[i].pin == pin) return true;
    }
    return false;
}

}  // namespace GpioControl

#endif
