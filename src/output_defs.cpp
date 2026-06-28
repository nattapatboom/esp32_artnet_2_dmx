#include "output_defs.h"

namespace OutputDefs {

const OutputModeDef* modeDef(uint8_t type, uint8_t mode) {
    const OutputModeDef* fallback = nullptr;
    for (const auto& def : OUTPUT_MODES) {
        if (def.type != type) continue;
        if (def.mode == (int8_t)mode) return &def;
        if (def.mode == -1) fallback = &def;
    }
    return fallback;
}

uint16_t baseCpuUs(uint8_t type, uint8_t mode) {
    const OutputModeDef* def = modeDef(type, mode);
    return def ? def->cost.cpuUs : 0;
}

const PinRule* pinRule(uint8_t type, uint8_t mode, uint8_t slotIndex) {
    const OutputModeDef* def = modeDef(type, mode);
    if (def == nullptr || slotIndex >= def->pinCount) return nullptr;
    return &def->pins[slotIndex];
}

uint8_t sourceMaskForSourceId(uint8_t source) {
    if (source == 0) return SRC_GPIO;
    for (const auto& rule : SourceRules::ADDRESS_RULES) {
        if (rule.source == source) return rule.mask;
    }
    return 0;
}

bool isPwmExpanderSource(uint8_t source) {
    return (sourceMaskForSourceId(source) & SRC_PWM_EXPANDER) != 0;
}

bool isDigitalExpanderSource(uint8_t source) {
    return (sourceMaskForSourceId(source) & SRC_DIGITAL_EXPANDER) != 0;
}

bool sourceAllowedForSlot(uint8_t type, uint8_t mode, uint8_t slotIndex, uint8_t source) {
    const PinRule* rule = pinRule(type, mode, slotIndex);
    uint8_t sourceMask = sourceMaskForSourceId(source);
    return rule != nullptr && sourceMask != 0 && (rule->sources & sourceMask);
}

bool pinSlotUsesGpio(uint8_t type, uint8_t mode, uint8_t slotIndex, uint8_t routeSource) {
    const PinRule* rule = pinRule(type, mode, slotIndex);
    return rule != nullptr && routeSource == 0 && (rule->sources & SRC_GPIO);
}

bool startsAtFirstUniverse(uint8_t type, uint8_t mode) {
    const OutputModeDef* def = modeDef(type, mode);
    return def != nullptr && def->startAtFirstUniverse;
}

const char* typeInterfaceName(uint8_t type) {
    switch (type) {
        case TYPE_DIMMER:    return Type0::TYPE_NAME;
        case TYPE_DMX:       return Type1::TYPE_NAME;
        case TYPE_RELAY:     return Type2::TYPE_NAME;
        case TYPE_LED_STRIP: return Type3::TYPE_NAME;
        case TYPE_SINGLE_LED:return Type4::TYPE_NAME;
        case TYPE_ANALOG_RGB:return Type5::TYPE_NAME;
        case TYPE_MOTOR:     return Type6::TYPE_NAME;
        case TYPE_STEPPER:   return Type7::TYPE_NAME;
        case TYPE_SERVO:     return Type8::TYPE_NAME;
        case TYPE_BUZZER:    return Type9::TYPE_NAME;
        case TYPE_DFPLAYER:  return Type10::TYPE_NAME;
        case TYPE_TM1637:    return Type11::TYPE_NAME;
        case TYPE_7SEG_7PIN: return Type12::TYPE_NAME;
        case TYPE_7SEG_8PIN: return Type13::TYPE_NAME;
        case TYPE_DAC:       return Type14::TYPE_NAME;
        case TYPE_PWM_DAC:   return Type15::TYPE_NAME;
        case TYPE_FUNC_GEN:  return Type16::TYPE_NAME;
        case TYPE_SOLENOID:  return Type17::TYPE_NAME;
        case TYPE_SMOKE:     return Type18::TYPE_NAME;
        default: return "Unknown";
    }
}

const TypeProtocol::FieldDef* typeExtraFields(uint8_t type, uint8_t& count) {
    switch (type) {
        case TYPE_DIMMER:    count = TYPEPROTO_ARRAY_SIZE(Type0::EXTRA_FIELDS);  return Type0::EXTRA_FIELDS;
        case TYPE_DMX:       count = TYPEPROTO_ARRAY_SIZE(Type1::EXTRA_FIELDS);  return Type1::EXTRA_FIELDS;
        case TYPE_RELAY:     count = TYPEPROTO_ARRAY_SIZE(Type2::EXTRA_FIELDS);  return Type2::EXTRA_FIELDS;
        case TYPE_LED_STRIP: count = TYPEPROTO_ARRAY_SIZE(Type3::EXTRA_FIELDS);  return Type3::EXTRA_FIELDS;
        case TYPE_SINGLE_LED:count = TYPEPROTO_ARRAY_SIZE(Type4::EXTRA_FIELDS);  return Type4::EXTRA_FIELDS;
        case TYPE_ANALOG_RGB:count = TYPEPROTO_ARRAY_SIZE(Type5::EXTRA_FIELDS);  return Type5::EXTRA_FIELDS;
        case TYPE_MOTOR:     count = TYPEPROTO_ARRAY_SIZE(Type6::EXTRA_FIELDS);  return Type6::EXTRA_FIELDS;
        case TYPE_STEPPER:   count = TYPEPROTO_ARRAY_SIZE(Type7::EXTRA_FIELDS);  return Type7::EXTRA_FIELDS;
        case TYPE_SERVO:     count = TYPEPROTO_ARRAY_SIZE(Type8::EXTRA_FIELDS);  return Type8::EXTRA_FIELDS;
        case TYPE_BUZZER:    count = TYPEPROTO_ARRAY_SIZE(Type9::EXTRA_FIELDS);  return Type9::EXTRA_FIELDS;
        case TYPE_DFPLAYER:  count = TYPEPROTO_ARRAY_SIZE(Type10::EXTRA_FIELDS); return Type10::EXTRA_FIELDS;
        case TYPE_TM1637:    count = TYPEPROTO_ARRAY_SIZE(Type11::EXTRA_FIELDS); return Type11::EXTRA_FIELDS;
        case TYPE_7SEG_7PIN: count = TYPEPROTO_ARRAY_SIZE(Type12::EXTRA_FIELDS); return Type12::EXTRA_FIELDS;
        case TYPE_7SEG_8PIN: count = TYPEPROTO_ARRAY_SIZE(Type13::EXTRA_FIELDS); return Type13::EXTRA_FIELDS;
        case TYPE_DAC:       count = TYPEPROTO_ARRAY_SIZE(Type14::EXTRA_FIELDS); return Type14::EXTRA_FIELDS;
        case TYPE_PWM_DAC:   count = TYPEPROTO_ARRAY_SIZE(Type15::EXTRA_FIELDS); return Type15::EXTRA_FIELDS;
        case TYPE_FUNC_GEN:  count = TYPEPROTO_ARRAY_SIZE(Type16::EXTRA_FIELDS); return Type16::EXTRA_FIELDS;
        case TYPE_SOLENOID:  count = TYPEPROTO_ARRAY_SIZE(Type17::EXTRA_FIELDS); return Type17::EXTRA_FIELDS;
        case TYPE_SMOKE:     count = TYPEPROTO_ARRAY_SIZE(Type18::EXTRA_FIELDS); return Type18::EXTRA_FIELDS;
        default: count = 0; return nullptr;
    }
}

const TypeProtocol::TestCmdDef* typeTestCommands(uint8_t type, uint8_t& count) {
    switch (type) {
        case TYPE_DIMMER:    count = TYPEPROTO_ARRAY_SIZE(Type0::TEST_COMMANDS);  return Type0::TEST_COMMANDS;
        case TYPE_DMX:       count = TYPEPROTO_ARRAY_SIZE(Type1::TEST_COMMANDS);  return Type1::TEST_COMMANDS;
        case TYPE_RELAY:     count = TYPEPROTO_ARRAY_SIZE(Type2::TEST_COMMANDS);  return Type2::TEST_COMMANDS;
        case TYPE_LED_STRIP: count = TYPEPROTO_ARRAY_SIZE(Type3::TEST_COMMANDS);  return Type3::TEST_COMMANDS;
        case TYPE_SINGLE_LED:count = TYPEPROTO_ARRAY_SIZE(Type4::TEST_COMMANDS);  return Type4::TEST_COMMANDS;
        case TYPE_ANALOG_RGB:count = TYPEPROTO_ARRAY_SIZE(Type5::TEST_COMMANDS);  return Type5::TEST_COMMANDS;
        case TYPE_MOTOR:     count = TYPEPROTO_ARRAY_SIZE(Type6::TEST_COMMANDS);  return Type6::TEST_COMMANDS;
        case TYPE_STEPPER:   count = TYPEPROTO_ARRAY_SIZE(Type7::TEST_COMMANDS);  return Type7::TEST_COMMANDS;
        case TYPE_SERVO:     count = TYPEPROTO_ARRAY_SIZE(Type8::TEST_COMMANDS);  return Type8::TEST_COMMANDS;
        case TYPE_BUZZER:    count = TYPEPROTO_ARRAY_SIZE(Type9::TEST_COMMANDS);  return Type9::TEST_COMMANDS;
        case TYPE_DFPLAYER:  count = TYPEPROTO_ARRAY_SIZE(Type10::TEST_COMMANDS); return Type10::TEST_COMMANDS;
        case TYPE_TM1637:    count = TYPEPROTO_ARRAY_SIZE(Type11::TEST_COMMANDS); return Type11::TEST_COMMANDS;
        case TYPE_7SEG_7PIN: count = TYPEPROTO_ARRAY_SIZE(Type12::TEST_COMMANDS); return Type12::TEST_COMMANDS;
        case TYPE_7SEG_8PIN: count = TYPEPROTO_ARRAY_SIZE(Type13::TEST_COMMANDS); return Type13::TEST_COMMANDS;
        case TYPE_DAC:       count = TYPEPROTO_ARRAY_SIZE(Type14::TEST_COMMANDS); return Type14::TEST_COMMANDS;
        case TYPE_PWM_DAC:   count = TYPEPROTO_ARRAY_SIZE(Type15::TEST_COMMANDS); return Type15::TEST_COMMANDS;
        case TYPE_FUNC_GEN:  count = TYPEPROTO_ARRAY_SIZE(Type16::TEST_COMMANDS); return Type16::TEST_COMMANDS;
        case TYPE_SOLENOID:  count = TYPEPROTO_ARRAY_SIZE(Type17::TEST_COMMANDS); return Type17::TEST_COMMANDS;
        case TYPE_SMOKE:     count = TYPEPROTO_ARRAY_SIZE(Type18::TEST_COMMANDS); return Type18::TEST_COMMANDS;
        default: count = 0; return nullptr;
    }
}

uint32_t dmxBufferRamForChannel(uint8_t type, uint16_t ledCount, uint8_t colorOrder, uint8_t resolution, uint8_t mode) {
    int8_t signedMode = (int8_t)mode;
    const auto* def = modeDef(type, mode);
    uint16_t flags = def != nullptr ? def->cost.flags : CF_NONE;
    if (def != nullptr && def->cost.dmxSlots > 0) return def->cost.dmxSlots;
    if (flags & CF_DYN_LED_STRIP) {
        uint8_t bpp = colorOrder >= 4 ? 4 : 3;
        uint16_t pixelsPerUni = 512 / bpp;
        uint16_t universes = (ledCount + pixelsPerUni - 1) / pixelsPerUni;
        if (universes < 1) universes = 1;
        return (uint32_t)universes * 512;
    }
    if (flags & CF_DYN_COLOR_BYTES) return colorOrder >= 4 ? 4 : 3;
    if (flags & CF_DYN_STEPPER) return getValueByteCount(resolution) + 2;
    if (flags & CF_DYN_TEXT_MODE) return signedMode == 1 ? 4 : 2;
    if (flags & CF_DYN_SEGMENT_MODE) return signedMode >= 0 ? 2 : 1;
    return getValueByteCount(resolution);
}

}  // namespace OutputDefs
