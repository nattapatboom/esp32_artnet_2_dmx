#ifndef TYPE_11_H
#define TYPE_11_H

#include "type_protocol.h"

// ─────────────────────────────────────────────
//  Type 11 — 7-Segment TM1637
//  Bit-bang I2C-like display driver.
//  CLK + DIO GPIO pins. 4-digit numeric.
// ─────────────────────────────────────────────
namespace Type11 {
using namespace TypeProtocol;

constexpr uint8_t TYPE_ID = 11;
constexpr const char* TYPE_NAME = "TM1637 7-Segment";

constexpr const char* MODE_OPTS = "0:Numeric,1:ASCII";

constexpr FieldDef EXTRA_FIELDS[] = {
    {"mc_mode", FT_SELECT, "Display Mode", 0, 1, 0, MODE_OPTS}
};

constexpr FieldDef TEST_FIELDS[] = {
    {"test_7seg_num",  FT_NUMBER, "Number",              0,   9999, 1234, nullptr},
    {"test_7seg_text", FT_TEXT,   "ASCII Text",          0,   4,    0,    nullptr}
};

constexpr TestCmdDef TEST_COMMANDS[] = {
    {"Apply Num",    0, "Set numeric value (0-9999)"},
    {"Apply Text",   1, "Set ASCII text (4 chars)"},
    {"Clear",        2, "Clear display"}
};

}  // namespace Type11
#endif
