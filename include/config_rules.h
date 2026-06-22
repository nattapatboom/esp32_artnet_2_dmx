#ifndef CONFIG_RULES_H
#define CONFIG_RULES_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include "output_control.h"
#include "source_rules.h"
#include "display_protocol.h"

// ── IP validation (general-purpose, not duplicated in JS) ──

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

// ── GPIO routing helpers (stay here — unique to output routing logic) ──

inline bool isPin2GpioRouting(uint8_t type, uint8_t source, uint8_t pin2Source) {
    if (type == OutputDefs::TYPE_MOTOR || type == OutputDefs::TYPE_ANALOG_RGB || type == OutputDefs::TYPE_SMOKE || (type == OutputDefs::TYPE_STEPPER && pin2Source == 0)) return pin2Source == 0;
    if (type == OutputDefs::TYPE_TM1637 || type == OutputDefs::TYPE_DFPLAYER) return source == 0;
    return false;
}

inline bool isPin3GpioRouting(uint8_t type, uint8_t pin3Source, uint8_t mcMode) {
    if (type == OutputDefs::TYPE_ANALOG_RGB || (type == OutputDefs::TYPE_MOTOR && mcMode == 2) || type == OutputDefs::TYPE_STEPPER) return pin3Source == 0;
    return false;
}

inline bool isPin4GpioRouting(uint8_t type, uint8_t pin4Source, uint8_t colorOrder, uint8_t homingMode) {
    if (type == OutputDefs::TYPE_ANALOG_RGB && colorOrder >= 4) return pin4Source == 0;
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
