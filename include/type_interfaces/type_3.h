#ifndef TYPE_3_H
#define TYPE_3_H

#include "type_protocol.h"

// ─────────────────────────────────────────────
//  Type 3 — RGB/RGBW LED Strip
//  Uses RMT + NeoPixelBus. Extra fields for
//  LED count, color order, protocol.
// ─────────────────────────────────────────────
namespace Type3 {
using namespace TypeProtocol;

constexpr uint8_t TYPE_ID = 3;
constexpr const char* TYPE_NAME = "RGB/RGBW LED Strip";

// Color order options
constexpr const char* COLOR_ORDER_OPTS = "0:RGB,1:RBG,2:GRB,3:GBR,4:RGBW,5:RBGW,6:GRBW,7:GBRW";

// Protocol options
constexpr const char* PROTOCOL_OPTS = "0:WS2812B/WS2811,1:SK6812,2:WS2813,3:CS8812";

constexpr FieldDef EXTRA_FIELDS[] = {
    {"led_count",     FT_NUMBER, "LED Count",      1,  1360, 170,   nullptr},
    {"color_order",   FT_SELECT, "Color Order",     0,  7,    0,     COLOR_ORDER_OPTS},
    {"led_protocol",  FT_SELECT, "Protocol",        0,  3,    0,     PROTOCOL_OPTS}
};

constexpr FieldDef TEST_FIELDS[] = {
    {"test_color", FT_COLOR, "Color", 0, 0, 0, nullptr},
    {"test_white", FT_NUMBER, "White", 0, 255, 0, nullptr},
    {"test_pixel", FT_NUMBER, "Pixel Number", 1, 1360, 1, nullptr}
};

constexpr TestCmdDef TEST_COMMANDS[] = {
    {"All Custom",   0, "Set all LEDs to selected color"},
    {"All Red",      1, "Set all LEDs red"},
    {"All Green",    2, "Set all LEDs green"},
    {"All Blue",     3, "Set all LEDs blue"},
    {"Pixel Custom", 4, "Set single pixel to selected color"},
    {"Pixel Red",    5, "Set single pixel red"},
    {"Pixel Green",  6, "Set single pixel green"},
    {"Pixel Blue",   7, "Set single pixel blue"},
    {"Blackout",     8, "Turn all LEDs off"}
};

}  // namespace Type3
#endif
