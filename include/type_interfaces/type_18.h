#ifndef TYPE_18_H
#define TYPE_18_H

#include "type_protocol.h"

// ─────────────────────────────────────────────
//  Type 18 — Smoke Shooter
//  Dual-valve sequential smoke machine control.
//  Pin 1 = Smoke Valve, Pin 2 = Shoot Valve.
//  Sequence: Smoke ON → Settle → Shoot ON → OFF → Cooldown.
// ─────────────────────────────────────────────
namespace Type18 {
using namespace TypeProtocol;

constexpr uint8_t TYPE_ID = 18;
constexpr const char* TYPE_NAME = "Smoke Shooter";

constexpr FieldDef EXTRA_FIELDS[] = {
    {"solenoid_threshold", FT_NUMBER, "Trigger Threshold",  0,   255,   127,   nullptr},
    {"smoke_duration_ms",  FT_NUMBER, "Smoke Duration (ms)",0,   10000, 1000,  nullptr},
    {"settle_delay_ms",    FT_NUMBER, "Settle Delay (ms)",  0,   10000, 500,   nullptr},
    {"shoot_duration_ms",  FT_NUMBER, "Shoot Duration (ms)",0,   10000, 1000,  nullptr},
    {"smoke_lockout_ms",   FT_NUMBER, "Lockout (ms)",       0,   30000, 2000,  nullptr}
};

constexpr TestCmdDef TEST_COMMANDS[] = {
    {"Fire Full", 0, "Run full smoke → settle → shoot sequence"},
    {"Smoke On",  1, "Turn smoke valve ON"},
    {"Smoke Off", 2, "Turn smoke valve OFF"},
    {"Shoot On",  3, "Turn shoot valve ON"},
    {"Shoot Off", 4, "Turn shoot valve OFF"}
};

}  // namespace Type18
#endif
