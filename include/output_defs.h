#ifndef OUTPUT_DEFS_H
#define OUTPUT_DEFS_H

#include <Arduino.h>
#include "type_protocol.h"
#include "type_interfaces/type_0.h"
#include "type_interfaces/type_1.h"
#include "type_interfaces/type_2.h"
#include "type_interfaces/type_3.h"
#include "type_interfaces/type_4.h"
#include "type_interfaces/type_5.h"
#include "type_interfaces/type_6.h"
#include "type_interfaces/type_7.h"
#include "type_interfaces/type_8.h"
#include "type_interfaces/type_9.h"
#include "type_interfaces/type_10.h"
#include "type_interfaces/type_11.h"
#include "type_interfaces/type_12.h"
#include "type_interfaces/type_13.h"
#include "type_interfaces/type_14.h"
#include "type_interfaces/type_15.h"
#include "type_interfaces/type_16.h"
#include "type_interfaces/type_17.h"
#include "type_interfaces/type_18.h"

namespace OutputDefs {

enum SourceMask : uint8_t {
    SRC_GPIO = 1 << 0,
    SRC_PCA = 1 << 1,
    SRC_DIGITAL_EXPANDER = 1 << 2,
    SRC_I2C_DAC = 1 << 3
};

constexpr uint8_t TYPE_DIMMER       = 0;
constexpr uint8_t TYPE_DMX          = 1;
constexpr uint8_t TYPE_RELAY        = 2;
constexpr uint8_t TYPE_LED_STRIP    = 3;
constexpr uint8_t TYPE_SINGLE_LED   = 4;
constexpr uint8_t TYPE_ANALOG_RGB   = 5;
constexpr uint8_t TYPE_MOTOR        = 6;
constexpr uint8_t TYPE_STEPPER      = 7;
constexpr uint8_t TYPE_SERVO        = 8;
constexpr uint8_t TYPE_BUZZER       = 9;
constexpr uint8_t TYPE_DFPLAYER     = 10;
constexpr uint8_t TYPE_TM1637       = 11;
constexpr uint8_t TYPE_7SEG_7PIN    = 12;
constexpr uint8_t TYPE_7SEG_8PIN    = 13;
constexpr uint8_t TYPE_DAC          = 14;
constexpr uint8_t TYPE_PWM_DAC      = 15;
constexpr uint8_t TYPE_FUNC_GEN     = 16;
constexpr uint8_t TYPE_SOLENOID     = 17;
constexpr uint8_t TYPE_SMOKE        = 18;

enum PinDirection : uint8_t {
    PIN_OUTPUT,
    PIN_INPUT
};

// hwIfGpio: when this pin is driven by ESP32 GPIO directly,
// what hardware does it consume?
//   0 = none (digital output/input)
//   1 = LEDC (PWM-capable output)
//   2 = RMT (WS281x data, stepper STEP)
struct PinRule {
    const char* slot;
    const char* label;
    uint8_t sources;
    PinDirection direction;
    bool invertible;
    uint8_t hwIfGpio;
};

struct HardwareCost {
    uint8_t ledc;
    uint8_t rmt;
    uint8_t uart;
    uint8_t dac;
    uint8_t timer;
};

struct ModeCost {
    uint16_t cpuUs;
    uint16_t extraRamBytes;
    HardwareCost hardware;
    uint8_t cpuPerUnit;   // µs per unit (LED = per pixel, DMX = pre-filled)
    uint8_t ramPerUnit;   // bytes per unit (LED = per pixel, DMX = pre-filled)
    uint16_t dmxSlots;    // 0 = use dmxValueByteCount(resolution); >0 = fixed
};

struct OutputModeDef {
    uint8_t type;
    int8_t mode;
    const char* name;
    ModeCost cost;
    const PinRule* pins;
    uint8_t pinCount;
};

constexpr PinRule PINS_GPIO_MAIN[] = {
    {"pin1", "Main", SRC_GPIO, PIN_OUTPUT, true, 0}
};
constexpr PinRule PINS_DMX[] = {
    {"pin1", "DMX TX", SRC_GPIO, PIN_OUTPUT, true, 0}
};
constexpr PinRule PINS_RELAY_DIGITAL[] = {
    {"pin1", "Relay", SRC_GPIO | SRC_PCA | SRC_DIGITAL_EXPANDER, PIN_OUTPUT, true, 0}
};
constexpr PinRule PINS_LED_STRIP[] = {
    {"pin1", "Data", SRC_GPIO, PIN_OUTPUT, true, 2}
};
constexpr PinRule PINS_PWM[] = {
    {"pin1", "PWM", SRC_GPIO | SRC_PCA, PIN_OUTPUT, true, 1}
};
constexpr PinRule PINS_ANALOG_RGB[] = {
    {"pin1", "Red", SRC_GPIO | SRC_PCA, PIN_OUTPUT, true, 1},
    {"pin2", "Green", SRC_GPIO | SRC_PCA, PIN_OUTPUT, true, 1},
    {"pin3", "Blue", SRC_GPIO | SRC_PCA, PIN_OUTPUT, true, 1},
    {"pin4", "White", SRC_GPIO | SRC_PCA, PIN_OUTPUT, true, 1}
};
constexpr PinRule PINS_MOTOR_PWM_DIR[] = {
    {"pin1", "PWM", SRC_GPIO | SRC_PCA, PIN_OUTPUT, true, 1},
    {"pin2", "DIR", SRC_GPIO | SRC_PCA | SRC_DIGITAL_EXPANDER, PIN_OUTPUT, true, 0}
};
constexpr PinRule PINS_MOTOR_IN1_IN2_EN[] = {
    {"pin1", "IN1", SRC_GPIO | SRC_PCA, PIN_OUTPUT, true, 1},
    {"pin2", "IN2", SRC_GPIO | SRC_PCA | SRC_DIGITAL_EXPANDER, PIN_OUTPUT, true, 0},
    {"pin3", "EN PWM", SRC_GPIO | SRC_PCA, PIN_OUTPUT, true, 1}
};
constexpr PinRule PINS_STEPPER[] = {
    {"pin1", "STEP", SRC_GPIO, PIN_OUTPUT, true, 2},
    {"pin2", "DIR", SRC_GPIO | SRC_DIGITAL_EXPANDER, PIN_OUTPUT, true, 0},
    {"pin3", "ENABLE", SRC_GPIO | SRC_DIGITAL_EXPANDER, PIN_OUTPUT, true, 0},
    {"pin4", "HOME", SRC_GPIO | SRC_DIGITAL_EXPANDER, PIN_INPUT, true, 0}
};
constexpr PinRule PINS_DFPLAYER[] = {
    {"pin1", "TX", SRC_GPIO, PIN_OUTPUT, false, 0},
    {"pin2", "RX", SRC_GPIO, PIN_INPUT, false, 0}
};
constexpr PinRule PINS_TM1637[] = {
    {"pin1", "CLK", SRC_GPIO, PIN_OUTPUT, true, 0},
    {"pin2", "DIO", SRC_GPIO, PIN_OUTPUT, true, 0}
};
constexpr PinRule PINS_7SEG_DIRECT[] = {
    {"pin1", "Segment A", SRC_GPIO | SRC_PCA, PIN_OUTPUT, true, 1},
    {"pin2", "Segment B", SRC_GPIO | SRC_PCA | SRC_DIGITAL_EXPANDER, PIN_OUTPUT, true, 1},
    {"pin3", "Segment C", SRC_GPIO | SRC_PCA | SRC_DIGITAL_EXPANDER, PIN_OUTPUT, true, 1},
    {"pin4", "Segment D", SRC_GPIO | SRC_PCA | SRC_DIGITAL_EXPANDER, PIN_OUTPUT, true, 1},
    {"pin5", "Segment E", SRC_GPIO | SRC_PCA | SRC_DIGITAL_EXPANDER, PIN_OUTPUT, true, 1},
    {"pin6", "Segment F", SRC_GPIO | SRC_PCA | SRC_DIGITAL_EXPANDER, PIN_OUTPUT, true, 1},
    {"pin7", "Segment G", SRC_GPIO | SRC_PCA | SRC_DIGITAL_EXPANDER, PIN_OUTPUT, true, 1},
    {"pin8", "Segment DP", SRC_GPIO | SRC_PCA | SRC_DIGITAL_EXPANDER, PIN_OUTPUT, true, 1}
};
constexpr PinRule PINS_7SEG_DIMMED[] = {
    {"pin1", "Segment A", SRC_GPIO | SRC_PCA, PIN_OUTPUT, true, 1},
    {"pin2", "Segment B", SRC_GPIO | SRC_PCA, PIN_OUTPUT, true, 1},
    {"pin3", "Segment C", SRC_GPIO | SRC_PCA, PIN_OUTPUT, true, 1},
    {"pin4", "Segment D", SRC_GPIO | SRC_PCA, PIN_OUTPUT, true, 1},
    {"pin5", "Segment E", SRC_GPIO | SRC_PCA, PIN_OUTPUT, true, 1},
    {"pin6", "Segment F", SRC_GPIO | SRC_PCA, PIN_OUTPUT, true, 1},
    {"pin7", "Segment G", SRC_GPIO | SRC_PCA, PIN_OUTPUT, true, 1},
    {"pin8", "Segment DP", SRC_GPIO | SRC_PCA, PIN_OUTPUT, true, 1}
};
constexpr PinRule PINS_7SEG_COMMON_DIM[] = {
    {"pin1", "Common PWM", SRC_GPIO | SRC_PCA, PIN_OUTPUT, true, 1},
    {"pin2", "Segment A", SRC_GPIO | SRC_PCA | SRC_DIGITAL_EXPANDER, PIN_OUTPUT, true, 0},
    {"pin3", "Segment B", SRC_GPIO | SRC_PCA | SRC_DIGITAL_EXPANDER, PIN_OUTPUT, true, 0},
    {"pin4", "Segment C", SRC_GPIO | SRC_PCA | SRC_DIGITAL_EXPANDER, PIN_OUTPUT, true, 0},
    {"pin5", "Segment D", SRC_GPIO | SRC_PCA | SRC_DIGITAL_EXPANDER, PIN_OUTPUT, true, 0},
    {"pin6", "Segment E", SRC_GPIO | SRC_PCA | SRC_DIGITAL_EXPANDER, PIN_OUTPUT, true, 0},
    {"pin7", "Segment F", SRC_GPIO | SRC_PCA | SRC_DIGITAL_EXPANDER, PIN_OUTPUT, true, 0},
    {"pin8", "Segment G", SRC_GPIO | SRC_PCA | SRC_DIGITAL_EXPANDER, PIN_OUTPUT, true, 0},
    {"pin9", "Segment DP", SRC_GPIO | SRC_PCA | SRC_DIGITAL_EXPANDER, PIN_OUTPUT, true, 0}
};
constexpr PinRule PINS_DAC[] = {
    {"pin1", "I2C DAC", SRC_I2C_DAC, PIN_OUTPUT, false, 0}
};
constexpr PinRule PINS_SOLENOID[] = {
    {"pin1", "Solenoid", SRC_GPIO | SRC_PCA | SRC_DIGITAL_EXPANDER, PIN_OUTPUT, true, 0}
};
constexpr PinRule PINS_SMOKE[] = {
    {"pin1", "Smoke Valve", SRC_GPIO | SRC_PCA | SRC_DIGITAL_EXPANDER, PIN_OUTPUT, true, 0},
    {"pin2", "Shoot Valve", SRC_GPIO | SRC_PCA | SRC_DIGITAL_EXPANDER, PIN_OUTPUT, true, 0}
};

constexpr HardwareCost HW_NONE = {0, 0, 0, 0, 0};
constexpr HardwareCost HW_LEDC_1 = {1, 0, 0, 0, 0};
constexpr HardwareCost HW_LEDC_2 = {2, 0, 0, 0, 0};
constexpr HardwareCost HW_LEDC_4 = {4, 0, 0, 0, 0};
constexpr HardwareCost HW_LEDC_7 = {7, 0, 0, 0, 0};
constexpr HardwareCost HW_LEDC_8 = {8, 0, 0, 0, 0};
constexpr HardwareCost HW_RMT_1 = {0, 1, 0, 0, 0};
constexpr HardwareCost HW_RMT_2 = {0, 2, 0, 0, 0};
constexpr HardwareCost HW_UART_1 = {0, 0, 1, 0, 0};
constexpr HardwareCost HW_LEDC_1_TIMER_1 = {1, 0, 0, 0, 1};

constexpr ModeCost modeCost(
    uint16_t cpuUs,
    uint16_t extraRamBytes = 0,
    HardwareCost hardware = HW_NONE,
    uint8_t cpuPerUnit = 0,
    uint8_t ramPerUnit = 0,
    uint16_t dmxSlots = 0
) {
    return {cpuUs, extraRamBytes, hardware, cpuPerUnit, ramPerUnit, dmxSlots};
}

constexpr ModeCost COST_DIMMER = modeCost(5, 0, HW_NONE, 0, 0, 1);
constexpr ModeCost COST_DMX_SERIAL = modeCost(250, 0, HW_UART_1, 0, 0, 512);
constexpr ModeCost COST_RELAY = modeCost(5, 0, HW_NONE, 0, 0, 1);
constexpr ModeCost COST_LED_STRIP_BASE = modeCost(80, 256, HW_RMT_1, 3, 3);
constexpr ModeCost COST_SINGLE_LED = modeCost(6, 0, HW_LEDC_1);
constexpr ModeCost COST_ANALOG_RGBW = modeCost(18, 0, HW_LEDC_4);
constexpr ModeCost COST_MOTOR_PWM_DIR = modeCost(35, 0, HW_LEDC_2);
constexpr ModeCost COST_MOTOR_IN1_IN2 = modeCost(35, 0, HW_LEDC_1);
constexpr ModeCost COST_MOTOR_IN1_IN2_EN = modeCost(35, 0, HW_LEDC_2);
constexpr ModeCost COST_STEPPER = modeCost(80, 512, HW_RMT_2);
constexpr ModeCost COST_SERVO = modeCost(12, 0, HW_LEDC_1);
constexpr ModeCost COST_BUZZER = modeCost(35, 0, HW_LEDC_1);
constexpr ModeCost COST_DFPLAYER = modeCost(30, 260, HW_UART_1, 0, 0, 3);
constexpr ModeCost COST_TM1637 = modeCost(900);
constexpr ModeCost COST_7SEG_7PIN = modeCost(30, 0, HW_LEDC_7);
constexpr ModeCost COST_7SEG_8PIN = modeCost(35, 0, HW_LEDC_8);
constexpr ModeCost COST_7SEG_COMMON_DIM_7PIN = modeCost(30, 0, HW_LEDC_1);
constexpr ModeCost COST_7SEG_COMMON_DIM_8PIN = modeCost(35, 0, HW_LEDC_1);
constexpr ModeCost COST_DAC = modeCost(10);
constexpr ModeCost COST_PWM_DAC = modeCost(6, 0, HW_LEDC_1);
constexpr ModeCost COST_FUNC_GEN = modeCost(120, 1120, HW_LEDC_1_TIMER_1, 0, 0, 5);
constexpr ModeCost COST_SOLENOID = modeCost(10, 0, HW_NONE, 0, 0, 1);
constexpr ModeCost COST_SMOKE = modeCost(25, 0, HW_NONE, 0, 0, 1);

constexpr OutputModeDef OUTPUT_MODES[] = {
    {TYPE_DIMMER, -1, "Triac dimmer", COST_DIMMER, PINS_GPIO_MAIN, 1},
    {TYPE_DMX, -1, "DMX serial", COST_DMX_SERIAL, PINS_DMX, 1},
    {TYPE_RELAY, -1, "Relay", COST_RELAY, PINS_RELAY_DIGITAL, 1},
    {TYPE_LED_STRIP, -1, "RGB/RGBW strip", COST_LED_STRIP_BASE, PINS_LED_STRIP, 1},
    {TYPE_SINGLE_LED, -1, "Single-color PWM", COST_SINGLE_LED, PINS_PWM, 1},
    {TYPE_ANALOG_RGB, -1, "Analog RGB/RGBW", COST_ANALOG_RGBW, PINS_ANALOG_RGB, 4},
    {TYPE_MOTOR, 0, "PWM + DIR", COST_MOTOR_PWM_DIR, PINS_MOTOR_PWM_DIR, 2},
    {TYPE_MOTOR, 1, "IN1 + IN2", COST_MOTOR_IN1_IN2, PINS_MOTOR_PWM_DIR, 2},
    {TYPE_MOTOR, 2, "IN1 + IN2 + EN", COST_MOTOR_IN1_IN2_EN, PINS_MOTOR_IN1_IN2_EN, 3},
    {TYPE_STEPPER, -1, "Stepper", COST_STEPPER, PINS_STEPPER, 4},
    {TYPE_SERVO, -1, "RC servo", COST_SERVO, PINS_PWM, 1},
    {TYPE_BUZZER, -1, "Passive buzzer", COST_BUZZER, PINS_GPIO_MAIN, 1},
    {TYPE_DFPLAYER, -1, "DFPlayer", COST_DFPLAYER, PINS_DFPLAYER, 2},
    {TYPE_TM1637, -1, "TM1637", COST_TM1637, PINS_TM1637, 2},
    {TYPE_7SEG_7PIN, -1, "7-seg 7-pin", COST_7SEG_7PIN, PINS_7SEG_DIRECT, 7},
    {TYPE_7SEG_7PIN, 4, "7-seg 7-pin direct dim CA", COST_7SEG_7PIN, PINS_7SEG_DIMMED, 7},
    {TYPE_7SEG_7PIN, 5, "7-seg 7-pin direct dim CC", COST_7SEG_7PIN, PINS_7SEG_DIMMED, 7},
    {TYPE_7SEG_7PIN, 6, "7-seg 7-pin common anode dim", COST_7SEG_COMMON_DIM_7PIN, PINS_7SEG_COMMON_DIM, 8},
    {TYPE_7SEG_7PIN, 7, "7-seg 7-pin common cathode dim", COST_7SEG_COMMON_DIM_7PIN, PINS_7SEG_COMMON_DIM, 8},
    {TYPE_7SEG_8PIN, -1, "7-seg 8-pin", COST_7SEG_8PIN, PINS_7SEG_DIRECT, 8},
    {TYPE_7SEG_8PIN, 4, "7-seg 8-pin direct dim CA", COST_7SEG_8PIN, PINS_7SEG_DIMMED, 8},
    {TYPE_7SEG_8PIN, 5, "7-seg 8-pin direct dim CC", COST_7SEG_8PIN, PINS_7SEG_DIMMED, 8},
    {TYPE_7SEG_8PIN, 8, "7-seg 8-pin common anode dim", COST_7SEG_COMMON_DIM_8PIN, PINS_7SEG_COMMON_DIM, 9},
    {TYPE_7SEG_8PIN, 9, "7-seg 8-pin common cathode dim", COST_7SEG_COMMON_DIM_8PIN, PINS_7SEG_COMMON_DIM, 9},
    {TYPE_DAC, -1, "I2C DAC", COST_DAC, PINS_DAC, 1},
    {TYPE_PWM_DAC, -1, "PWM DAC", COST_PWM_DAC, PINS_PWM, 1},
    {TYPE_FUNC_GEN, -1, "Function generator", COST_FUNC_GEN, PINS_GPIO_MAIN, 1},
    {TYPE_SOLENOID, -1, "Solenoid", COST_SOLENOID, PINS_SOLENOID, 1},
    {TYPE_SMOKE, -1, "Smoke shooter", COST_SMOKE, PINS_SMOKE, 2}
};

inline const OutputModeDef* modeDef(uint8_t type, uint8_t mode) {
    const OutputModeDef* fallback = nullptr;
    for (const auto& def : OUTPUT_MODES) {
        if (def.type != type) continue;
        if (def.mode == (int8_t)mode) return &def;
        if (def.mode == -1) fallback = &def;
    }
    return fallback;
}

inline uint16_t baseCpuUs(uint8_t type, uint8_t mode = 0) {
    const OutputModeDef* def = modeDef(type, mode);
    return def ? def->cost.cpuUs : 0;
}

inline const PinRule* pinRule(uint8_t type, uint8_t mode, uint8_t slotIndex) {
    const OutputModeDef* def = modeDef(type, mode);
    if (def == nullptr || slotIndex >= def->pinCount) return nullptr;
    return &def->pins[slotIndex];
}

inline bool pinSlotUsesGpio(uint8_t type, uint8_t mode, uint8_t slotIndex, uint8_t routeSource) {
    const PinRule* rule = pinRule(type, mode, slotIndex);
    return rule != nullptr && routeSource == 0 && (rule->sources & SRC_GPIO);
}

// ─────────────────────────────────────
//  Type interface (Web UI contract) lookup
// ─────────────────────────────────────

inline const char* typeInterfaceName(uint8_t type) {
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

inline const TypeProtocol::FieldDef* typeExtraFields(uint8_t type, uint8_t& count) {
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

inline const TypeProtocol::TestCmdDef* typeTestCommands(uint8_t type, uint8_t& count) {
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

}  // namespace OutputDefs

#endif
