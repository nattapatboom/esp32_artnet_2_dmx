#ifndef SCORING_H
#define SCORING_H

#include <Arduino.h>
#include "output_control.h"

constexpr float W_GPIO = 0.5f;
constexpr float W_LEDC = 2.5f;
constexpr float W_RMT = 3.0f;
constexpr float W_UART = 8.0f;
constexpr float W_DAC = 2.0f;
constexpr float W_PCA = 0.25f;
constexpr float W_EXP = 0.125f;

constexpr uint8_t MAX_GPIO_RESOURCE = 8;
constexpr uint8_t MAX_LEDC_RESOURCE = 16;
constexpr uint8_t MAX_RMT_RESOURCE = 8;
constexpr uint8_t MAX_UART_RESOURCE = 2;
constexpr uint8_t MAX_DAC_RESOURCE = 0;

inline constexpr float resourceScoreLimit() {
    return MAX_GPIO_RESOURCE * W_GPIO +
           MAX_LEDC_RESOURCE * W_LEDC +
           MAX_RMT_RESOURCE * W_RMT +
           MAX_UART_RESOURCE * W_UART +
           MAX_DAC_RESOURCE * W_DAC;
}

constexpr float MAX_COMPUTE_SCORE = 25.0f;

inline constexpr float totalScoreLimit() {
    return resourceScoreLimit() + MAX_COMPUTE_SCORE;
}

#define SCORE_LIMIT totalScoreLimit()

struct ResourceUsage {
    uint8_t gpio = 0;
    uint8_t ledc = 0;
    uint8_t rmt = 0;
    uint8_t uart = 0;
    uint8_t dac = 0;
    uint8_t pca = 0;
    uint8_t expander = 0;
};

inline ResourceUsage estimateResources(const OutputChannel& ch) {
    ResourceUsage u;
    switch (ch.type) {
        case 0:
            if (ch.source == 0) u.gpio = 1;
            break;
        case 1:
            if (ch.source == 0) u.gpio = 1;
            u.uart = 1;
            break;
        case 2:
            if (ch.source == 0) u.gpio = 1;
            else if (ch.source == 1) u.pca = 1;
            else u.expander = 1;
            break;
        case 3:
            if (ch.source == 0) u.gpio = 1;
            break;
        case 4:
            if (ch.source == 0) { u.gpio = 1; u.ledc = 1; }
            else if (ch.source == 1) u.pca = 1;
            else u.expander = 1;
            break;
        case 5:
            // Red
            if (ch.source == 0) { u.gpio++; u.ledc++; }
            else if (ch.source == 1) { u.pca++; }

            // Green
            if (ch.pin2_source == 0) { u.gpio++; u.ledc++; }
            else if (ch.pin2_source == 1) { u.pca++; }

            // Blue
            if (ch.pin3_source == 0) { u.gpio++; u.ledc++; }
            else if (ch.pin3_source == 1) { u.pca++; }

            // White
            if (ch.color_order >= 4) {
                if (ch.pin4_source == 0) { u.gpio++; u.ledc++; }
                else if (ch.pin4_source == 1) { u.pca++; }
            }
            break;
        case 6:
            if (ch.source == 0) {
                u.gpio = (ch.mc_mode == 2) ? 3 : 2;
                u.ledc = (ch.mc_mode == 0) ? 2 : 1;
            } else if (ch.source == 1) {
                u.pca = (ch.mc_mode == 2) ? 3 : 2;
            }
            break;
        case 7:
            if (ch.source == 0) {
                u.gpio = 2 + (ch.pin4 != 255 && ch.mc_homing_mode == 0 ? 1 : 0);
                u.rmt = 2;
            } else if (ch.source == 1) {
                u.pca = 2;
            }
            break;
        case 8:
            if (ch.source == 0) { u.gpio = 1; u.ledc = 1; }
            else if (ch.source == 1) u.pca = 1;
            break;
        case 9:
            if (ch.source == 0) { u.gpio = 1; u.ledc = 1; }
            break;
        case 10:
            if (ch.source == 0) u.gpio = 2;
            u.uart = 1;
            break;
        case 11:
            if (ch.source == 0) u.gpio = 2;
            else u.expander = 2;
            break;
        case 12:
            u.gpio = 7; u.ledc = 7;
            break;
        case 13:
            u.gpio = 8; u.ledc = 8;
            break;
        case 14:
            if (ch.source == 5) u.expander = 1;
            else { u.gpio = 1; u.dac = 1; }
            break;
        case 15:
            if (ch.source == 0) { u.gpio = 1; u.ledc = 1; }
            else if (ch.source == 1) u.pca = 1;
            break;
        case 16:
            u.gpio = 1; u.ledc = 1;
            break;
        case 17:
            if (ch.source == 0) u.gpio = 1;
            else if (ch.source == 1) u.pca = 1;
            else u.expander = 1;
            break;
        case 18:
            if (ch.source == 0) u.gpio = 2;
            else if (ch.source == 1) u.pca = 2;
            else u.expander = 2;
            break;
    }
    return u;
}

inline float resourceScore(const ResourceUsage& u) {
    return u.gpio * W_GPIO +
           u.ledc * W_LEDC +
           u.rmt * W_RMT +
           u.uart * W_UART +
           u.dac * W_DAC +
           u.pca * W_PCA +
           u.expander * W_EXP;
}

inline float outputChannelScore(const OutputChannel& ch) {
    return resourceScore(estimateResources(ch));
}

inline float channelComputeScore(const OutputChannel& ch) {
    switch (ch.type) {
        case 3: return ch.led_count * 0.005f;
        case 6: return 0.5f;
        case 7: return 2.0f;
        case 10: return 0.5f;
        case 11: return 0.5f;
        case 12: return 1.0f;
        case 13: return 1.2f;
        case 16: return 2.0f;
        case 18: return 0.3f;
        default: return 0;
    }
}

inline float fpsComputeFactor(uint8_t outputFps) {
    if (outputFps == 0) outputFps = 40;
    return (outputFps / 60.0f) * 5.0f;
}

inline float totalComputeScore(const std::vector<OutputChannel>& channels, uint8_t outputFps = 40) {
    float total = fpsComputeFactor(outputFps);
    for (const auto& ch : channels) {
        total += channelComputeScore(ch);
    }
    return total;
}

inline float totalComputeScoreFromJson(JsonArrayConst outputs, uint8_t outputFps = 40) {
    float total = fpsComputeFactor(outputFps);
    for (JsonObjectConst ch : outputs) {
        OutputChannel tmp;
        tmp.type = ch["type"] | 0;
        tmp.led_count = ch["led_count"] | 0;
        total += channelComputeScore(tmp);
    }
    return total;
}

inline float totalOutputScore(const std::vector<OutputChannel>& channels) {
    float total = 0;
    for (const auto& ch : channels) {
        total += outputChannelScore(ch);
    }
    return total;
}

inline float totalCombinedScore(const std::vector<OutputChannel>& channels, uint8_t outputFps = 40) {
    return totalOutputScore(channels) + totalComputeScore(channels, outputFps);
}

inline float totalOutputScoreFromJson(JsonArrayConst outputs) {
    float total = 0;
    for (JsonObjectConst ch : outputs) {
        OutputChannel tmp;
        tmp.type = ch["type"] | 0;
        tmp.source = ch["source"] | 0;
        tmp.pin2_source = ch["pin2_source"] | 0;
        tmp.pin3_source = ch["pin3_source"] | 0;
        tmp.pin4_source = ch["pin4_source"] | 0;
        tmp.color_order = ch["color_order"] | 0;
        tmp.mc_mode = ch["mc_mode"] | 0;
        tmp.mc_homing_mode = ch["mc_homing_mode"] | 0;
        tmp.pin = ch["pin"] | 255;
        tmp.pin2 = ch["pin2"] | 255;
        tmp.pin4 = ch["pin4"] | 255;
        tmp.led_count = ch["led_count"] | 0;
        total += outputChannelScore(tmp);
    }
    return total;
}

inline float totalCombinedScoreFromJson(JsonArrayConst outputs, uint8_t outputFps = 40) {
    return totalOutputScoreFromJson(outputs) + totalComputeScoreFromJson(outputs, outputFps);
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
