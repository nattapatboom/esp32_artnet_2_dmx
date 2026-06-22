#ifndef TYPE_8_H
#define TYPE_8_H

#include "type_protocol.h"

// ─────────────────────────────────────────────
//  Type 8 — RC Servo
//  PWM pulse via GPIO LEDC or PCA9685.
//  Config: min_us, max_us, freq.
// ─────────────────────────────────────────────
namespace Type8 {
using namespace TypeProtocol;

constexpr uint8_t TYPE_ID = 8;
constexpr const char* TYPE_NAME = "RC Servo";

constexpr FieldDef EXTRA_FIELDS[] = {
    {"mc_min_us", FT_NUMBER, "Min Pulse (µs)",  500,  2500, 1000, nullptr},
    {"mc_max_us", FT_NUMBER, "Max Pulse (µs)",  500,  2500, 2000, nullptr},
    {"mc_freq",   FT_NUMBER, "Frequency (Hz)",  1,    40000, 50,   nullptr}
};

constexpr TestCmdDef TEST_COMMANDS[] = {
    {"Apply",   0, "Set servo angle/position"},
    {"Min",     1, "Set to min position"},
    {"Center",  2, "Set to center position"},
    {"Max",     3, "Set to max position"}
};

}  // namespace Type8
#endif
