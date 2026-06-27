#ifndef TYPE_4_H
#define TYPE_4_H

#include "type_protocol.h"

// ─────────────────────────────────────────────
//  Type 4 — Single Color LED
//  PWM LED via GPIO (LEDC) or PCA9685.
// ─────────────────────────────────────────────
namespace Type4 {
using namespace TypeProtocol;

constexpr uint8_t TYPE_ID = 4;
constexpr const char* TYPE_NAME = "Single-Color PWM";

constexpr const char* RES_OPTS = "8:8-bit,10:10-bit,12:12-bit,16:16-bit";

constexpr FieldDef EXTRA_FIELDS[] = {
    {"mc_resolution", FT_SELECT, "Resolution",     8,  16,    8,    RES_OPTS},
    {"mc_freq",       FT_NUMBER, "Frequency (Hz)", 1,  40000, 1000, nullptr}
};

constexpr FieldDef TEST_FIELDS[] = {
    {"test_level_num", FT_NUMBER, "Value",               0,   255, 128,  nullptr}
};

constexpr TestCmdDef TEST_COMMANDS[] = {
    {"Apply",   0, "Set brightness level"},
    {"Min/Off", 1, "Set to 0"},
    {"Max",     2, "Set to 255"}
};

}  // namespace Type4
#endif
