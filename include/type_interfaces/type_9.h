#ifndef TYPE_9_H
#define TYPE_9_H

#include "type_protocol.h"

// ─────────────────────────────────────────────
//  Type 9 — Passive Buzzer
//  LEDC tone/PWM output on single GPIO.
//  Config: mc_freq, mc_resolution.
// ─────────────────────────────────────────────
namespace Type9 {
using namespace TypeProtocol;

constexpr uint8_t TYPE_ID = 9;
constexpr const char* TYPE_NAME = "Passive Buzzer";

constexpr const char* RES_OPTS = "8:8-bit,10:10-bit,12:12-bit,16:16-bit";

constexpr FieldDef EXTRA_FIELDS[] = {
    {"mc_freq",       FT_NUMBER, "Tone Freq (Hz)", 1,   10000, 1000, nullptr},
    {"mc_resolution", FT_SELECT, "Resolution",      8,   16,    8,    RES_OPTS},
    {"mc_mode",       FT_SELECT, "Mode",            0,   1,     0,    "0:Continuous,1:Timeout"}
};

constexpr TestCmdDef TEST_COMMANDS[] = {
    {"Play",  0, "Play tone at selected frequency"},
    {"Stop",  1, "Stop the tone"}
};

}  // namespace Type9
#endif
