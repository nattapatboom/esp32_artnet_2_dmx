#ifndef TYPE_5_H
#define TYPE_5_H

#include "type_protocol.h"

// ─────────────────────────────────────────────
//  Type 5 — Analog RGB/RGBW
//  3 or 4 independent PWM channels routed via
//  pin/pin2/pin3/pin4.
// ─────────────────────────────────────────────
namespace Type5 {
using namespace TypeProtocol;

constexpr uint8_t TYPE_ID = 5;
constexpr const char* TYPE_NAME = "Analog RGB/RGBW";

// Color order options (same as Type 3)
constexpr const char* COLOR_ORDER_OPTS = "0:RGB,1:RBG,2:GRB,3:GBR,4:RGBW,5:RBGW,6:GRBW,7:GBRW";
constexpr const char* RES_OPTS = "8:8-bit,10:10-bit,12:12-bit,16:16-bit";

constexpr FieldDef EXTRA_FIELDS[] = {
    {"mc_resolution", FT_SELECT, "Resolution",     8,  16,    8,    RES_OPTS},
    {"mc_freq",       FT_NUMBER, "Frequency (Hz)", 1,  40000, 1000, nullptr},
    {"color_order", FT_SELECT, "Color Order", 0, 7, 0, COLOR_ORDER_OPTS}
};

constexpr FieldDef TEST_FIELDS[] = {
    {"test_color", FT_COLOR, "Color", 0, 0, 0, nullptr},
    {"test_white", FT_NUMBER, "White", 0, 255, 0, nullptr}
};

constexpr TestCmdDef TEST_COMMANDS[] = {
    {"Apply",     0, "Set RGB/RGBW levels"},
    {"All Red",   1, "Set R=255, G=0, B=0"},
    {"All Green", 2, "Set R=0, G=255, B=0"},
    {"All Blue",  3, "Set R=0, G=0, B=255"},
    {"Off",       4, "Turn all channels off"}
};

}  // namespace Type5
#endif
