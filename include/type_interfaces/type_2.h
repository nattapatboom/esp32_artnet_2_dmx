#ifndef TYPE_2_H
#define TYPE_2_H

#include "type_protocol.h"

// ─────────────────────────────────────────────
//  Type 2 — Relay
//  Digital on/off output. GPIO, PCA9685, or
//  digital expander.
// ─────────────────────────────────────────────
namespace Type2 {
using namespace TypeProtocol;

constexpr uint8_t TYPE_ID = 2;
constexpr const char* TYPE_NAME = "Relay";

constexpr FieldDef EXTRA_FIELDS[] = {};

constexpr TestCmdDef TEST_COMMANDS[] = {
    {"On",  1, "Turn relay ON"},
    {"Off", 0, "Turn relay OFF"},
    {"Pulse", 2, "Toggle on/off briefly"}
};

}  // namespace Type2
#endif
