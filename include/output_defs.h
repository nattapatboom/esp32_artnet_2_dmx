#ifndef OUTPUT_DEFS_H
#define OUTPUT_DEFS_H

#include <Arduino.h>

namespace OutputDefs {

enum SourceMask : uint8_t {
    SRC_GPIO = 1 << 0,
    SRC_PCA = 1 << 1,
    SRC_DIGITAL_EXPANDER = 1 << 2,
    SRC_I2C_DAC = 1 << 3
};

enum PinDirection : uint8_t {
    PIN_OUTPUT,
    PIN_INPUT
};

struct PinRule {
    const char* slot;
    const char* label;
    uint8_t sources;
    PinDirection direction;
    bool invertible;
};

struct ModeCost {
    uint16_t cpuUs;
    uint16_t extraRamBytes;
    uint8_t ledc;
    uint8_t rmt;
    uint8_t uart;
    uint8_t dac;
    uint8_t timer;
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
    {"pin1", "Main", SRC_GPIO, PIN_OUTPUT, true}
};
constexpr PinRule PINS_DMX[] = {
    {"pin1", "DMX TX", SRC_GPIO, PIN_OUTPUT, true}
};
constexpr PinRule PINS_RELAY_DIGITAL[] = {
    {"pin1", "Relay", SRC_GPIO | SRC_PCA | SRC_DIGITAL_EXPANDER, PIN_OUTPUT, true}
};
constexpr PinRule PINS_LED_STRIP[] = {
    {"pin1", "Data", SRC_GPIO, PIN_OUTPUT, true}
};
constexpr PinRule PINS_PWM[] = {
    {"pin1", "PWM", SRC_GPIO | SRC_PCA, PIN_OUTPUT, true}
};
constexpr PinRule PINS_ANALOG_RGB[] = {
    {"pin1", "Red", SRC_GPIO | SRC_PCA, PIN_OUTPUT, true},
    {"pin2", "Green", SRC_GPIO | SRC_PCA, PIN_OUTPUT, true},
    {"pin3", "Blue", SRC_GPIO | SRC_PCA, PIN_OUTPUT, true},
    {"pin4", "White", SRC_GPIO | SRC_PCA, PIN_OUTPUT, true}
};
constexpr PinRule PINS_MOTOR_PWM_DIR[] = {
    {"pin1", "PWM", SRC_GPIO | SRC_PCA, PIN_OUTPUT, true},
    {"pin2", "DIR", SRC_GPIO | SRC_PCA | SRC_DIGITAL_EXPANDER, PIN_OUTPUT, true}
};
constexpr PinRule PINS_MOTOR_IN1_IN2_EN[] = {
    {"pin1", "IN1", SRC_GPIO | SRC_PCA, PIN_OUTPUT, true},
    {"pin2", "IN2", SRC_GPIO | SRC_PCA | SRC_DIGITAL_EXPANDER, PIN_OUTPUT, true},
    {"pin3", "EN PWM", SRC_GPIO | SRC_PCA, PIN_OUTPUT, true}
};
constexpr PinRule PINS_STEPPER[] = {
    {"pin1", "STEP", SRC_GPIO, PIN_OUTPUT, true},
    {"pin2", "DIR", SRC_GPIO | SRC_DIGITAL_EXPANDER, PIN_OUTPUT, true},
    {"pin3", "ENABLE", SRC_GPIO | SRC_DIGITAL_EXPANDER, PIN_OUTPUT, true},
    {"pin4", "HOME", SRC_GPIO | SRC_DIGITAL_EXPANDER, PIN_INPUT, true}
};
constexpr PinRule PINS_DFPLAYER[] = {
    {"pin1", "TX", SRC_GPIO, PIN_OUTPUT, false},
    {"pin2", "RX", SRC_GPIO, PIN_INPUT, false}
};
constexpr PinRule PINS_TM1637[] = {
    {"pin1", "CLK", SRC_GPIO, PIN_OUTPUT, true},
    {"pin2", "DIO", SRC_GPIO, PIN_OUTPUT, true}
};
constexpr PinRule PINS_7SEG_DIRECT[] = {
    {"pin1", "Segment A", SRC_GPIO | SRC_PCA, PIN_OUTPUT, true},
    {"segments", "Segments", SRC_GPIO | SRC_PCA | SRC_DIGITAL_EXPANDER, PIN_OUTPUT, true}
};
constexpr PinRule PINS_7SEG_DIMMED[] = {
    {"segments", "Dimmed Segments", SRC_GPIO | SRC_PCA, PIN_OUTPUT, true}
};
constexpr PinRule PINS_7SEG_COMMON_DIM[] = {
    {"pin1", "Common PWM", SRC_GPIO | SRC_PCA, PIN_OUTPUT, true},
    {"segments", "Segments", SRC_GPIO | SRC_PCA | SRC_DIGITAL_EXPANDER, PIN_OUTPUT, true}
};
constexpr PinRule PINS_DAC[] = {
    {"pin1", "I2C DAC", SRC_I2C_DAC, PIN_OUTPUT, false}
};
constexpr PinRule PINS_SOLENOID[] = {
    {"pin1", "Solenoid", SRC_GPIO | SRC_PCA | SRC_DIGITAL_EXPANDER, PIN_OUTPUT, true}
};
constexpr PinRule PINS_SMOKE[] = {
    {"pin1", "Smoke Valve", SRC_GPIO | SRC_PCA | SRC_DIGITAL_EXPANDER, PIN_OUTPUT, true},
    {"pin2", "Shoot Valve", SRC_GPIO | SRC_PCA | SRC_DIGITAL_EXPANDER, PIN_OUTPUT, true}
};

constexpr OutputModeDef OUTPUT_MODES[] = {
    {0, -1, "Triac dimmer", {5, 0, 0, 0, 0, 0, 0}, PINS_GPIO_MAIN, 1},
    {1, -1, "DMX serial", {250, 0, 0, 0, 1, 0, 0}, PINS_DMX, 1},
    {2, -1, "Relay", {5, 0, 0, 0, 0, 0, 0}, PINS_RELAY_DIGITAL, 1},
    {3, -1, "RGB/RGBW strip", {80, 0, 0, 1, 0, 0, 0}, PINS_LED_STRIP, 1},
    {4, -1, "Single-color PWM", {6, 0, 1, 0, 0, 0, 0}, PINS_PWM, 1},
    {5, -1, "Analog RGB/RGBW", {18, 0, 4, 0, 0, 0, 0}, PINS_ANALOG_RGB, 4},
    {6, 0, "PWM + DIR", {35, 0, 2, 0, 0, 0, 0}, PINS_MOTOR_PWM_DIR, 2},
    {6, 1, "IN1 + IN2", {35, 0, 1, 0, 0, 0, 0}, PINS_MOTOR_PWM_DIR, 2},
    {6, 2, "IN1 + IN2 + EN", {35, 0, 2, 0, 0, 0, 0}, PINS_MOTOR_IN1_IN2_EN, 3},
    {7, -1, "Stepper", {80, 512, 0, 2, 0, 0, 0}, PINS_STEPPER, 4},
    {8, -1, "RC servo", {12, 0, 1, 0, 0, 0, 0}, PINS_PWM, 1},
    {9, -1, "Passive buzzer", {35, 0, 1, 0, 0, 0, 0}, PINS_GPIO_MAIN, 1},
    {10, -1, "DFPlayer", {30, 260, 0, 0, 1, 0, 0}, PINS_DFPLAYER, 2},
    {11, -1, "TM1637", {900, 0, 0, 0, 0, 0, 0}, PINS_TM1637, 2},
    {12, -1, "7-seg 7-pin", {30, 0, 7, 0, 0, 0, 0}, PINS_7SEG_DIRECT, 2},
    {13, -1, "7-seg 8-pin", {35, 0, 8, 0, 0, 0, 0}, PINS_7SEG_DIRECT, 2},
    {14, -1, "I2C DAC", {10, 0, 0, 0, 0, 0, 0}, PINS_DAC, 1},
    {15, -1, "PWM DAC", {6, 0, 1, 0, 0, 0, 0}, PINS_PWM, 1},
    {16, -1, "Function generator", {120, 1120, 1, 0, 0, 0, 1}, PINS_GPIO_MAIN, 1},
    {17, -1, "Solenoid", {10, 0, 0, 0, 0, 0, 0}, PINS_SOLENOID, 1},
    {18, -1, "Smoke shooter", {25, 0, 0, 0, 0, 0, 0}, PINS_SMOKE, 2}
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

}  // namespace OutputDefs

#endif
