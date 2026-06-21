#ifndef SCORING_H
#define SCORING_H

#include <Arduino.h>
#include "output_control.h"

// ═══════════════════════════════════════
//  HARDWARE RESOURCE COUNTS
// ═══════════════════════════════════════

constexpr uint8_t MAX_LEDC_RESOURCE = 16;
constexpr uint8_t MAX_RMT_RESOURCE = 8;
constexpr uint8_t MAX_UART_RESOURCE = 2;
constexpr uint8_t MAX_DAC_RESOURCE = 2;  // internal DAC (GPIO 25/26; both are Ethernet on WT32-ETH01)

// GPIO is NOT counted — expanders can substitute.
// PCA9685, digital expander channels are NOT counted as hardware — they use I2C
// bus, reflected in CPU weight instead.
struct HardwareResource {
    uint8_t ledc = 0;
    uint8_t rmt = 0;
    uint8_t uart = 0;
    uint8_t dac = 0;

    HardwareResource operator+(const HardwareResource& o) const {
        HardwareResource r;
        r.ledc = ledc + o.ledc;
        r.rmt  = rmt  + o.rmt;
        r.uart = uart + o.uart;
        r.dac  = dac  + o.dac;
        return r;
    }
};

inline bool hardwareWithinLimit(const HardwareResource& h) {
    return h.ledc <= MAX_LEDC_RESOURCE &&
           h.rmt <= MAX_RMT_RESOURCE &&
           h.uart <= MAX_UART_RESOURCE &&
           h.dac <= MAX_DAC_RESOURCE;
}

// ═══════════════════════════════════════
//  PROCESSING: CPU WEIGHT & RAM
// ═══════════════════════════════════════

// CPU weight is per-frame at 40 FPS. Scale = 25.0 at 40 FPS = max budget.
// RAM is static buffer estimate in bytes.
struct CpuBudget {
    float weight = 0;   // total CPU weight of all channels + device-mode overhead

    CpuBudget operator+(const CpuBudget& o) const {
        CpuBudget r;
        r.weight = weight + o.weight;
        return r;
    }

    // Budget scales inversely with FPS (lower FPS = more time per frame)
    static float limit(uint8_t outputFps) {
        if (outputFps < 1) outputFps = 40;
        return 25.0f * (40.0f / outputFps);
    }
};

struct RamBudget {
    uint16_t bytes = 0;  // total estimated RAM

    RamBudget operator+(const RamBudget& o) const {
        RamBudget r;
        r.bytes = bytes + o.bytes;
        return r;
    }

    static constexpr uint16_t limit() { return 65535; }  // 64 KB soft ceiling for output buffers
};

// ═══════════════════════════════════════
//  PER-TYPE ESTIMATES (OutputChannel)
// ═══════════════════════════════════════

inline HardwareResource estimateHardware(const OutputChannel& ch) {
    HardwareResource h;
    switch (ch.type) {
        case 0: break;
        case 1: h.uart = 1; break;
        case 2: break;
        case 3: h.rmt = 1; break;
        case 4:
            if (ch.source == 0) h.ledc = 1;
            break;
        case 5:
            if (ch.source == 0) h.ledc++;
            if (ch.pin2_source == 0) h.ledc++;
            if (ch.pin3_source == 0) h.ledc++;
            if (ch.color_order >= 4 && ch.pin4_source == 0) h.ledc++;
            break;
        case 6:
            if (ch.source == 0) h.ledc++;
            if (ch.pin2_source == 0 && ch.mc_mode == 0) h.ledc++;
            if (ch.mc_mode == 2 && ch.pin3_source == 0) h.ledc++;
            break;
        case 7: h.rmt = 2; break;
        case 8:
            if (ch.source == 0) h.ledc = 1;
            break;
        case 9:
            if (ch.source == 0) h.ledc = 1;
            break;
        case 10: h.uart = 1; break;
        case 11: break;
        case 12:
        case 13: {
            uint8_t segN = (ch.type == 13) ? 8 : 7;
            for (uint8_t i = 0; i < segN; i++) {
                if (ch.seg_sources[i] == 0) h.ledc++;
            }
            break;
        }
        case 14:
            if (ch.source != 5) h.dac = 1;
            break;
        case 15:
            if (ch.source == 0) h.ledc = 1;
            break;
        case 16: h.ledc = 1; break;
        case 17: break;
        case 18: break;
    }
    return h;
}

// True if a source ID uses the I2C bus (adds CPU overhead)
inline bool srcOnI2C(uint8_t src) {
    return src >= 1 && src <= 5;
}

inline bool usesI2C(const OutputChannel& ch) {
    if (srcOnI2C(ch.source)) return true;
    if (srcOnI2C(ch.pin2_source)) return true;
    if (srcOnI2C(ch.pin3_source)) return true;
    if (srcOnI2C(ch.pin4_source)) return true;
    for (auto s : ch.seg_sources) {
        if (srcOnI2C(s)) return true;
    }
    return false;
}

// CPU weight & RAM per channel (independent of FPS)
struct PerChannelCost {
    float cpuWeight = 0;
    uint16_t ramBytes = 0;
};

inline PerChannelCost estimateChannelCost(const OutputChannel& ch) {
    PerChannelCost c;
    switch (ch.type) {
        case 0:  c.cpuWeight = 0.10f; break;
        case 1:  c.cpuWeight = 0.50f; c.ramBytes = 512; break;
        case 2:  c.cpuWeight = 0.05f; break;
        case 3:  c.cpuWeight = ch.led_count * 0.005f; c.ramBytes = ch.led_count * 3; break;
        case 4:  c.cpuWeight = 0.10f; break;
        case 5:  c.cpuWeight = 0.20f; break;
        case 6:  c.cpuWeight = 0.50f; break;
        case 7:  c.cpuWeight = 2.00f; break;
        case 8:  c.cpuWeight = 0.20f; break;
        case 9:  c.cpuWeight = 0.10f; break;
        case 10: c.cpuWeight = 0.50f; c.ramBytes = 100; break;
        case 11: c.cpuWeight = 0.50f; break;
        case 12: c.cpuWeight = 1.00f; break;
        case 13: c.cpuWeight = 1.20f; break;
        case 14: c.cpuWeight = 0.30f; break;
        case 15: c.cpuWeight = 0.10f; break;
        case 16: c.cpuWeight = 2.00f; break;
        case 17: c.cpuWeight = 0.10f; break;
        case 18: c.cpuWeight = 0.30f; break;
    }
    if (usesI2C(ch)) c.cpuWeight += 0.30f;
    return c;
}

// ESP-NOW Master overhead
inline PerChannelCost espnowMasterCost(uint8_t peerCount, uint8_t universeCount) {
    PerChannelCost c;
    c.cpuWeight = 1.0f + peerCount * 0.2f + universeCount * 0.3f;
    c.ramBytes  = 512 + peerCount * 256;
    return c;
}

// ═══════════════════════════════════════
//  AGGREGATE SUMS (vector version)
// ═══════════════════════════════════════

inline HardwareResource totalHardware(const std::vector<OutputChannel>& chs) {
    HardwareResource t;
    for (auto& ch : chs) t = t + estimateHardware(ch);
    return t;
}

inline CpuBudget totalCpu(const std::vector<OutputChannel>& chs) {
    CpuBudget t;
    for (auto& ch : chs) t.weight += estimateChannelCost(ch).cpuWeight;
    return t;
}

inline RamBudget totalRam(const std::vector<OutputChannel>& chs) {
    RamBudget t;
    for (auto& ch : chs) t.bytes += estimateChannelCost(ch).ramBytes;
    return t;
}

// ═══════════════════════════════════════
//  JSON VERSIONS (for Web API validation from raw JSON)
// ═══════════════════════════════════════

inline HardwareResource estimateHardwareFromJson(JsonObjectConst j) {
    HardwareResource h;
    uint8_t t  = j["type"]  | 0;
    uint8_t src = j["source"] | 0;
    switch (t) {
        case 0: break;
        case 1: h.uart = 1; break;
        case 2: break;
        case 3: h.rmt = 1; break;
        case 4: if (src == 0) h.ledc = 1; break;
        case 5:
            if (src == 0) h.ledc++;
            if ((j["pin2_source"] | 0) == 0) h.ledc++;
            if ((j["pin3_source"] | 0) == 0) h.ledc++;
            if ((j["color_order"] | 0) >= 4 && (j["pin4_source"] | 0) == 0) h.ledc++;
            break;
        case 6:
            if (src == 0) h.ledc++;
            if ((j["pin2_source"] | 0) == 0 && (j["mc_mode"] | 0) == 0) h.ledc++;
            if ((j["mc_mode"] | 0) == 2 && (j["pin3_source"] | 0) == 0) h.ledc++;
            break;
        case 7: h.rmt = 2; break;
        case 8:  if (src == 0) h.ledc = 1; break;
        case 9:  if (src == 0) h.ledc = 1; break;
        case 10: h.uart = 1; break;
        case 11: break;
        case 12:
        case 13: {
            uint8_t segN = (t == 13) ? 8 : 7;
            JsonArrayConst sa = j["seg_sources"];
            for (uint8_t i = 0; i < segN; i++) {
                if (!sa.isNull() && i < sa.size()) { if ((sa[i] | 0) == 0) h.ledc++; }
                else { h.ledc++; }  // default seg_sources[i]=0 → GPIO
            }
            break;
        }
        case 14: if (src != 5) h.dac = 1; break;
        case 15: if (src == 0) h.ledc = 1; break;
        case 16: h.ledc = 1; break;
        case 17: break;
        case 18: break;
    }
    return h;
}

inline bool usesI2CFromJson(JsonObjectConst j) {
    if (srcOnI2C(j["source"] | 0)) return true;
    if (srcOnI2C(j["pin2_source"] | 0)) return true;
    if (srcOnI2C(j["pin3_source"] | 0)) return true;
    if (srcOnI2C(j["pin4_source"] | 0)) return true;
    JsonArrayConst sa = j["seg_sources"];
    if (!sa.isNull()) {
        for (auto s : sa) { if (srcOnI2C(s | 0)) return true; }
    }
    return false;
}

inline PerChannelCost estimateChannelCostFromJson(JsonObjectConst j) {
    PerChannelCost c;
    uint8_t t = j["type"] | 0;
    switch (t) {
        case 0:  c.cpuWeight = 0.10f; break;
        case 1:  c.cpuWeight = 0.50f; c.ramBytes = 512; break;
        case 2:  c.cpuWeight = 0.05f; break;
        case 3:  c.cpuWeight = (j["led_count"] | 0) * 0.005f; c.ramBytes = (j["led_count"] | 0) * 3; break;
        case 4:  c.cpuWeight = 0.10f; break;
        case 5:  c.cpuWeight = 0.20f; break;
        case 6:  c.cpuWeight = 0.50f; break;
        case 7:  c.cpuWeight = 2.00f; break;
        case 8:  c.cpuWeight = 0.20f; break;
        case 9:  c.cpuWeight = 0.10f; break;
        case 10: c.cpuWeight = 0.50f; c.ramBytes = 100; break;
        case 11: c.cpuWeight = 0.50f; break;
        case 12: c.cpuWeight = 1.00f; break;
        case 13: c.cpuWeight = 1.20f; break;
        case 14: c.cpuWeight = 0.30f; break;
        case 15: c.cpuWeight = 0.10f; break;
        case 16: c.cpuWeight = 2.00f; break;
        case 17: c.cpuWeight = 0.10f; break;
        case 18: c.cpuWeight = 0.30f; break;
    }
    if (usesI2CFromJson(j)) c.cpuWeight += 0.30f;
    return c;
}

inline HardwareResource totalHardwareFromJson(JsonArrayConst arr) {
    HardwareResource t;
    for (JsonObjectConst j : arr) t = t + estimateHardwareFromJson(j);
    return t;
}

inline CpuBudget totalCpuFromJson(JsonArrayConst arr, uint8_t fps = 40) {
    CpuBudget t;
    for (JsonObjectConst j : arr) t.weight += estimateChannelCostFromJson(j).cpuWeight;
    return t;
}

inline RamBudget totalRamFromJson(JsonArrayConst arr) {
    RamBudget t;
    for (JsonObjectConst j : arr) t.bytes += estimateChannelCostFromJson(j).ramBytes;
    return t;
}

// ═══════════════════════════════════════
//  COMBINED VALIDATION
// ═══════════════════════════════════════

enum class ScoreBlocker : uint8_t {
    None,
    HardwareLimit,  // LEDC/RMT/UART/DAC exceeds max (source-based, expander-aware)
    CpuLimit,       // CPU weight exceeds budget at current FPS
    RamLimit        // RAM bytes exceeds 64KB
};

// Hardware, CPU, and RAM are independent checks.
// Hardware counts are source-aware: PCA/EXP channels don't consume ESP32 hardware.
inline ScoreBlocker checkScores(const HardwareResource& hw, const CpuBudget& cpu, const RamBudget& ram, uint8_t fps) {
    if (!hardwareWithinLimit(hw)) return ScoreBlocker::HardwareLimit;
    if (cpu.weight > CpuBudget::limit(fps)) return ScoreBlocker::CpuLimit;
    if (ram.bytes > RamBudget::limit()) return ScoreBlocker::RamLimit;
    return ScoreBlocker::None;
}

inline const char* scoreBlockerName(ScoreBlocker b) {
    switch (b) {
        case ScoreBlocker::HardwareLimit: return "Hardware limit exceeded";
        case ScoreBlocker::CpuLimit:      return "CPU budget exceeded";
        case ScoreBlocker::RamLimit:      return "RAM budget exceeded";
        default:                          return "";
    }
}

// Convenience: check from JSON (as done in POST /api/outputs)
inline ScoreBlocker checkScoresFromJson(JsonArrayConst arr, uint8_t fps) {
    HardwareResource hw = totalHardwareFromJson(arr);
    CpuBudget cpu = totalCpuFromJson(arr, fps);
    RamBudget ram = totalRamFromJson(arr);
    return checkScores(hw, cpu, ram, fps);
}

// ═══════════════════════════════════════
//  NAME HELPERS
// ═══════════════════════════════════════

inline const char* outputTypeName(uint8_t type) {
    switch (type) {
        case 0:  return "AC Dimmer";
        case 1:  return "DMX Output";
        case 2:  return "Relay";
        case 3:  return "RGB LED";
        case 4:  return "Single Color LED";
        case 5:  return "Analog RGB";
        case 6:  return "Motor";
        case 7:  return "Stepper";
        case 8:  return "Servo";
        case 9:  return "Buzzer";
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
