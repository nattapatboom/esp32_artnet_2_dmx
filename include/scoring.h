#ifndef SCORING_H
#define SCORING_H

#include <Arduino.h>
#include "output_control.h"

#define SCORE_LIMIT 100.0f

inline float scoreForType(uint8_t type, uint16_t ledCount = 0) {
    switch (type) {
        case 0: return 2.0f + (ledCount / 100.0f) * 1.0f;
        case 1: return 4.0f;
        case 2: return 0.5f;
        case 3: return 2.0f;
        case 5: return 3.0f;
        case 6: return 3.0f;
        case 7: return 8.0f;
        case 8: return 2.0f;
        case 9: return 0.5f;
        case 10: return 3.0f;
        case 11: return 1.5f;
        case 12: return 1.5f;
        case 13: return 2.0f;
        case 15: return 2.0f;
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
        case 0: return "LED Strip";
        case 1: return "DMX Output";
        case 2: return "Relay";
        case 3: return "AC Dimmer";
        case 5: return "PWM Dimmer";
        case 6: return "DC Motor";
        case 7: return "Stepper";
        case 8: return "RC Servo";
        case 9: return "Solenoid";
        case 10: return "Analog RGB";
        case 11: return "Buzzer";
        case 12: return "Smoke Shooter";
        case 13: return "7-Segment";
        case 15: return "DFPlayer MP3";
        default: return "Unknown";
    }
}

#endif
