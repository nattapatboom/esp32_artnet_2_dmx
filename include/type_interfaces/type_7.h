#ifndef TYPE_7_H
#define TYPE_7_H

#include "type_protocol.h"

// ─────────────────────────────────────────────
//  Type 7 — Stepper Motor
//  RMT-based STEP pulse, DIR + ENABLE + HOME pins.
//  Position tracking + homing support.
// ─────────────────────────────────────────────
namespace Type7 {
using namespace TypeProtocol;

constexpr uint8_t TYPE_ID = 7;
constexpr const char* TYPE_NAME = "Stepper Motor";

constexpr const char* RES_OPTS = "8:8-bit,10:10-bit,12:12-bit,16:16-bit,24:24-bit,32:32-bit";
constexpr const char* UNIT_OPTS = "0:Steps,1:Degrees,2:Millimeters";
constexpr const char* HOME_MODE_OPTS = "0:Sensor Pin,1:Time/Stall";

constexpr FieldDef EXTRA_FIELDS[] = {
    {"mc_resolution",      FT_SELECT, "Resolution",         8,  32, 8,   RES_OPTS},
    {"mc_freq",            FT_NUMBER, "Speed (Hz)",          1,  40000, 1000, nullptr},
    {"mc_steps_per_rev",   FT_NUMBER, "Steps/Rev",           1,  10000, 200,  nullptr},
    {"mc_unit_type",       FT_SELECT, "Unit Type",           0,  2,  0,   UNIT_OPTS},
    {"mc_scale_factor",    FT_FLOAT,  "Scale Factor",        0,  10000, 0,   nullptr},
    {"mc_homing_mode",     FT_SELECT, "Homing Mode",         0,  1,  0,   HOME_MODE_OPTS},
    {"mc_homing_dir",      FT_SELECT, "Homing Direction",    0,  1,  0,   "0:Forward,1:Reverse"},
    {"mc_homing_speed",    FT_NUMBER, "Homing Speed (Hz)",   1,  40000, 500,  nullptr},
    {"mc_homing_timeout",  FT_NUMBER, "Homing Timeout (s)",  1,  100, 5,    nullptr},
    {"mc_invert",          FT_BOOL,   "Invert Direction",    0,  1,  0,    nullptr},
    {"mc_enable_active_high", FT_BOOL, "Enable Active High", 0,  1,  0,    nullptr}
};

constexpr TestCmdDef TEST_COMMANDS[] = {
    {"Move",    0,   "Move to target position at set speed"},
    {"Stop Cmd", 120, "Send stop command"},
    {"Home Cmd", 220, "Start homing sequence"}
};

}  // namespace Type7
#endif
