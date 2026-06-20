#ifndef SCORING_H
#define SCORING_H

#include <Arduino.h>
#include "output_control.h"

#define SCORE_LIMIT 100.0f

inline float scoreForType(uint8_t type, uint16_t ledCount = 0) {
    switch (type) {
        case 0: return 2.0f; // AC Dimmer
        case 1: return 4.0f; // DMX Output
        case 2: return 0.5f; // Relay
        case 3: return 2.0f + (ledCount / 100.0f) * 1.0f; // RGB LED
        case 4: return 3.0f; // Single Color LED
        case 5: return 3.0f; // Analog RGB
        case 6: return 8.0f; // Motor
        case 7: return 2.0f; // Stepper
        case 8: return 2.0f; // Servo
        case 9: return 3.0f; // Buzzer
        case 10: return 1.5f; // DFPlayer
        case 11: return 2.0f; // 7-Seg 2-pin
        case 12: return 3.0f; // 7-Seg DD PWM 7-pin (NEW)
        case 13: return 3.0f; // 7-Seg DD PWM 8-pin (NEW)
        case 14: return 0.5f; // DAC
        case 15: return 2.0f; // PWM DAC
        case 16: return 3.0f; // Func Gen
        case 17: return 0.5f; // Solenoid
        case 18: return 1.5f; // Smoke Shooter
        default: return 0.0f;
    }
}

inline float outputChannelScore(const OutputChannel& ch) {
    return scoreForType(ch.type, ch.led_count);
}

inline float totalOutputScore(const std::vector<OutputChannel>& channels) {
    float total = 0;
    for (const auto& ch : channels) {
        total += outputChannelScore(ch);
    }
    return total;
}

inline float totalOutputScoreFromJson(JsonArrayConst outputs) {
    float total = 0;
    for (JsonObjectConst ch : outputs) {
        total += scoreForType(ch["type"] | 0, ch["led_count"] | 0);
    }
    return total;
}

inline const char* outputTypeName(uint8_t type) {
    switch (type) {
        case 0: return "AC Dimmer";
        case 1: return "DMX Output";
        case 2: return "Relay";
        case 3: return "RGB LED";
        case 4: return "Single Color LED";
        case 5: return "Analog RGB";
        case 6: return "Motor";
        case 7: return "Stepper";
        case 8: return "Servo";
        case 9: return "Buzzer";
        case 10: return "DFPlayer MP3";
        case 11: return "7-Segment 2-Pin";
        case 12: return "7-Segment DD 7-Pin PWM";
        case 13: return "7-Segment DD 8-Pin PWM";
        case 14: return "DAC";
        case 15: return "PWM DAC";
        case 16: return "Function Gen";
        case 17: return "Solenoid";
        case 18: return "Smoke Shooter";
        default: return "Unknown";
    }
}

#endif
