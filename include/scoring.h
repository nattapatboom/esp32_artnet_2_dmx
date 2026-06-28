#ifndef SCORING_H
#define SCORING_H

#include <Arduino.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include "output_common.h"
#include "output_control.h"
#include "scoring_limits.h"

// ═══════════════════════════════════════
//  HARDWARE RESOURCE COUNTS
// ═══════════════════════════════════════

constexpr uint8_t MAX_LEDC_RESOURCE = ScoringLimits::MAX_LEDC;
constexpr uint8_t MAX_RMT_RESOURCE = ScoringLimits::MAX_RMT;
constexpr uint8_t MAX_UART_RESOURCE = ScoringLimits::MAX_UART;
constexpr uint8_t MAX_DAC_RESOURCE = ScoringLimits::MAX_DAC;
constexpr uint8_t MAX_TIMER_RESOURCE = ScoringLimits::MAX_TIMER;

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
    static constexpr uint32_t BASE_OVERHEAD_US = ScoringLimits::BASE_OVERHEAD_US;
    static constexpr uint32_t SAFETY_RESERVE_US = ScoringLimits::SAFETY_RESERVE_US;
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
        int32_t available = freeHeap - ScoringLimits::KEEP_FREE_BYTES;
        if (available < 0) available = 0;
        if (available > ScoringLimits::LIMIT_CAP_BYTES) available = ScoringLimits::LIMIT_CAP_BYTES;
        return (uint32_t)available;
    }
};

// ═══════════════════════════════════════
//  PER-TYPE ESTIMATES (OutputChannel)
// ═══════════════════════════════════════

uint8_t getPinSource(const OutputChannel& ch, uint8_t pinIndex);

HardwareResource estimateHardware(const OutputChannel& ch);

bool srcOnI2C(uint8_t src);
bool usesI2C(const OutputChannel& ch);
uint32_t ledStripServiceUs(uint16_t ledCount, uint8_t colorOrder);
uint8_t sevenSegCount(uint8_t type, uint8_t mode);
uint16_t i2cWriteUs();
uint8_t i2cWritesForChannel(const OutputChannel& ch);

constexpr uint32_t BASE_CHANNEL_RAM = ScoringLimits::BASE_CHANNEL_BYTES;
constexpr uint32_t RMT_DMX_DRIVER_RAM = 5150UL * sizeof(rmt_item32_t) + 32;
constexpr uint32_t I2C_ROUTE_RAM = ScoringLimits::I2C_ROUTE_BYTES;
constexpr uint32_t DMX_OUTPUT_SERVICE_US = ScoringLimits::DMX_SERVICE_US;

uint32_t frameTimeUs(uint8_t outputFps);
uint32_t acDimmerBackgroundUs(uint8_t dimmerCount, uint8_t outputFps);
uint32_t funcGenBackgroundUs(uint8_t funcGenCount, uint8_t outputFps);
uint32_t dmxBufferRamForChannel(uint8_t type, uint16_t ledCount, uint8_t colorOrder, uint8_t resolution, uint8_t mode);
uint32_t pixelBufferRam(uint16_t ledCount, uint8_t colorOrder);

struct PerChannelCost {
    uint32_t cpuUs = 0;
    uint32_t ramBytes = 0;
};

PerChannelCost estimateChannelCost(const OutputChannel& ch);
PerChannelCost espnowMasterCost(uint8_t peerCount, uint8_t universeCount, uint16_t chunkSize = 200);
uint8_t getEspNowPeerCount();
uint8_t countUniqueUniverses(const std::vector<OutputChannel>& chs);

HardwareResource totalHardware(const std::vector<OutputChannel>& chs);
CpuBudget totalCpu(const std::vector<OutputChannel>& chs, uint8_t fps = 40);
RamBudget totalRam(const std::vector<OutputChannel>& chs);

OutputChannel channelFromJson(JsonObjectConst j);
std::vector<OutputChannel> channelsFromJson(JsonArrayConst arr);
uint8_t getPinSourceFromJson(JsonObjectConst j, uint8_t pinIndex);
HardwareResource estimateHardwareFromJson(JsonObjectConst j);
bool usesI2CFromJson(JsonObjectConst j);
uint8_t i2cWritesFromJson(JsonObjectConst j);
PerChannelCost estimateChannelCostFromJson(JsonObjectConst j);
HardwareResource totalHardwareFromJson(JsonArrayConst arr);
CpuBudget totalCpuFromJson(JsonArrayConst arr, uint8_t fps = 40);
RamBudget totalRamFromJson(JsonArrayConst arr);

enum class ScoreBlocker : uint8_t {
    None,
    HardwareLimit,
    CpuLimit,
    RamLimit
};

ScoreBlocker checkScores(const HardwareResource& hw, const CpuBudget& cpu, const RamBudget& ram, uint8_t fps);
const char* scoreBlockerName(ScoreBlocker b);
uint8_t countUniqueUniversesFromJson(JsonArrayConst arr);
ScoreBlocker checkScoresFromJson(JsonArrayConst arr, uint8_t fps);
const char* outputTypeName(uint8_t type);

#endif
