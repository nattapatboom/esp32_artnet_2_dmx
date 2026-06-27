#ifndef TYPE_15_H
#define TYPE_15_H

#include "type_protocol.h"

// ─────────────────────────────────────────────
//  Type 15 — PWM DAC
//  Uses LEDC (GPIO) or PCA9685 with duty
//  calibration for 0-10V or 4-20mA output.
// ─────────────────────────────────────────────
namespace Type15 {
using namespace TypeProtocol;

constexpr uint8_t TYPE_ID = 15;
constexpr const char* TYPE_NAME = "PWM DAC";

constexpr const char* MODE_OPTS = "0:Custom,1:0-10V,2:4-20mA";
constexpr const char* RES_OPTS = "8:8-bit,10:10-bit,12:12-bit,16:16-bit";

constexpr FieldDef EXTRA_FIELDS[] = {
    {"mc_resolution", FT_SELECT, "Resolution", 8, 16, 8, RES_OPTS},
    {"mc_freq",       FT_NUMBER, "PWM Carrier Frequency (Hz)", 1, 40000, 1000, nullptr},
    {"pwm_dac_mode", FT_SELECT, "Output Mode", 0, 2, 0, MODE_OPTS},
    {"pwm_dac_min",  FT_NUMBER, "Min Duty (x0.01%)", 0, 10000, 0,     nullptr},
    {"pwm_dac_max",  FT_NUMBER, "Max Duty (x0.01%)", 0, 10000, 10000, nullptr}
};

constexpr FieldDef TEST_FIELDS[] = {
    {"test_level_num", FT_NUMBER, "Value",               0,   255, 128,  nullptr}
};

constexpr TestCmdDef TEST_COMMANDS[] = {
    {"Apply",  0, "Set DAC output level"},
    {"Min",    1, "Set to calibrated minimum"},
    {"Mid",    2, "Set to calibrated mid-scale"},
    {"Max",    3, "Set to calibrated maximum"}
};

}  // namespace Type15
#endif
