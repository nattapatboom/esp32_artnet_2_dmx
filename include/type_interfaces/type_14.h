#ifndef TYPE_14_H
#define TYPE_14_H

#include "type_protocol.h"

// ─────────────────────────────────────────────
//  Type 14 — I2C DAC
//  MCP4725, DAC7571 (single-channel) or DAC7573
//  (quad-channel via pca_channel 0-3).
//  Source MUST be I2C DAC (5-7).
// ─────────────────────────────────────────────
namespace Type14 {
using namespace TypeProtocol;

constexpr uint8_t TYPE_ID = 14;
constexpr const char* TYPE_NAME = "I2C DAC";

constexpr FieldDef EXTRA_FIELDS[] = {};

constexpr TestCmdDef TEST_COMMANDS[] = {
    {"Apply",  0, "Set DAC output value"},
    {"Min",    1, "Set to minimum"},
    {"Mid",    2, "Set to mid-scale"},
    {"Max",    3, "Set to maximum"}
};

}  // namespace Type14
#endif
