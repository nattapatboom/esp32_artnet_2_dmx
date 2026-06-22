#ifndef TYPE_PROTOCOL_H
#define TYPE_PROTOCOL_H

#include <Arduino.h>

// ─────────────────────────────────────────────
//  Type Interface Protocol
//  Defines the contract between firmware (C++)
//  and Web UI (JS) per output type.
//  Each type_N.h in include/type_interfaces/
//  fills in these definitions.
// ─────────────────────────────────────────────

namespace TypeProtocol {

// JSON field value types
enum FieldType : uint8_t {
    FT_NUMBER,   // integer input
    FT_FLOAT,    // decimal input
    FT_BOOL,     // checkbox
    FT_SELECT,   // dropdown
    FT_COLOR     // hex color
};

// Single config field definition
struct FieldDef {
    const char* key;       // JSON field name (must match OutputChannel key in JS)
    FieldType type;
    const char* label;     // Web UI label
    int32_t min;           // for FT_NUMBER/FT_FLOAT
    int32_t max;
    int32_t def;           // default value
    const char* opts;      // for FT_SELECT: comma-delimited "val:label,val:label"
};

// Test command definition
struct TestCmdDef {
    const char* label;     // Button label in Web UI
    uint8_t cmd;           // Command byte (if type uses a command byte)
    const char* desc;      // Tooltip / description
};

#define TYPEPROTO_ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

}  // namespace TypeProtocol

#endif
