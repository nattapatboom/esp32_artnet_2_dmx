#ifndef SOURCE_RULES_H
#define SOURCE_RULES_H

#include <Arduino.h>

// ─────────────────────────────────────────────
//  Source Address Rules
//  I2C address validation for expanders, DACs,
//  and PCA9685. Single source of truth shared
//  between C++ and Web UI (mirror of
//  SOURCE_ADDRESS_RULES in web/js/_gpio.js).
// ─────────────────────────────────────────────

namespace SourceRules {

enum SourceMask : uint8_t {
    SRC_GPIO = 1 << 0,
    SRC_PWM_EXPANDER = 1 << 1,
    SRC_DIGITAL_EXPANDER = 1 << 2,
    SRC_I2C_DAC = 1 << 3
};

struct AddressRange {
    uint8_t min;
    uint8_t max;
};

struct SourceAddressRule {
    uint8_t source;
    uint8_t mask;
    const char* label;
    AddressRange ranges[2];  // second range is {0,0} if unused
};

constexpr SourceAddressRule ADDRESS_RULES[] = {
    {1, SRC_PWM_EXPANDER, "PCA9685 address must be 0x40-0x47", {{0x40, 0x47}, {0, 0}}},
    {2, SRC_DIGITAL_EXPANDER, "MCP23017 address must be 0x20-0x27", {{0x20, 0x27}, {0, 0}}},
    {3, SRC_DIGITAL_EXPANDER, "TCA9555 address must be 0x20-0x27", {{0x20, 0x27}, {0, 0}}},
    {4, SRC_DIGITAL_EXPANDER, "PCF857x address must be 0x20-0x27 or 0x38-0x3F", {{0x20, 0x27}, {0x38, 0x3F}}},
    {5, SRC_I2C_DAC, "MCP4725 address must be 0x60 or 0x61", {{0x60, 0x61}, {0, 0}}},
    {6, SRC_I2C_DAC, "DAC7571 address must be 0x4C or 0x4D", {{0x4C, 0x4D}, {0, 0}}},
    {7, SRC_I2C_DAC, "DAC7573 address must be 0x4C-0x5B", {{0x4C, 0x5B}, {0, 0}}}
};
constexpr uint8_t ADDRESS_RULES_COUNT = 7;

// Source display names (must match index: 0=GPIO, 1=PCA9685, 2-4=digital expanders, 5-7=I2C DAC)
constexpr const char* SOURCE_NAMES[] = {
    "ESP32",
    "PCA9685",
    "MCP23017",
    "TCA9555",
    "PCF857x",
    "MCP4725 (I2C DAC)",
    "DAC7571 (I2C DAC)",
    "DAC7573 (I2C DAC)"
};
constexpr uint8_t SOURCE_NAMES_COUNT = 8;

constexpr const char* UNSUPPORTED_LABEL = "Unsupported I2C source";

inline bool addressValid(uint8_t source, uint8_t address) {
    if (source == 0) return true;
    for (const auto& rule : ADDRESS_RULES) {
        if (rule.source != source) continue;
        for (const auto& range : rule.ranges) {
            if (range.min == 0 && range.max == 0) continue;
            if (address >= range.min && address <= range.max) return true;
        }
        return false;
    }
    return false;
}

inline const char* addressRangeLabel(uint8_t source) {
    for (const auto& rule : ADDRESS_RULES) {
        if (rule.source == source) return rule.label;
    }
    return UNSUPPORTED_LABEL;
}

inline bool validateAddress(uint8_t source, uint8_t address, const String& label, String& message) {
    if (source == 0) return true;
    if (addressValid(source, address)) return true;
    message = label + " has invalid I2C address 0x" + String(address, HEX) + ". " + addressRangeLabel(source);
    return false;
}

}  // namespace SourceRules

#endif
