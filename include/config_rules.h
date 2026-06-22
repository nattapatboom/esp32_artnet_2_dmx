#ifndef CONFIG_RULES_H
#define CONFIG_RULES_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include "output_control.h"

struct AddressRange {
    uint8_t min;
    uint8_t max;
};

struct SourceAddressRule {
    uint8_t source;
    const char* label;
    AddressRange ranges[2];
};

constexpr SourceAddressRule SOURCE_ADDRESS_RULES[] = {
    {1, "PCA9685 address must be 0x40-0x47", {{0x40, 0x47}, {0, 0}}},
    {2, "MCP23017 address must be 0x20-0x27", {{0x20, 0x27}, {0, 0}}},
    {3, "TCA9555 address must be 0x20-0x27", {{0x20, 0x27}, {0, 0}}},
    {4, "PCF857x address must be 0x20-0x27 or 0x38-0x3F", {{0x20, 0x27}, {0x38, 0x3F}}},
    {5, "MCP4725 address must be 0x60 or 0x61", {{0x60, 0x61}, {0, 0}}},
    {6, "DAC7571 address must be 0x4C or 0x4D", {{0x4C, 0x4D}, {0, 0}}},
    {7, "DAC7573 address must be 0x4C-0x5B", {{0x4C, 0x5B}, {0, 0}}}
};

inline bool validateIp4(const char* s) {
    if (s == nullptr || s[0] == '\0') return true;
    uint8_t part = 0;
    uint16_t value = 0;
    bool hasDigit = false;
    for (const char* p = s; ; ++p) {
        char c = *p;
        if (c >= '0' && c <= '9') {
            hasDigit = true;
            value = value * 10 + (uint16_t)(c - '0');
            if (value > 255) return false;
        } else if (c == '.' || c == '\0') {
            if (!hasDigit) return false;
            part++;
            if (c == '\0') return part == 4;
            if (part >= 4) return false;
            value = 0;
            hasDigit = false;
        } else {
            return false;
        }
    }
}

inline bool sourceAddressValid(uint8_t source, uint8_t address) {
    if (source == 0) return true;
    for (const auto& rule : SOURCE_ADDRESS_RULES) {
        if (rule.source != source) continue;
        for (const auto& range : rule.ranges) {
            if (range.min == 0 && range.max == 0) continue;
            if (address >= range.min && address <= range.max) return true;
        }
        return false;
    }
    return false;
}

inline const char* sourceAddressRangeLabel(uint8_t source) {
    for (const auto& rule : SOURCE_ADDRESS_RULES) {
        if (rule.source == source) return rule.label;
    }
    return "Unsupported I2C source";
}

inline bool validateSourceAddress(uint8_t source, uint8_t address, const String& label, String& message) {
    if (source == 0) return true;
    if (sourceAddressValid(source, address)) return true;
    message = label + " has invalid I2C address 0x" + String(address, HEX) + ". " + sourceAddressRangeLabel(source);
    return false;
}

inline bool displayAddressValid(uint8_t displayType, uint8_t address) {
    if (displayType == 0) return true;
    if (displayType == 1 || displayType == 2) return address == 0x3C || address == 0x3D;
    if (displayType == 3) return address == 0x27 || address == 0x3F;
    return false;
}

inline bool isPin2GpioRouting(uint8_t type, uint8_t source, uint8_t pin2Source) {
    if (type == OutputDefs::TYPE_MOTOR || type == CHAN_TYPE_ANALOG_RGB || type == OutputDefs::TYPE_SMOKE || (type == OutputDefs::TYPE_STEPPER && pin2Source == 0)) return pin2Source == 0;
    if (type == OutputDefs::TYPE_TM1637 || type == OutputDefs::TYPE_DFPLAYER) return source == 0;
    return false;
}

inline bool isPin3GpioRouting(uint8_t type, uint8_t pin3Source, uint8_t mcMode) {
    if (type == CHAN_TYPE_ANALOG_RGB || (type == OutputDefs::TYPE_MOTOR && mcMode == 2) || type == OutputDefs::TYPE_STEPPER) return pin3Source == 0;
    return false;
}

inline bool isPin4GpioRouting(uint8_t type, uint8_t pin4Source, uint8_t colorOrder, uint8_t homingMode) {
    if (type == CHAN_TYPE_ANALOG_RGB && colorOrder >= 4) return pin4Source == 0;
    if (type == OutputDefs::TYPE_STEPPER && homingMode == 0) return pin4Source == 0;
    return false;
}

inline uint8_t numSegPins(uint8_t type) {
    return (type == OutputDefs::TYPE_7SEG_8PIN) ? 8 : 7;
}

inline bool isSegCommonDim(uint8_t mcMode) {
    return mcMode >= 6 && mcMode <= 9;
}

#endif
