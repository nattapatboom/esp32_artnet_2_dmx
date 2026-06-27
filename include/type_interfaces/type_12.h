#ifndef TYPE_12_H
#define TYPE_12_H

#include "type_protocol.h"

// ─────────────────────────────────────────────
//  Type 12 — 7-Segment Direct-Drive 7-Pin PWM
//  7 segments + DP via GPIO or PCA9685.
//  Modes: -1=no dim, 4/5=direct dim, 6/7=common dim.
// ─────────────────────────────────────────────
namespace Type12 {
using namespace TypeProtocol;

constexpr uint8_t TYPE_ID = 12;
constexpr const char* TYPE_NAME = "7-Seg 7-Pin PWM";

constexpr const char* MODE_OPTS =
    "-1:No Dim,0:Direct CA,1:Direct CC,2:Common Anode,3:Common Cathode";
constexpr const char* RES_OPTS = "8:ASCII / Character,10:Numeric";

constexpr FieldDef EXTRA_FIELDS[] = {
    {"mc_mode", FT_SELECT, "Dim Mode", -1, 3, -1, MODE_OPTS},
    {"mc_resolution", FT_SELECT, "Decode Mode", 8, 10, 8, RES_OPTS},
    {"mc_freq", FT_NUMBER, "PWM Frequency (Hz)", 1, 40000, 1000, nullptr},
    {"mc_invert", FT_BOOL, "Invert Segments", 0, 1, 0, nullptr}
};

constexpr FieldDef TEST_FIELDS[] = {
    {"test_7seg_num",  FT_NUMBER, "Number",              0,   9999, 1234, nullptr},
    {"test_7seg_text", FT_TEXT,   "ASCII Text",          0,   4,    0,    nullptr}
};

constexpr TestCmdDef TEST_COMMANDS[] = {
    {"Apply Num",  0, "Set numeric value"},
    {"Clear",      1, "Clear all segments"},
    {"All On",     2, "Light all segments"}
};

}  // namespace Type12
#endif
