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

constexpr ModeCost modeCost(uint16_t cpuUs, uint16_t extraRamBytes = 0, HardwareCost hardware = HW_NONE) {
    return {cpuUs, extraRamBytes, hardware};
}

constexpr ModeCost COST_DIMMER = modeCost(5);
constexpr ModeCost COST_DMX_SERIAL = modeCost(250, 0, HW_UART_1);
constexpr ModeCost COST_RELAY = modeCost(5);
constexpr ModeCost COST_LED_STRIP_BASE = modeCost(80, 0, HW_RMT_1);
constexpr ModeCost COST_SINGLE_LED = modeCost(6, 0, HW_LEDC_1);
constexpr ModeCost COST_ANALOG_RGBW = modeCost(18, 0, HW_LEDC_4);
constexpr ModeCost COST_MOTOR_PWM_DIR = modeCost(35, 0, HW_LEDC_2);
constexpr ModeCost COST_MOTOR_IN1_IN2 = modeCost(35, 0, HW_LEDC_1);
constexpr ModeCost COST_MOTOR_IN1_IN2_EN = modeCost(35, 0, HW_LEDC_2);
constexpr ModeCost COST_STEPPER = modeCost(80, 512, HW_RMT_2);
constexpr ModeCost COST_SERVO = modeCost(12, 0, HW_LEDC_1);
constexpr ModeCost COST_BUZZER = modeCost(35, 0, HW_LEDC_1);
constexpr ModeCost COST_DFPLAYER = modeCost(30, 260, HW_UART_1);
constexpr ModeCost COST_TM1637 = modeCost(900);
constexpr ModeCost COST_7SEG_7PIN = modeCost(30, 0, HW_LEDC_7);
constexpr ModeCost COST_7SEG_8PIN = modeCost(35, 0, HW_LEDC_8);
constexpr ModeCost COST_7SEG_COMMON_DIM_7PIN = modeCost(30, 0, HW_LEDC_1);
constexpr ModeCost COST_7SEG_COMMON_DIM_8PIN = modeCost(35, 0, HW_LEDC_1);
constexpr ModeCost COST_DAC = modeCost(10);
constexpr ModeCost COST_PWM_DAC = modeCost(6, 0, HW_LEDC_1);
constexpr ModeCost COST_FUNC_GEN = modeCost(120, 1120, HW_LEDC_1_TIMER_1);
constexpr ModeCost COST_SOLENOID = modeCost(10);
constexpr ModeCost COST_SMOKE = modeCost(25);

constexpr OutputModeDef OUTPUT_MODES[] = {
    {0, -1, "Triac dimmer", COST_DIMMER, PINS_GPIO_MAIN, 1},
    {1, -1, "DMX serial", COST_DMX_SERIAL, PINS_DMX, 1},
    {2, -1, "Relay", COST_RELAY, PINS_RELAY_DIGITAL, 1},
    {3, -1, "RGB/RGBW strip", COST_LED_STRIP_BASE, PINS_LED_STRIP, 1},
    {4, -1, "Single-color PWM", COST_SINGLE_LED, PINS_PWM, 1},
    {5, -1, "Analog RGB/RGBW", COST_ANALOG_RGBW, PINS_ANALOG_RGB, 4},
    {6, 0, "PWM + DIR", COST_MOTOR_PWM_DIR, PINS_MOTOR_PWM_DIR, 2},
    {6, 1, "IN1 + IN2", COST_MOTOR_IN1_IN2, PINS_MOTOR_PWM_DIR, 2},
    {6, 2, "IN1 + IN2 + EN", COST_MOTOR_IN1_IN2_EN, PINS_MOTOR_IN1_IN2_EN, 3},
    {7, -1, "Stepper", COST_STEPPER, PINS_STEPPER, 4},
    {8, -1, "RC servo", COST_SERVO, PINS_PWM, 1},
    {9, -1, "Passive buzzer", COST_BUZZER, PINS_GPIO_MAIN, 1},
    {10, -1, "DFPlayer", COST_DFPLAYER, PINS_DFPLAYER, 2},
    {11, -1, "TM1637", COST_TM1637, PINS_TM1637, 2},
    {12, -1, "7-seg 7-pin", COST_7SEG_7PIN, PINS_7SEG_DIRECT, 2},
    {13, -1, "7-seg 8-pin", COST_7SEG_8PIN, PINS_7SEG_DIRECT, 2},
    {14, -1, "I2C DAC", COST_DAC, PINS_DAC, 1},
    {15, -1, "PWM DAC", COST_PWM_DAC, PINS_PWM, 1},
    {16, -1, "Function generator", COST_FUNC_GEN, PINS_GPIO_MAIN, 1},
    {17, -1, "Solenoid", COST_SOLENOID, PINS_SOLENOID, 1},
    {18, -1, "Smoke shooter", COST_SMOKE, PINS_SMOKE, 2}
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
