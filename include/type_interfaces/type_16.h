#ifndef TYPE_16_H
#define TYPE_16_H

#include "type_protocol.h"

// ─────────────────────────────────────────────
//  Type 16 — Function Generator
//  LEDC + esp_timer for sine/triangle/sawtooth
//  waveform output. Uses hardware timer.
// ─────────────────────────────────────────────
namespace Type16 {
using namespace TypeProtocol;

constexpr uint8_t TYPE_ID = 16;
constexpr const char* TYPE_NAME = "Function Generator";

constexpr const char* WAVE_OPTS = "0:Sine,1:Triangle,2:Sawtooth,3:Square";

constexpr FieldDef EXTRA_FIELDS[] = {
    {"mc_freq",       FT_NUMBER, "Frequency (Hz)",  1,   5000, 100,    nullptr},
    {"mc_resolution", FT_NUMBER, "Resolution (bit)", 8,  16,   8,      nullptr},
    {"mc_mode",       FT_SELECT, "Waveform",         0,  3,    0,      WAVE_OPTS}
};

constexpr TestCmdDef TEST_COMMANDS[] = {
    {"Start",   0, "Start waveform output"},
    {"Stop",    1, "Stop waveform output"},
    {"Sine",    2, "Switch to sine wave"},
    {"Triangle",3, "Switch to triangle wave"},
    {"Saw",     4, "Switch to sawtooth"},
    {"Square",  5, "Switch to square wave"}
};

}  // namespace Type16
#endif
