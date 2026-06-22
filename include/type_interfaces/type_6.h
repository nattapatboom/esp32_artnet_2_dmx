#ifndef TYPE_6_H
#define TYPE_6_H

#include "type_protocol.h"

// ─────────────────────────────────────────────
//  Type 6 — DC Motor
//  Modes: 0=PWM+DIR, 1=IN1+IN2, 2=IN1+IN2+EN.
//  Config: resolution, freq, deadband, invert,
//  brake, mode selection.
// ─────────────────────────────────────────────
namespace Type6 {
using namespace TypeProtocol;

constexpr uint8_t TYPE_ID = 6;
constexpr const char* TYPE_NAME = "DC Motor";

constexpr const char* MODE_OPTS = "0:PWM+DIR,1:IN1+IN2,2:IN1+IN2+EN";
constexpr const char* RES_OPTS = "8:8-bit,10:10-bit,12:12-bit,16:16-bit";

constexpr FieldDef EXTRA_FIELDS[] = {
    {"mc_mode",       FT_SELECT, "Motor Mode",     0,  2,  0,  MODE_OPTS},
    {"mc_resolution", FT_SELECT, "Resolution",     8,  16, 8,  RES_OPTS},
    {"mc_freq",       FT_NUMBER, "PWM Freq (Hz)",  1,  40000, 5000,  nullptr},
    {"mc_deadband",   FT_NUMBER, "Deadband",        0,  255, 0,    nullptr},
    {"mc_invert",     FT_BOOL,   "Invert Direction",0,  1,  0,    nullptr},
    {"mc_brake",      FT_BOOL,   "Brake Mode",      0,  1,  0,    nullptr}
};

constexpr TestCmdDef TEST_COMMANDS[] = {
    {"Stop",    0, "Stop motor (brake)"},
    {"Forward", 1, "Run forward at slider speed"},
    {"Reverse", 2, "Run reverse at slider speed"}
};

}  // namespace Type6
#endif
