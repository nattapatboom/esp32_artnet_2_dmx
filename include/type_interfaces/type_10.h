#ifndef TYPE_10_H
#define TYPE_10_H

#include "type_protocol.h"

// ─────────────────────────────────────────────
//  Type 10 — DFPlayer MP3
//  Serial UART control. Plays MP3/WAV from SD.
//  TX/RX GPIO pins.
// ─────────────────────────────────────────────
namespace Type10 {
using namespace TypeProtocol;

constexpr uint8_t TYPE_ID = 10;
constexpr const char* TYPE_NAME = "DFPlayer MP3";

constexpr const char* MODE_OPTS = "0:Continuous (3ch),1:One-shot (3ch)";

constexpr FieldDef EXTRA_FIELDS[] = {
    {"mc_mode", FT_SELECT, "Play Mode", 0, 1, 0, MODE_OPTS}
};

constexpr TestCmdDef TEST_COMMANDS[] = {
    {"Play",     0, "Play track number"},
    {"Stop",     1, "Stop playback"},
    {"Set Vol",  2, "Set volume level"},
    {"Next",     3, "Next track"},
    {"Prev",     4, "Previous track"}
};

}  // namespace Type10
#endif
