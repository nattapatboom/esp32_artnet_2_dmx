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

constexpr FieldDef EXTRA_FIELDS[] = {};

constexpr TestCmdDef TEST_COMMANDS[] = {
    {"Apply",   0, "Set brightness level"},
    {"Min/Off", 1, "Set to 0"},
    {"Max",     2, "Set to 255"}
};

}  // namespace Type4
#endif
