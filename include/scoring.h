#ifndef SCORING_H
#define SCORING_H

#include <Arduino.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include "output_control.h"
#include "scoring_defs.h"

// ═══════════════════════════════════════
//  HARDWARE RESOURCE COUNTS
// ═══════════════════════════════════════

constexpr uint8_t MAX_LEDC_RESOURCE = ScoringDefs::MAX_LEDC_RESOURCE;
constexpr uint8_t MAX_RMT_RESOURCE = ScoringDefs::MAX_RMT_RESOURCE;
constexpr uint8_t MAX_UART_RESOURCE = ScoringDefs::MAX_UART_RESOURCE;
constexpr uint8_t MAX_DAC_RESOURCE = ScoringDefs::MAX_DAC_RESOURCE;
constexpr uint8_t MAX_TIMER_RESOURCE = ScoringDefs::MAX_TIMER_RESOURCE;

// GPIO is NOT counted — expanders can substitute.
// PCA9685, digital expander channels are NOT counted as hardware — they use I2C
// bus, reflected in service-time budget instead.
struct HardwareResource {
    uint8_t ledc = 0;
    uint8_t rmt = 0;
    uint8_t uart = 0;
    uint8_t dac = 0;
    uint8_t timer = 0;

    HardwareResource operator+(const HardwareResource& o) const {
        HardwareResource r;
        r.ledc = ledc + o.ledc;
        r.rmt  = rmt  + o.rmt;
        r.uart = uart + o.uart;
        r.dac  = dac  + o.dac;
        r.timer = timer + o.timer;
        return r;
    }
};

inline bool hardwareWithinLimit(const HardwareResource& h) {
    return h.ledc <= MAX_LEDC_RESOURCE &&
           h.rmt <= MAX_RMT_RESOURCE &&
           h.uart <= MAX_UART_RESOURCE &&
           h.dac <= MAX_DAC_RESOURCE &&
           h.timer <= MAX_TIMER_RESOURCE;
}

// ═══════════════════════════════════════
//  PROCESSING: CPU TIME (µs per frame) & RAM
// ═══════════════════════════════════════

// Output service time is estimated in microseconds per frame.
// It includes blocking/serialized output work, not only pure CPU cycles.
// Budget = frame_time_us - safety reserve; total starts with base loop overhead.
// RAM is static buffer estimate in bytes.
struct CpuBudget {
    static constexpr uint32_t BASE_OVERHEAD_US = ScoringDefs::CPU_BASE_OVERHEAD_US;
    static constexpr uint32_t SAFETY_RESERVE_US = ScoringDefs::CPU_SAFETY_RESERVE_US;
    uint32_t usPerFrame = BASE_OVERHEAD_US;  // starts with base overhead

    CpuBudget operator+(const CpuBudget& o) const {
        CpuBudget r;
        r.usPerFrame = usPerFrame + o.usPerFrame;
        return r;
    }

    static uint32_t limit(uint8_t outputFps) {
        if (outputFps < 1) outputFps = 40;
        uint32_t frameUs = 1000000U / (uint32_t)outputFps;
        return frameUs > SAFETY_RESERVE_US ? frameUs - SAFETY_RESERVE_US : frameUs / 2;
    }
};

struct RamBudget {
    uint32_t bytes = 0;  // total estimated RAM

    RamBudget operator+(const RamBudget& o) const {
        RamBudget r;
        r.bytes = bytes + o.bytes;
        return r;
    }

    static uint32_t limit() {
        // Keep at least 150 KB free for system/network/runtime; use the rest for output buffers, capped at 64 KB.
        int32_t freeHeap = ESP.getFreeHeap();
        int32_t available = freeHeap - ScoringDefs::RAM_KEEP_FREE;
        if (available < 0) available = 0;
        if (available > ScoringDefs::RAM_LIMIT_CAP) available = ScoringDefs::RAM_LIMIT_CAP;
        return (uint32_t)available;
    }
};

// ═══════════════════════════════════════
//  PER-TYPE ESTIMATES (OutputChannel)
// ═══════════════════════════════════════

// Pin index → source field helper: index 0→ch.source, 1→pin2_source, etc.
inline uint8_t getPinSource(const OutputChannel& ch, uint8_t pinIndex) {
    switch (pinIndex) {
        case 0: return ch.source;
        case 1: return ch.pin2_source;
        case 2: return ch.pin3_source;
        case 3: return ch.pin4_source;
        default: return (pinIndex < 12) ? ch.seg_sources[pinIndex - 4] : 0;
    }
}

inline HardwareResource estimateHardware(const OutputChannel& ch) {
    const auto* def = OutputDefs::modeDef(ch.type, ch.mc_mode);
    if (!def) return {};
    HardwareResource h;
    h.ledc = 0; // recompute per-pin below
    h.rmt = def->cost.hardware.rmt;
    h.uart = def->cost.hardware.uart;
    h.dac = def->cost.hardware.dac;
    h.timer = def->cost.hardware.timer;

    // Per-pin GPIO-driven hardware
    for (uint8_t i = 0; i < def->pinCount; i++) {
        uint8_t src = getPinSource(ch, i);
        if (src == 0) {
            if (def->pins[i].hwIfGpio == 1) h.ledc++;
            if (def->pins[i].hwIfGpio == 2 && h.rmt < def->cost.hardware.rmt) h.rmt++;
        }
    }

    // Analog RGB pin4 only counts in RGBW mode (color_order >= 4)
    if (ch.type == OutputDefs::TYPE_ANALOG_RGB && ch.color_order < 4 && ch.pin4_source == 0 && h.ledc > 0) {
        h.ledc--;
    }

    // DAC type 14: source 5-7 is I2C DAC (no internal DAC). Internal DAC=source 0.
    if (ch.type == OutputDefs::TYPE_DAC && ch.source == 0) {
        h.dac = 1;
    }

    return h;
}

// True if a source ID uses the I2C bus (adds CPU overhead)
inline bool srcOnI2C(uint8_t src) {
    return src >= 1 && src <= 7;
}

inline bool usesI2C(const OutputChannel& ch) {
    const auto* def = OutputDefs::modeDef(ch.type, ch.mc_mode);
    if (!def) return false;
    for (uint8_t i = 0; i < def->pinCount; i++) {
        if (srcOnI2C(getPinSource(ch, i))) return true;
    }
    return false;
}

inline uint32_t ledStripServiceUs(uint16_t ledCount, uint8_t colorOrder) {
    uint8_t bytesPerPixel = colorOrder >= 4 ? 4 : 3;
    return 80 + ledCount * (bytesPerPixel == 4 ? 4 : 3);  // CPU prep + Show enqueue; RMT wire time can be skipped
}

inline uint8_t sevenSegCount(uint8_t type, uint8_t mode) {
    if (type == OutputDefs::TYPE_7SEG_8PIN) return 8;
    if (type == OutputDefs::TYPE_7SEG_7PIN) return 7;
    return 0;
}

constexpr uint16_t I2C_WRITE_US = ScoringDefs::I2C_WRITE_US;

inline uint8_t i2cWritesForChannel(const OutputChannel& ch) {
    const auto* def = OutputDefs::modeDef(ch.type, ch.mc_mode);
    if (!def) return 0;
    uint8_t writes = 0;
    for (uint8_t i = 0; i < def->pinCount; i++) {
        if (srcOnI2C(getPinSource(ch, i))) writes++;
    }
    return writes;
}

// Service time & RAM per channel (independent of FPS)
constexpr uint32_t BASE_CHANNEL_RAM = ScoringDefs::BASE_CHANNEL_RAM;
constexpr uint32_t RMT_DMX_DRIVER_RAM = 5150UL * sizeof(rmt_item32_t) + 32;
constexpr uint32_t I2C_ROUTE_RAM = ScoringDefs::I2C_ROUTE_RAM;
constexpr uint32_t DMX_OUTPUT_SERVICE_US = ScoringDefs::DMX_OUTPUT_SERVICE_US;

inline uint32_t frameTimeUs(uint8_t outputFps) {
    if (outputFps < 1) outputFps = 40;
    return 1000000U / (uint32_t)outputFps;
}

inline uint32_t acDimmerBackgroundUs(uint8_t dimmerCount, uint8_t outputFps) {
    if (dimmerCount == 0) return 0;
    uint32_t ticksPerFrame = frameTimeUs(outputFps) / 39; // DIMMER_TICK_US
    return ticksPerFrame * (1 + dimmerCount);            // timer ISR base + per-channel compare
}

inline uint32_t funcGenBackgroundUs(uint8_t funcGenCount, uint8_t outputFps) {
    if (funcGenCount == 0) return 0;
    uint32_t samplesPerFrame = frameTimeUs(outputFps) / 50; // esp_timer minimum period in FuncGenController
    return samplesPerFrame * 4U * funcGenCount;            // table lookup + LEDC write ISR estimate
}

inline uint32_t dmxBufferRamForChannel(uint8_t type, uint16_t ledCount, uint8_t colorOrder, uint8_t resolution, uint8_t mode) {
    // DMX: full universe buffer
    if (type == OutputDefs::TYPE_DMX) return 512;
    // LED strip: universe-rounded (led_count may span multiple universes)
    if (type == OutputDefs::TYPE_LED_STRIP) {
        uint8_t bpp = colorOrder >= 4 ? 4 : 3;
        uint16_t pixelsPerUni = 512 / bpp;
        uint16_t universes = (ledCount + pixelsPerUni - 1) / pixelsPerUni;
        if (universes < 1) universes = 1;
        return (uint32_t)universes * 512;
    }
    // Types where buffer depends on mode/colorOrder (can't be static)
    if (type == OutputDefs::TYPE_ANALOG_RGB) return colorOrder >= 4 ? 4 : 3;
    if (type == OutputDefs::TYPE_STEPPER) return dmxValueByteCount(resolution) + 2;
    if (type == OutputDefs::TYPE_BUZZER || type == OutputDefs::TYPE_TM1637) return mode == 1 ? 4 : 2;
    if (type == OutputDefs::TYPE_7SEG_7PIN || type == OutputDefs::TYPE_7SEG_8PIN) return (mode == 4 || mode == 5 || mode >= 6) ? 2 : 1;
    // Fallback: use dmxSlots from definition, or dmxValueByteCount
    const auto* def = OutputDefs::modeDef(type, mode);
    if (!def) return 1;
    if (def->cost.dmxSlots > 0) return def->cost.dmxSlots;
    return dmxValueByteCount(resolution);
}

inline uint32_t pixelBufferRam(uint16_t ledCount, uint8_t colorOrder) {
    return (uint32_t)ledCount * (colorOrder >= 4 ? 4 : 3);
}

struct PerChannelCost {
    uint32_t cpuUs = 0;       // estimated µs per frame
    uint32_t ramBytes = 0;
};

// Per-type active-frame service-time estimates (µs/frame) for ESP32 at 240 MHz.
// Includes serialized DMX/TM1637 work, LED strip mapping/enqueue, and active I2C transactions.
inline PerChannelCost estimateChannelCost(const OutputChannel& ch) {
    PerChannelCost c;
    c.cpuUs = OutputDefs::baseCpuUs(ch.type, ch.mc_mode);
    c.ramBytes = BASE_CHANNEL_RAM + dmxBufferRamForChannel(ch.type, ch.led_count, ch.color_order, ch.mc_resolution, ch.mc_mode);
    auto* def = OutputDefs::modeDef(ch.type, ch.mc_mode);
    if (def) c.ramBytes += def->cost.extraRamBytes;
    if (ch.type == OutputDefs::TYPE_LED_STRIP) c.cpuUs = ledStripServiceUs(ch.led_count, ch.color_order);
    if (ch.type == OutputDefs::TYPE_LED_STRIP) c.ramBytes += pixelBufferRam(ch.led_count, ch.color_order);
    c.cpuUs += (uint16_t)i2cWritesForChannel(ch) * I2C_WRITE_US;
    c.ramBytes += (uint32_t)i2cWritesForChannel(ch) * I2C_ROUTE_RAM;
    return c;
}

// ESP-NOW Master overhead. chunkSize = DMX data bytes per packet (not including 12-byte header).
inline PerChannelCost espnowMasterCost(uint8_t peerCount, uint8_t universeCount, uint16_t chunkSize = 200) {
    PerChannelCost c;
    uint8_t chunksPerUni = (511 + chunkSize) / chunkSize;  // ceil(512 / chunkSize)
    // Each chunk: build packet (20 µs) + esp_now_send (~150 µs blocking)
    c.cpuUs = 500 + peerCount * chunksPerUni * 170 + universeCount * 100;
    c.ramBytes  = 512 + peerCount * (chunkSize + 44);
    return c;
}

inline uint8_t getEspNowPeerCount() {
    uint8_t count = 0;
    if (LittleFS.exists("/espnow_peers.json")) {
        File file = LittleFS.open("/espnow_peers.json", "r");
        if (file) {
            JsonDocument doc;
            DeserializationError error = deserializeJson(doc, file);
            file.close();
            if (!error && doc["peers"].is<JsonArray>()) {
                count = doc["peers"].as<JsonArray>().size();
            }
        }
    }
    return count == 0 ? 1 : count; // Default to 1 (broadcast) if 0
}

inline uint8_t countUniqueUniverses(const std::vector<OutputChannel>& chs) {
    std::vector<uint16_t> unis;
    for (const auto& ch : chs) {
        bool found = false;
        for (uint16_t u : unis) {
            if (u == ch.start_universe) {
                found = true;
                break;
            }
        }
        if (!found) {
            unis.push_back(ch.start_universe);
        }
    }
    return unis.size();
}

inline uint8_t countUniqueUniversesFromJson(JsonArrayConst arr) {
    std::vector<uint16_t> unis;
    for (JsonObjectConst j : arr) {
        uint16_t u = j["start_universe"] | 0;
        bool found = false;
        for (uint16_t uni : unis) {
            if (uni == u) {
                found = true;
                break;
            }
        }
        if (!found) {
            unis.push_back(u);
        }
    }
    return unis.size();
}

// ═══════════════════════════════════════
//  AGGREGATE SUMS (vector version)
// ═══════════════════════════════════════

inline HardwareResource totalHardware(const std::vector<OutputChannel>& chs) {
    HardwareResource t;
    bool hasDimmer = false;
    uint8_t dmxCount = 0;
    uint8_t dfPlayerCount = 0;
    for (auto& ch : chs) {
        t = t + estimateHardware(ch);
        if (ch.type == OutputDefs::TYPE_DIMMER) hasDimmer = true;
        if (ch.type == OutputDefs::TYPE_DMX && ch.source == 0) dmxCount++;
        else if (ch.type == OutputDefs::TYPE_DFPLAYER) dfPlayerCount++;
    }
    // DFPlayer has UART priority; DMX falls back to RMT when UARTs exhausted
    uint8_t freeUarts = dfPlayerCount >= 2 ? 0 : (uint8_t)(2 - dfPlayerCount);
    uint8_t dmxUartUse = dmxCount < freeUarts ? dmxCount : freeUarts;
    uint8_t dmxRmtUse = dmxCount > freeUarts ? (uint8_t)(dmxCount - freeUarts) : 0;
    t.uart = dfPlayerCount + dmxUartUse;
    t.rmt += dmxRmtUse;
    if (hasDimmer) t.timer += 1;
    return t;
}

inline CpuBudget totalCpu(const std::vector<OutputChannel>& chs, uint8_t fps = 40) {
    CpuBudget t;
    uint8_t dimmerCount = 0;
    uint8_t funcGenCount = 0;
    for (auto& ch : chs) {
        t.usPerFrame += estimateChannelCost(ch).cpuUs;
        if (ch.type == OutputDefs::TYPE_DIMMER) dimmerCount++;
        else if (ch.type == OutputDefs::TYPE_FUNC_GEN) funcGenCount++;
    }
    t.usPerFrame += acDimmerBackgroundUs(dimmerCount, fps);
    t.usPerFrame += funcGenBackgroundUs(funcGenCount, fps);
    
    if (sysCfg.device_mode == MODE_ESPNOW_MASTER) {
        uint8_t peerCount = getEspNowPeerCount();
        uint8_t universeCount = countUniqueUniverses(chs);
        t.usPerFrame += espnowMasterCost(peerCount, universeCount, sysCfg.espnow_chunk_size).cpuUs;
    }
    return t;
}

inline RamBudget totalRam(const std::vector<OutputChannel>& chs) {
    RamBudget t;
    uint8_t dmxCount = 0;
    uint8_t dfPlayerCount = 0;
    for (auto& ch : chs) {
        t.bytes += estimateChannelCost(ch).ramBytes;
        if (ch.type == OutputDefs::TYPE_DMX && ch.source == 0) dmxCount++;
        else if (ch.type == OutputDefs::TYPE_DFPLAYER) dfPlayerCount++;
    }
    uint8_t freeUarts = dfPlayerCount >= 2 ? 0 : (uint8_t)(2 - dfPlayerCount);
    uint8_t dmxRmtUse = dmxCount > freeUarts ? (uint8_t)(dmxCount - freeUarts) : 0;
    t.bytes += (uint32_t)dmxRmtUse * RMT_DMX_DRIVER_RAM;

    if (sysCfg.device_mode == MODE_ESPNOW_MASTER) {
        uint8_t peerCount = getEspNowPeerCount();
        t.bytes += espnowMasterCost(peerCount, 0, sysCfg.espnow_chunk_size).ramBytes;
    }
    return t;
}

// ═══════════════════════════════════════
//  JSON VERSIONS (for Web API validation from raw JSON)
// ═══════════════════════════════════════

inline uint8_t getPinSourceFromJson(JsonObjectConst j, uint8_t pinIndex) {
    switch (pinIndex) {
        case 0: return j["source"] | 0;
        case 1: return j["pin2_source"] | 0;
        case 2: return j["pin3_source"] | 0;
        case 3: return j["pin4_source"] | 0;
        default: {
            JsonArrayConst sa = j["seg_sources"];
            uint8_t i = pinIndex - 4;
            return (!sa.isNull() && i < sa.size()) ? (uint8_t)(sa[i] | 0) : 0;
        }
    }
}

inline HardwareResource estimateHardwareFromJson(JsonObjectConst j) {
    uint8_t t = j["type"] | 0;
    uint8_t mode = j["mc_mode"] | 0;
    const auto* def = OutputDefs::modeDef(t, mode);
    if (!def) return {};
    HardwareResource h;
    h.ledc = 0;
    h.rmt = def->cost.hardware.rmt;
    h.uart = def->cost.hardware.uart;
    h.dac = def->cost.hardware.dac;
    h.timer = def->cost.hardware.timer;

    for (uint8_t i = 0; i < def->pinCount; i++) {
        uint8_t src = getPinSourceFromJson(j, i);
        if (src == 0) {
            if (def->pins[i].hwIfGpio == 1) h.ledc++;
            if (def->pins[i].hwIfGpio == 2 && h.rmt < def->cost.hardware.rmt) h.rmt++;
        }
    }

    uint8_t colorOrder = j["color_order"] | 0;
    if (t == OutputDefs::TYPE_ANALOG_RGB && colorOrder < 4 && getPinSourceFromJson(j, 3) == 0 && h.ledc > 0) {
        h.ledc--;
    }

    if (t == OutputDefs::TYPE_DAC && (j["source"] | 0) == 0) {
        h.dac = 1;
    }

    return h;
}

inline bool usesI2CFromJson(JsonObjectConst j) {
    uint8_t t = j["type"] | 0;
    uint8_t mode = j["mc_mode"] | 0;
    const auto* def = OutputDefs::modeDef(t, mode);
    if (!def) return false;
    for (uint8_t i = 0; i < def->pinCount; i++) {
        if (srcOnI2C(getPinSourceFromJson(j, i))) return true;
    }
    return false;
}

inline uint8_t i2cWritesFromJson(JsonObjectConst j) {
    uint8_t t = j["type"] | 0;
    uint8_t mode = j["mc_mode"] | 0;
    const auto* def = OutputDefs::modeDef(t, mode);
    if (!def) return 0;
    uint8_t writes = 0;
    for (uint8_t i = 0; i < def->pinCount; i++) {
        if (srcOnI2C(getPinSourceFromJson(j, i))) writes++;
    }
    return writes;
}

inline PerChannelCost estimateChannelCostFromJson(JsonObjectConst j) {
    PerChannelCost c;
    uint8_t t = j["type"] | 0;
    uint8_t mode = j["mc_mode"] | 0;
    uint16_t ledCount = j["led_count"] | 0;
    uint8_t colorOrder = j["color_order"] | 0;
    c.cpuUs = OutputDefs::baseCpuUs(t, mode);
    c.ramBytes = BASE_CHANNEL_RAM + dmxBufferRamForChannel(t, ledCount, colorOrder, j["mc_resolution"] | 8, mode);
    auto* def = OutputDefs::modeDef(t, mode);
    if (def) c.ramBytes += def->cost.extraRamBytes;
    if (t == OutputDefs::TYPE_LED_STRIP) c.cpuUs = ledStripServiceUs(ledCount, colorOrder);
    if (t == OutputDefs::TYPE_LED_STRIP) c.ramBytes += pixelBufferRam(ledCount, colorOrder);
    c.cpuUs += (uint16_t)i2cWritesFromJson(j) * I2C_WRITE_US;
    c.ramBytes += (uint32_t)i2cWritesFromJson(j) * I2C_ROUTE_RAM;
    return c;
}

inline HardwareResource totalHardwareFromJson(JsonArrayConst arr) {
    HardwareResource t;
    bool hasDimmer = false;
    uint8_t dmxCount = 0;
    uint8_t dfPlayerCount = 0;
    for (JsonObjectConst j : arr) {
        t = t + estimateHardwareFromJson(j);
        uint8_t type = j["type"] | 0;
        uint8_t source = j["source"] | 0;
        if (type == OutputDefs::TYPE_DMX && source == 0) dmxCount++;
        else if (type == OutputDefs::TYPE_DFPLAYER) dfPlayerCount++;
        if (type == OutputDefs::TYPE_DIMMER) hasDimmer = true;
    }
    // DFPlayer has UART priority; DMX falls back to RMT when UARTs exhausted
    uint8_t freeUarts = dfPlayerCount >= 2 ? 0 : (uint8_t)(2 - dfPlayerCount);
    uint8_t dmxUartUse = dmxCount < freeUarts ? dmxCount : freeUarts;
    uint8_t dmxRmtUse = dmxCount > freeUarts ? (uint8_t)(dmxCount - freeUarts) : 0;
    t.uart = dfPlayerCount + dmxUartUse;
    t.rmt += dmxRmtUse;
    if (hasDimmer) t.timer += 1;
    return t;
}

inline CpuBudget totalCpuFromJson(JsonArrayConst arr, uint8_t fps = 40) {
    CpuBudget t;
    uint8_t dimmerCount = 0;
    uint8_t funcGenCount = 0;
    for (JsonObjectConst j : arr) {
        uint8_t type = j["type"] | 0;
        t.usPerFrame += estimateChannelCostFromJson(j).cpuUs;
        if (type == OutputDefs::TYPE_DIMMER) dimmerCount++;
        else if (type == OutputDefs::TYPE_FUNC_GEN) funcGenCount++;
    }
    t.usPerFrame += acDimmerBackgroundUs(dimmerCount, fps);
    t.usPerFrame += funcGenBackgroundUs(funcGenCount, fps);

    if (sysCfg.device_mode == MODE_ESPNOW_MASTER) {
        uint8_t peerCount = getEspNowPeerCount();
        uint8_t universeCount = countUniqueUniversesFromJson(arr);
        t.usPerFrame += espnowMasterCost(peerCount, universeCount, sysCfg.espnow_chunk_size).cpuUs;
    }
    return t;
}

inline RamBudget totalRamFromJson(JsonArrayConst arr) {
    RamBudget t;
    uint8_t dmxCount = 0;
    uint8_t dfPlayerCount = 0;
    for (JsonObjectConst j : arr) {
        t.bytes += estimateChannelCostFromJson(j).ramBytes;
        uint8_t type = j["type"] | 0;
        uint8_t source = j["source"] | 0;
        if (type == OutputDefs::TYPE_DMX && source == 0) dmxCount++;
        else if (type == OutputDefs::TYPE_DFPLAYER) dfPlayerCount++;
    }
    uint8_t freeUarts = dfPlayerCount >= 2 ? 0 : (uint8_t)(2 - dfPlayerCount);
    uint8_t dmxRmtUse = dmxCount > freeUarts ? (uint8_t)(dmxCount - freeUarts) : 0;
    t.bytes += (uint32_t)dmxRmtUse * RMT_DMX_DRIVER_RAM;

    if (sysCfg.device_mode == MODE_ESPNOW_MASTER) {
        uint8_t peerCount = getEspNowPeerCount();
        t.bytes += espnowMasterCost(peerCount, 0, sysCfg.espnow_chunk_size).ramBytes;
    }
    return t;
}

// ═══════════════════════════════════════
//  COMBINED VALIDATION
// ═══════════════════════════════════════

enum class ScoreBlocker : uint8_t {
    None,
    HardwareLimit,  // LEDC/RMT/UART/DAC exceeds max (source-based, expander-aware)
    CpuLimit,       // CPU µs exceeds budget at current FPS
    RamLimit        // RAM bytes exceeds 64KB
};

// Hardware, CPU, and RAM are independent checks.
// Hardware counts are source-aware: PCA/EXP channels don't consume ESP32 hardware.
inline ScoreBlocker checkScores(const HardwareResource& hw, const CpuBudget& cpu, const RamBudget& ram, uint8_t fps) {
    if (!hardwareWithinLimit(hw)) return ScoreBlocker::HardwareLimit;
    if (cpu.usPerFrame > CpuBudget::limit(fps)) return ScoreBlocker::CpuLimit;
    if (ram.bytes > RamBudget::limit()) return ScoreBlocker::RamLimit;
    return ScoreBlocker::None;
}

inline const char* scoreBlockerName(ScoreBlocker b) {
    switch (b) {
        case ScoreBlocker::HardwareLimit: return "Hardware limit exceeded";
        case ScoreBlocker::CpuLimit:      return "CPU time exceeded — reduce channels or lower FPS";
        case ScoreBlocker::RamLimit:      return "RAM budget exceeded — reduce channels";
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
        case OutputDefs::TYPE_DIMMER:  return "AC Dimmer";
        case OutputDefs::TYPE_DMX:     return "DMX Output";
        case OutputDefs::TYPE_RELAY:   return "Relay";
        case OutputDefs::TYPE_LED_STRIP: return "RGB LED";
        case OutputDefs::TYPE_SINGLE_LED: return "Single Color LED";
        case OutputDefs::TYPE_ANALOG_RGB: return "Analog RGB";
        case OutputDefs::TYPE_MOTOR:   return "Motor";
        case OutputDefs::TYPE_STEPPER: return "Stepper";
        case OutputDefs::TYPE_SERVO:   return "Servo";
        case OutputDefs::TYPE_BUZZER:  return "Buzzer";
        case OutputDefs::TYPE_DFPLAYER: return "DFPlayer MP3";
        case OutputDefs::TYPE_TM1637:  return "7-Segment 2-Pin";
        case OutputDefs::TYPE_7SEG_7PIN: return "7-Segment DD 7-Pin PWM";
        case OutputDefs::TYPE_7SEG_8PIN: return "7-Segment DD 8-Pin PWM";
        case OutputDefs::TYPE_DAC:     return "DAC";
        case OutputDefs::TYPE_PWM_DAC: return "PWM DAC";
        case OutputDefs::TYPE_FUNC_GEN: return "Function Gen";
        case OutputDefs::TYPE_SOLENOID: return "Solenoid";
        case OutputDefs::TYPE_SMOKE:   return "Smoke Shooter";
        default: return "Unknown";
    }
}

#endif
