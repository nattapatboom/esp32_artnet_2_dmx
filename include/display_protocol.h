#ifndef DISPLAY_PROTOCOL_H
#define DISPLAY_PROTOCOL_H

#include <Arduino.h>

// ─────────────────────────────────────────────
//  Display Protocol
//  Display type IDs and I2C address validation.
//  Single source of truth shared between C++
//  and Web UI (mirror of displayAddressValid()
//  in web/js/_gpio.js).
// ─────────────────────────────────────────────

namespace DisplayProtocol {

// Display type IDs (matches SystemConfig display fields)
// Uses DTYPE_ prefix to avoid conflict with display_driver.h macros
constexpr uint8_t DTYPE_NONE    = 0;
constexpr uint8_t DTYPE_SSD1306 = 1;
constexpr uint8_t DTYPE_SH1106  = 2;
constexpr uint8_t DTYPE_LCD     = 3;  // PCF8574 LCD backpack

// Valid I2C addresses per type
constexpr uint8_t SSD1306_ADDRS[] = {0x3C, 0x3D};
constexpr uint8_t SSD1306_ADDR_COUNT = 2;
constexpr uint8_t SH1106_ADDRS[]  = {0x3C, 0x3D};
constexpr uint8_t SH1106_ADDR_COUNT = 2;
constexpr uint8_t LCD_ADDRS[]     = {0x27, 0x3F};
constexpr uint8_t LCD_ADDR_COUNT = 2;

// Human-readable display type names
constexpr const char* TYPE_NAMES[] = {
    "None",
    "SSD1306 OLED",
    "SH1106 OLED",
    "PCF8574 LCD"
};

inline bool addressValid(uint8_t displayType, uint8_t address) {
    if (displayType == DTYPE_NONE) return true;
    if (displayType == DTYPE_SSD1306 || displayType == DTYPE_SH1106) {
        return address == 0x3C || address == 0x3D;
    }
    if (displayType == DTYPE_LCD) {
        return address == 0x27 || address == 0x3F;
    }
    return false;
}

inline const char* typeName(uint8_t displayType) {
    if (displayType <= 3) return TYPE_NAMES[displayType];
    return "Unknown";
}

}  // namespace DisplayProtocol

#endif
