#ifndef TYPE_17_H
#define TYPE_17_H

#include "type_protocol.h"

// ─────────────────────────────────────────────
//  Type 17 — Solenoid
//  Pulse-activated solenoid with configurable
//  pulse duration, threshold, and delays.
//  GPIO, PCA9685, or digital expander.
// ─────────────────────────────────────────────
namespace Type17 {
using namespace TypeProtocol;

constexpr uint8_t TYPE_ID = 17;
constexpr const char* TYPE_NAME = "Solenoid";

constexpr const char* MODE_OPTS = "0:Toggle,1:One-shot Pulse";

constexpr FieldDef EXTRA_FIELDS[] = {
    {"solenoid_mode",       FT_SELECT, "Mode",              0,  1,     0,     MODE_OPTS},
    {"solenoid_threshold",  FT_NUMBER, "Trigger Threshold",  0,  255,   127,   nullptr},
    {"solenoid_pulse_ms",   FT_NUMBER, "Pulse Duration (ms)",1,  10000, 50,    nullptr},
    {"solenoid_pre_delay",  FT_NUMBER, "Pre-delay (ms)",     0,  5000,  0,     nullptr},
    {"solenoid_post_delay", FT_NUMBER, "Post-delay (ms)",    0,  5000,  100,   nullptr}
};

constexpr TestCmdDef TEST_COMMANDS[] = {
    {"Fire",  0, "Fire solenoid pulse"},
    {"On",    1, "Turn solenoid on (toggle)"},
    {"Off",   2, "Turn solenoid off (toggle)"}
};

}  // namespace Type17
#endif
