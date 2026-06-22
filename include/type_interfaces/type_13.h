#ifndef TYPE_13_H
#define TYPE_13_H

#include "type_protocol.h"

// ─────────────────────────────────────────────
//  Type 13 — 7-Segment Direct-Drive 8-Pin PWM
//  8 segments via GPIO or PCA9685.
//  Modes: -1=no dim, 4/5=direct dim, 8/9=common dim.
// ─────────────────────────────────────────────
namespace Type13 {
using namespace TypeProtocol;

constexpr uint8_t TYPE_ID = 13;
constexpr const char* TYPE_NAME = "7-Seg 8-Pin PWM";

constexpr const char* MODE_OPTS =
    "-1:No Dim,4:Direct CA,5:Direct CC,8:Common Anode,9:Common Cathode";

constexpr FieldDef EXTRA_FIELDS[] = {
    {"mc_mode", FT_SELECT, "Dim Mode", -1, 9, -1, MODE_OPTS},
    {"mc_invert", FT_BOOL, "Invert Segments", 0, 1, 0, nullptr}
};

constexpr TestCmdDef TEST_COMMANDS[] = {
    {"Apply Num",  0, "Set numeric value"},
    {"Clear",      1, "Clear all segments"},
    {"All On",     2, "Light all segments"}
};

}  // namespace Type13
#endif
