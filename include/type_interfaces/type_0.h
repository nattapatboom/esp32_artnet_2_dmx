#ifndef TYPE_0_H
#define TYPE_0_H

#include "type_protocol.h"

// ─────────────────────────────────────────────
//  Type 0 — AC Dimmer
//  Uses shared zero-crossing pin + hardware timer.
//  Single GPIO output, no extra config fields.
// ─────────────────────────────────────────────
namespace Type0 {
using namespace TypeProtocol;

constexpr uint8_t TYPE_ID = 0;
constexpr const char* TYPE_NAME = "AC Dimmer";

// No extra fields beyond base routing (pin, source, DMX addr)
constexpr FieldDef EXTRA_FIELDS[] = {};

// Test: single value slider (0-255 level)
constexpr TestCmdDef TEST_COMMANDS[] = {
    {"Apply",    0, "Set dimmer level"},
    {"Off/Min",  1, "Set to 0"},
    {"Max",      2, "Set to 255"}
};

constexpr FieldDef* extraFields() { return nullptr; }
constexpr uint8_t extraFieldCount() { return 0; }

}  // namespace Type0
#endif
