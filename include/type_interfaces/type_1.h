#ifndef TYPE_1_H
#define TYPE_1_H

#include "type_protocol.h"

// ─────────────────────────────────────────────
//  Type 1 — DMX Serial Output
//  Uses UART (or RMT fallback). 512-byte buffer.
// ─────────────────────────────────────────────
namespace Type1 {
using namespace TypeProtocol;

constexpr uint8_t TYPE_ID = 1;
constexpr const char* TYPE_NAME = "DMX Serial";

// No extra config fields beyond base routing
constexpr FieldDef EXTRA_FIELDS[] = {};

constexpr FieldDef TEST_FIELDS[] = {
    {"test_dmx_ch",    FT_NUMBER, "DMX Channel",         1,   512, 1,    nullptr},
    {"test_level_num", FT_NUMBER, "Value",               0,   255, 128,  nullptr}
};

constexpr TestCmdDef TEST_COMMANDS[] = {
    {"Apply",     0, "Set single DMX channel value"},
    {"Min/Off",   1, "Set channel to 0"},
    {"Max",       2, "Set channel to 255"},
    {"Select Ch", 3, "Choose DMX channel to adjust"}
};

}  // namespace Type1
#endif
