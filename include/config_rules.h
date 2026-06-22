#ifndef CONFIG_RULES_H
#define CONFIG_RULES_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include "output_control.h"

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
    switch (source) {
        case 1: return address >= 0x40 && address <= 0x47; // PCA9685
        case 2: return address >= 0x20 && address <= 0x27; // MCP23017
        case 3: return address >= 0x20 && address <= 0x27; // TCA9555
        case 4: return (address >= 0x20 && address <= 0x27) || (address >= 0x38 && address <= 0x3F); // PCF857x / PCF8574A
        case 5: return address == 0x60 || address == 0x61; // MCP4725
        case 6: return address == 0x4C || address == 0x4D; // DAC7571
        case 7: return address >= 0x4C && address <= 0x5B; // DAC7573
        default: return source == 0;
    }
}

inline const char* sourceAddressRangeLabel(uint8_t source) {
    switch (source) {
        case 1: return "PCA9685 address must be 0x40-0x47";
        case 2: return "MCP23017 address must be 0x20-0x27";
        case 3: return "TCA9555 address must be 0x20-0x27";
        case 4: return "PCF857x address must be 0x20-0x27 or 0x38-0x3F";
        case 5: return "MCP4725 address must be 0x60 or 0x61";
        case 6: return "DAC7571 address must be 0x4C or 0x4D";
        case 7: return "DAC7573 address must be 0x4C-0x5B";
        default: return "Unsupported I2C source";
    }
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
    if (type == 6 || type == CHAN_TYPE_ANALOG_RGB || type == 18 || (type == 7 && pin2Source == 0)) return pin2Source == 0;
    if (type == 11 || type == 10) return source == 0;
    return false;
}

inline bool isPin3GpioRouting(uint8_t type, uint8_t pin3Source, uint8_t mcMode) {
    if (type == CHAN_TYPE_ANALOG_RGB || (type == 6 && mcMode == 2) || type == 7) return pin3Source == 0;
    return false;
}

inline bool isPin4GpioRouting(uint8_t type, uint8_t pin4Source, uint8_t colorOrder, uint8_t homingMode) {
    if (type == CHAN_TYPE_ANALOG_RGB && colorOrder >= 4) return pin4Source == 0;
    if (type == 7 && homingMode == 0) return pin4Source == 0;
    return false;
}

inline uint8_t numSegPins(uint8_t type) {
    return (type == 13) ? 8 : 7;
}

inline bool isSegCommonDim(uint8_t mcMode) {
    return mcMode >= 6 && mcMode <= 9;
}

#endif
