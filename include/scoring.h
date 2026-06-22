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
            if (ch.mc_mode >= 6 && ch.mc_mode <= 9) {
                if (ch.source == 0) h.ledc = 1;
                break;
            }
            uint8_t segN = (ch.type == 13) ? 8 : 7;
            for (uint8_t i = 0; i < segN; i++) {
                if (ch.seg_sources[i] == 0) h.ledc++;
            }
            break;
        }
        case 14:
            if (!(ch.source >= 5 && ch.source <= 7)) h.dac = 1;
            break;
        case 15:
            if (ch.source == 0) h.ledc = 1;
            break;
        case 16: h.ledc = 1; h.timer = 1; break;
        case 17: break;
        case 18: break;
    }
    return h;
}

// True if a source ID uses the I2C bus (adds CPU overhead)
inline bool srcOnI2C(uint8_t src) {
    return src >= 1 && src <= 7;
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

inline uint32_t ledStripServiceUs(uint16_t ledCount, uint8_t colorOrder) {
    uint8_t bytesPerPixel = colorOrder >= 4 ? 4 : 3;
    return 80 + ledCount * (bytesPerPixel == 4 ? 4 : 3);  // CPU prep + Show enqueue; RMT wire time can be skipped
}

inline uint8_t sevenSegCount(uint8_t type, uint8_t mode) {
    if (type == 13) return 8;
    if (type == 12) return 7;
    return 0;
}

constexpr uint16_t I2C_WRITE_US = ScoringDefs::I2C_WRITE_US;

inline uint8_t i2cWritesForChannel(const OutputChannel& ch) {
    uint8_t writes = 0;
    switch (ch.type) {
        case 2:
        case 17:
            if (srcOnI2C(ch.source)) writes += 1;
            break;
        case 4:
        case 8:
        case 15:
            if (srcOnI2C(ch.source)) writes += 1;
            break;
        case 5:
            if (srcOnI2C(ch.source)) writes += 1;
            if (srcOnI2C(ch.pin2_source)) writes += 1;
            if (srcOnI2C(ch.pin3_source)) writes += 1;
            if (ch.color_order >= 4 && srcOnI2C(ch.pin4_source)) writes += 1;
            break;
        case 6:
            if (srcOnI2C(ch.source)) {
                writes += (ch.mc_mode == 2) ? 3 : 2;
            } else {
                if (srcOnI2C(ch.pin2_source)) writes += 1;
                if (ch.mc_mode == 2 && srcOnI2C(ch.pin3_source)) writes += 1;
            }
            break;
        case 7:
            if (srcOnI2C(ch.pin2_source)) writes += 1;
            if (srcOnI2C(ch.pin3_source)) writes += 1;
            break;
        case 12:
        case 13: {
            uint8_t n = sevenSegCount(ch.type, ch.mc_mode);
            if (ch.mc_mode >= 2 && srcOnI2C(ch.pin2_source)) {
                writes += n;
            } else {
                for (uint8_t i = 0; i < n; i++) {
                    if (srcOnI2C(ch.seg_sources[i])) writes += 1;
                }
            }
            if (ch.mc_mode >= 6 && ch.mc_mode <= 9 && srcOnI2C(ch.source)) writes += 1;
            break;
        }
        case 14:
            if (srcOnI2C(ch.source)) writes += 1;
            break;
        case 18:
            if (srcOnI2C(ch.source)) writes += 1;
            if (srcOnI2C(ch.pin2_source)) writes += 1;
            break;
    }
    return writes;
}

// Service time & RAM per channel (independent of FPS)
constexpr uint32_t BASE_CHANNEL_RAM = ScoringDefs::BASE_CHANNEL_RAM;
constexpr uint32_t DMX_BUFFER_RAM = ScoringDefs::DMX_BUFFER_RAM;
constexpr uint32_t PIXEL_STRIP_OBJECT_RAM = ScoringDefs::PIXEL_STRIP_OBJECT_RAM;
constexpr uint32_t DFPLAYER_OBJECT_RAM = ScoringDefs::DFPLAYER_OBJECT_RAM;
constexpr uint32_t STEPPER_RUNTIME_RAM = ScoringDefs::STEPPER_RUNTIME_RAM;
constexpr uint32_t FUNCGEN_OBJECT_RAM = ScoringDefs::FUNCGEN_OBJECT_RAM;
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
    switch (type) {
        case 1:
            return DMX_BUFFER_RAM;
        case 3: {
        uint8_t bytesPerPixel = colorOrder >= 4 ? 4 : 3;
        uint16_t pixelsPerUniverse = 512 / bytesPerPixel;
        uint16_t universes = (ledCount + pixelsPerUniverse - 1) / pixelsPerUniverse;
        if (universes < 1) universes = 1;
        return (uint32_t)universes * 512;
        }
        case 5:
            return colorOrder >= 4 ? 4 : 3;
        case 7:
            return dmxValueByteCount(resolution) + 2;
        case 9:
        case 11:
            return mode == 1 ? 4 : 2;
        case 12:
        case 13:
            return (mode == 4 || mode == 5 || mode >= 6) ? 2 : 1;
        case 10:
            return 3;
        case 16:
            return 5;
        case 4:
        case 6:
        case 8:
        case 15:
            return dmxValueByteCount(resolution);
        default:
            return 1;
    }
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
    c.cpuUs = ScoringDefs::channelCpuUs(ch.type, ch.mc_mode);
    c.ramBytes = BASE_CHANNEL_RAM + dmxBufferRamForChannel(ch.type, ch.led_count, ch.color_order, ch.mc_resolution, ch.mc_mode);
    if (ch.type == 3) c.cpuUs = ledStripServiceUs(ch.led_count, ch.color_order);
    if (ch.type == 3) c.ramBytes += pixelBufferRam(ch.led_count, ch.color_order) + PIXEL_STRIP_OBJECT_RAM;
    if (ch.type == 7) c.ramBytes += STEPPER_RUNTIME_RAM;
    if (ch.type == 10) c.ramBytes += DFPLAYER_OBJECT_RAM + 100;
    if (ch.type == 16) c.ramBytes += FUNCGEN_OBJECT_RAM;
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
        if (ch.type == 0) hasDimmer = true;
        if (ch.type == 1 && ch.source == 0) dmxCount++;
        else if (ch.type == 10) dfPlayerCount++;
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
        if (ch.type == 0) dimmerCount++;
        else if (ch.type == 16) funcGenCount++;
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
        if (ch.type == 1 && ch.source == 0) dmxCount++;
        else if (ch.type == 10) dfPlayerCount++;
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
            uint8_t mode = j["mc_mode"] | 0;
            if (mode >= 6 && mode <= 9) {
                if (src == 0) h.ledc = 1;
                break;
            }
            uint8_t segN = (t == 13) ? 8 : 7;
            JsonArrayConst sa = j["seg_sources"];
            for (uint8_t i = 0; i < segN; i++) {
                if (!sa.isNull() && i < sa.size()) { if ((sa[i] | 0) == 0) h.ledc++; }
                else { h.ledc++; }  // default seg_sources[i]=0 → GPIO
            }
            break;
        }
        case 14: if (!(src >= 5 && src <= 7)) h.dac = 1; break;
        case 15: if (src == 0) h.ledc = 1; break;
        case 16: h.ledc = 1; h.timer = 1; break;
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

inline uint8_t i2cWritesFromJson(JsonObjectConst j) {
    uint8_t writes = 0;
    uint8_t t = j["type"] | 0;
    uint8_t src = j["source"] | 0;
    uint8_t mode = j["mc_mode"] | 0;
    switch (t) {
        case 2:
        case 17:
        case 4:
        case 8:
        case 15:
            if (srcOnI2C(src)) writes += 1;
            break;
        case 5:
            if (srcOnI2C(src)) writes += 1;
            if (srcOnI2C(j["pin2_source"] | 0)) writes += 1;
            if (srcOnI2C(j["pin3_source"] | 0)) writes += 1;
            if ((j["color_order"] | 0) >= 4 && srcOnI2C(j["pin4_source"] | 0)) writes += 1;
            break;
        case 6:
            if (srcOnI2C(src)) {
                writes += (mode == 2) ? 3 : 2;
            } else {
                if (srcOnI2C(j["pin2_source"] | 0)) writes += 1;
                if (mode == 2 && srcOnI2C(j["pin3_source"] | 0)) writes += 1;
            }
            break;
        case 7:
            if (srcOnI2C(j["pin2_source"] | 0)) writes += 1;
            if (srcOnI2C(j["pin3_source"] | 0)) writes += 1;
            break;
        case 12:
        case 13: {
            uint8_t n = sevenSegCount(t, mode);
            uint8_t pin2Source = j["pin2_source"] | 0;
            if (mode >= 2 && srcOnI2C(pin2Source)) {
                writes += n;
            } else {
                JsonArrayConst sa = j["seg_sources"];
                for (uint8_t i = 0; i < n; i++) {
                    uint8_t segSource = (!sa.isNull() && i < sa.size()) ? (uint8_t)(sa[i] | 0) : 0;
                    if (srcOnI2C(segSource)) writes += 1;
                }
            }
            if (mode >= 6 && mode <= 9 && srcOnI2C(src)) writes += 1;
            break;
        }
        case 14:
            if (srcOnI2C(src)) writes += 1;
            break;
        case 18:
            if (srcOnI2C(src)) writes += 1;
            if (srcOnI2C(j["pin2_source"] | 0)) writes += 1;
            break;
    }
    return writes;
}

inline PerChannelCost estimateChannelCostFromJson(JsonObjectConst j) {
    PerChannelCost c;
    uint8_t t = j["type"] | 0;
    uint16_t ledCount = j["led_count"] | 0;
    uint8_t colorOrder = j["color_order"] | 0;
    c.cpuUs = ScoringDefs::channelCpuUs(t, j["mc_mode"] | 0);
    c.ramBytes = BASE_CHANNEL_RAM + dmxBufferRamForChannel(t, ledCount, colorOrder, j["mc_resolution"] | 8, j["mc_mode"] | 0);
    if (t == 3) c.cpuUs = ledStripServiceUs(ledCount, colorOrder);
    if (t == 3) c.ramBytes += pixelBufferRam(ledCount, colorOrder) + PIXEL_STRIP_OBJECT_RAM;
    if (t == 7) c.ramBytes += STEPPER_RUNTIME_RAM;
    if (t == 10) c.ramBytes += DFPLAYER_OBJECT_RAM + 100;
    if (t == 16) c.ramBytes += FUNCGEN_OBJECT_RAM;
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
        if (type == 1 && source == 0) dmxCount++;
        else if (type == 10) dfPlayerCount++;
        if (type == 0) hasDimmer = true;
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
        if (type == 0) dimmerCount++;
        else if (type == 16) funcGenCount++;
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
        if (type == 1 && source == 0) dmxCount++;
        else if (type == 10) dfPlayerCount++;
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
