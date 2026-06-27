#ifndef OUTPUT_DEVICES_SEVEN_SEG_DIGITS_H
#define OUTPUT_DEVICES_SEVEN_SEG_DIGITS_H

#include <Arduino.h>

static constexpr uint8_t SEG_DIGITS[10] = {0x3f, 0x06, 0x5b, 0x4f, 0x66, 0x6d, 0x7d, 0x07, 0x7f, 0x6f};

inline uint8_t asciiToSegment(uint8_t c) {
    if (c >= 'a' && c <= 'z') c -= 32;
    if (c >= '0' && c <= '9') return SEG_DIGITS[c - '0'];
    switch (c) {
        case 'A': return 0x77; case 'B': return 0x7C; case 'C': return 0x39;
        case 'D': return 0x5E; case 'E': return 0x79; case 'F': return 0x71;
        case 'G': return 0x3D; case 'H': return 0x76; case 'I': return 0x06;
        case 'J': return 0x0E; case 'K': return 0x76; case 'L': return 0x38;
        case 'M': return 0x37; case 'N': return 0x54; case 'O': return 0x3F;
        case 'P': return 0x73; case 'Q': return 0x67; case 'R': return 0x50;
        case 'S': return 0x6D; case 'T': return 0x78; case 'U': return 0x3E;
        case 'V': return 0x1C; case 'W': return 0x3E; case 'X': return 0x40;
        case 'Y': return 0x6E; case 'Z': return 0x5B;
        case '-': return 0x40; case '_': return 0x08; case ' ': return 0x00;
        default:  return 0x00;
    }
}

#endif
