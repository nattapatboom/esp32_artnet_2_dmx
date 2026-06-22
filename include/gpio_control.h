#ifndef GPIO_CONTROL_H
#define GPIO_CONTROL_H

#include <Arduino.h>
#include "output_defs.h"
#include "config_rules.h"

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

inline const char* reservedEthernetReason(uint8_t pin) {
    for (uint8_t i = 0; i < RESERVED_ETHERNET_COUNT; i++) {
        if (RESERVED_ETHERNET_PINS[i].pin == pin) return RESERVED_ETHERNET_PINS[i].reason;
    }
    return nullptr;
}

// Returns true if pin is neither 255 nor any known unavailable pin
inline bool isPinAvailableForOutput(uint8_t pin) {
    if (pin == 255) return false;
    if (isInputOnlyPin(pin)) return false;
    if (isReservedEthernetPin(pin)) return false;
    return true;
}

// Check if two pins conflict (both non-255 and equal)
inline bool pinsConflict(uint8_t a, uint8_t b) {
    return a != 255 && b != 255 && a == b;
}

// ── Output channel GPIO enumeration (no ArduinoJson needed) ──
// Caller provides a callback: addPin(pin, label) -> should return true to stop early
// Returns the total number of GPIO pins found.
template <typename F>
inline uint8_t enumerateChannelGpios(
    uint8_t type, uint8_t source, uint8_t mcMode,
    uint8_t pin, uint8_t pin2, uint8_t pin2Source,
    uint8_t pin3, uint8_t pin3Source,
    uint8_t pin4, uint8_t pin4Source,
    uint8_t colorOrder, uint8_t homingMode,
    const uint8_t* segPins, const uint8_t* segSources, uint8_t numSegs,
    F&& addPin
) {
    uint8_t count = 0;

    if (source == 0) {
        count++;
        if (addPin(pin, "pin1")) return count;
    }

    if (type == OutputDefs::TYPE_7SEG_7PIN || type == OutputDefs::TYPE_7SEG_8PIN) {
        if (pin2Source == 0) {
            uint8_t nSeg = numSegPins(type);
            uint8_t startIdx = isSegCommonDim(mcMode) ? 0 : 1;
            for (uint8_t s = startIdx; s < nSeg; s++) {
                uint8_t p = 255;
                if (segPins && s < numSegs) {
                    if (segSources && s < numSegs && segSources[s] != 0) continue;
                    p = segPins[s];
                }
                if (p == 255 && source == 0) {
                    p = (pin != 255 && pin + s <= 255) ? (uint8_t)(pin + s) : 255;
                }
                if (p != 255) {
                    count++;
                    if (addPin(p, "seg")) return count;
                }
            }
        }
    } else {
        if (isPin2GpioRouting(type, source, pin2Source)) {
            count++;
            if (addPin(pin2, "pin2")) return count;
        }
        if (isPin3GpioRouting(type, pin3Source, mcMode)) {
            count++;
            if (addPin(pin3, "pin3")) return count;
        }
        if (isPin4GpioRouting(type, pin4Source, colorOrder, homingMode)) {
            count++;
            if (addPin(pin4, "pin4")) return count;
        }
    }

    return count;
}

}  // namespace GpioControl

#endif
