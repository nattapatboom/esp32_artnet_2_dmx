#include "scoring.h"

// ═══════════════════════════════════════
//  PER-TYPE ESTIMATES (OutputChannel)
// ═══════════════════════════════════════

uint8_t getPinSource(const OutputChannel& ch, uint8_t pinIndex) {
    if (pinIndex < 9) {
        return ch.routes[pinIndex].source;
    }
    return 0;
}

bool srcOnI2C(uint8_t src) {
    return src >= 1 && src <= 7;
}

HardwareResource estimateHardware(const OutputChannel& ch) {
    const auto* def = OutputDefs::modeDef(ch.type, ch.mc_mode);
    if (!def) return {};
    HardwareResource h;
    h.ledc = 0;
    h.rmt = def->cost.hardware.rmt;
    h.uart = def->cost.hardware.uart;
    h.dac = def->cost.hardware.dac;
    h.timer = def->cost.hardware.timer;

    uint8_t rmtFromPins = 0;
    for (uint8_t i = 0; i < def->pinCount; i++) {
        uint8_t src = getPinSource(ch, i);
        if (src == 0) {
            if (def->pins[i].hwIfGpio == 1) h.ledc++;
            if (def->pins[i].hwIfGpio == 2) rmtFromPins++;
        }
    }
    h.rmt += rmtFromPins;

    if (ch.type == OutputDefs::TYPE_ANALOG_RGB && ch.color_order < 4 && ch.routes[3].source == 0 && h.ledc > 0) {
        h.ledc--;
    }

    if (ch.type == OutputDefs::TYPE_DAC && ch.routes[0].source == 0) {
        h.dac = 1;
    }

    return h;
}

bool usesI2C(const OutputChannel& ch) {
    const auto* def = OutputDefs::modeDef(ch.type, ch.mc_mode);
    if (!def) return false;
    for (uint8_t i = 0; i < def->pinCount; i++) {
        if (srcOnI2C(getPinSource(ch, i))) return true;
    }
    return false;
}

uint32_t ledStripServiceUs(uint16_t ledCount, uint8_t colorOrder) {
    uint8_t bytesPerPixel = colorOrder >= 4 ? 4 : 3;
    return 80 + ledCount * (bytesPerPixel == 4 ? 4 : 3);
}

uint8_t sevenSegCount(uint8_t type, uint8_t mode) {
    const auto* d = OutputDefs::modeDef(type, (int8_t)mode);
    return d ? d->segmentCount : 0;
}

uint16_t i2cWriteUs() {
    uint32_t speed = sysCfg.i2c_speed;
    if (speed < 10000) speed = 400000;
    return 100 + 27000000U / speed;
}

uint8_t i2cWritesForChannel(const OutputChannel& ch) {
    const auto* def = OutputDefs::modeDef(ch.type, ch.mc_mode);
    if (!def) return 0;
    uint8_t writes = 0;
    for (uint8_t i = 0; i < def->pinCount; i++) {
        if (srcOnI2C(getPinSource(ch, i))) writes++;
    }
    return writes;
}

uint32_t frameTimeUs(uint8_t outputFps) {
    if (outputFps < 1) outputFps = 40;
    return 1000000U / (uint32_t)outputFps;
}

uint32_t acDimmerBackgroundUs(uint8_t dimmerCount, uint8_t outputFps) {
    if (dimmerCount == 0) return 0;
    uint32_t ticksPerFrame = frameTimeUs(outputFps) / 39;
    return ticksPerFrame * (1 + dimmerCount);
}

uint32_t funcGenBackgroundUs(uint8_t funcGenCount, uint8_t outputFps) {
    if (funcGenCount == 0) return 0;
    uint32_t samplesPerFrame = frameTimeUs(outputFps) / 50;
    return samplesPerFrame * 4U * funcGenCount;
}

uint32_t dmxBufferRamForChannel(uint8_t type, uint16_t ledCount, uint8_t colorOrder, uint8_t resolution, uint8_t mode) {
    return OutputDefs::dmxBufferRamForChannel(type, ledCount, colorOrder, resolution, mode);
}

uint32_t pixelBufferRam(uint16_t ledCount, uint8_t colorOrder) {
    return (uint32_t)ledCount * (colorOrder >= 4 ? 4 : 3);
}

PerChannelCost estimateChannelCost(const OutputChannel& ch) {
    PerChannelCost c;
    c.cpuUs = OutputDefs::baseCpuUs(ch.type, ch.mc_mode);
    c.ramBytes = BASE_CHANNEL_RAM + dmxBufferRamForChannel(ch.type, ch.led_count, ch.color_order, ch.mc_resolution, ch.mc_mode);
    auto* def = OutputDefs::modeDef(ch.type, ch.mc_mode);
    if (def) c.ramBytes += def->cost.extraRamBytes;
    if (def != nullptr && (def->cost.flags & OutputDefs::CF_DYN_LED_STRIP)) c.cpuUs = ledStripServiceUs(ch.led_count, ch.color_order);
    if (def != nullptr && (def->cost.flags & OutputDefs::CF_DYN_LED_STRIP)) c.ramBytes += pixelBufferRam(ch.led_count, ch.color_order);
    c.cpuUs += (uint16_t)i2cWritesForChannel(ch) * i2cWriteUs();
    c.ramBytes += (uint32_t)i2cWritesForChannel(ch) * I2C_ROUTE_RAM;
    return c;
}

PerChannelCost espnowMasterCost(uint8_t peerCount, uint8_t universeCount, uint16_t chunkSize) {
    PerChannelCost c;
    uint8_t chunksPerUni = (511 + chunkSize) / chunkSize;
    c.cpuUs = 500 + peerCount * chunksPerUni * 170 + universeCount * 100;
    c.ramBytes  = 512 + peerCount * (chunkSize + 44);
    return c;
}

uint8_t getEspNowPeerCount() {
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
    return count == 0 ? 1 : count;
}

uint8_t countUniqueUniverses(const std::vector<OutputChannel>& chs) {
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

// ═══════════════════════════════════════
//  AGGREGATE SUMS (vector version)
// ═══════════════════════════════════════

HardwareResource totalHardware(const std::vector<OutputChannel>& chs) {
    HardwareResource t;
    bool hasDimmer = false;
    uint8_t dmxCount = 0;
    uint8_t dfPlayerCount = 0;
    for (auto& ch : chs) {
        const auto* def = OutputDefs::modeDef(ch.type, ch.mc_mode);
        t = t + estimateHardware(ch);
        uint16_t flags = def != nullptr ? def->cost.flags : OutputDefs::CF_NONE;
        if (flags & OutputDefs::CF_BG_DIMMER) hasDimmer = true;
        if ((flags & OutputDefs::CF_AGG_DMX) && ch.routes[0].source == 0) dmxCount++;
        else if (flags & OutputDefs::CF_AGG_UART_RESERVED) dfPlayerCount++;
    }
    uint8_t freeUarts = dfPlayerCount >= 2 ? 0 : (uint8_t)(2 - dfPlayerCount);
    uint8_t dmxUartUse = dmxCount < freeUarts ? dmxCount : freeUarts;
    uint8_t dmxRmtUse = dmxCount > freeUarts ? (uint8_t)(dmxCount - freeUarts) : 0;
    t.uart = dfPlayerCount + dmxUartUse;
    t.rmt += dmxRmtUse;
    if (hasDimmer) t.timer += 1;
    return t;
}

CpuBudget totalCpu(const std::vector<OutputChannel>& chs, uint8_t fps) {
    CpuBudget t;
    uint8_t dimmerCount = 0;
    uint8_t funcGenCount = 0;
    for (auto& ch : chs) {
        const auto* def = OutputDefs::modeDef(ch.type, ch.mc_mode);
        uint16_t flags = def != nullptr ? def->cost.flags : OutputDefs::CF_NONE;
        t.usPerFrame += estimateChannelCost(ch).cpuUs;
        if (flags & OutputDefs::CF_BG_DIMMER) dimmerCount++;
        else if (flags & OutputDefs::CF_BG_FUNCGEN) funcGenCount++;
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

RamBudget totalRam(const std::vector<OutputChannel>& chs) {
    RamBudget t;
    uint8_t dmxCount = 0;
    uint8_t dfPlayerCount = 0;
    for (auto& ch : chs) {
        const auto* def = OutputDefs::modeDef(ch.type, ch.mc_mode);
        uint16_t flags = def != nullptr ? def->cost.flags : OutputDefs::CF_NONE;
        t.bytes += estimateChannelCost(ch).ramBytes;
        if ((flags & OutputDefs::CF_AGG_DMX) && ch.routes[0].source == 0) dmxCount++;
        else if (flags & OutputDefs::CF_AGG_UART_RESERVED) dfPlayerCount++;
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
//  JSON BRIDGE
// ═══════════════════════════════════════

OutputChannel channelFromJson(JsonObjectConst j) {
    OutputChannel ch;
    ch.type = j["type"] | 0;
    ch.mc_mode = j["mc_mode"] | 0;
    ch.mc_resolution = j["mc_resolution"] | 8;
    ch.led_count = j["led_count"] | 170;
    ch.color_order = j["color_order"] | 0;
    ch.start_universe = j["start_universe"] | 0;
    
    if (j.containsKey("pins")) {
        loadChannelPins(ch, j["pins"].as<JsonArrayConst>());
    } else {
        JsonArrayConst empty;
        loadChannelPins(ch, empty);
    }
    return ch;
}

std::vector<OutputChannel> channelsFromJson(JsonArrayConst arr) {
    std::vector<OutputChannel> chs;
    chs.reserve(arr.size());
    for (JsonObjectConst j : arr) chs.push_back(channelFromJson(j));
    return chs;
}

uint8_t getPinSourceFromJson(JsonObjectConst j, uint8_t pinIndex) { return getPinSource(channelFromJson(j), pinIndex); }
HardwareResource estimateHardwareFromJson(JsonObjectConst j) { return estimateHardware(channelFromJson(j)); }
bool usesI2CFromJson(JsonObjectConst j) { return usesI2C(channelFromJson(j)); }
uint8_t i2cWritesFromJson(JsonObjectConst j) { return i2cWritesForChannel(channelFromJson(j)); }
PerChannelCost estimateChannelCostFromJson(JsonObjectConst j) { return estimateChannelCost(channelFromJson(j)); }

HardwareResource totalHardwareFromJson(JsonArrayConst arr) { return totalHardware(channelsFromJson(arr)); }

CpuBudget totalCpuFromJson(JsonArrayConst arr, uint8_t fps) {
    auto chs = channelsFromJson(arr);
    return totalCpu(chs, fps);
}

RamBudget totalRamFromJson(JsonArrayConst arr) {
    auto chs = channelsFromJson(arr);
    return totalRam(chs);
}

ScoreBlocker checkScores(const HardwareResource& hw, const CpuBudget& cpu, const RamBudget& ram, uint8_t fps) {
    if (!hardwareWithinLimit(hw)) return ScoreBlocker::HardwareLimit;
    if (cpu.usPerFrame > CpuBudget::limit(fps)) return ScoreBlocker::CpuLimit;
    if (ram.bytes > RamBudget::limit()) return ScoreBlocker::RamLimit;
    return ScoreBlocker::None;
}

const char* scoreBlockerName(ScoreBlocker b) {
    switch (b) {
        case ScoreBlocker::HardwareLimit: return "Hardware limit exceeded";
        case ScoreBlocker::CpuLimit:      return "CPU time exceeded — reduce channels or lower FPS";
        case ScoreBlocker::RamLimit:      return "RAM budget exceeded — reduce channels";
        default:                          return "";
    }
}

uint8_t countUniqueUniversesFromJson(JsonArrayConst arr) { return countUniqueUniverses(channelsFromJson(arr)); }

ScoreBlocker checkScoresFromJson(JsonArrayConst arr, uint8_t fps) {
    HardwareResource hw = totalHardwareFromJson(arr);
    CpuBudget cpu = totalCpuFromJson(arr, fps);
    RamBudget ram = totalRamFromJson(arr);
    return checkScores(hw, cpu, ram, fps);
}

const char* outputTypeName(uint8_t type) {
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
