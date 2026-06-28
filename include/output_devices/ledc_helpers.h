#ifndef OUTPUT_DEVICES_LEDC_HELPERS_H
#define OUTPUT_DEVICES_LEDC_HELPERS_H

#include <Arduino.h>
#include "output_common.h"
#include "output_control.h"

inline uint8_t allocateLedc(uint8_t& index) {
    if (index > 15) return 255;
    return index++;
}

inline uint8_t ledcResolution(OutputChannel& ch) {
    return ch.mc_resolution > 16 ? 16 : ch.mc_resolution;
}

inline uint32_t getDmxValue(OutputChannel& ch) {
    if (!ch.dmxBuffer) return 0;
    uint8_t bytes = getValueByteCount(ch.mc_resolution);
    uint32_t value = 0;
    for (uint8_t i = 0; i < bytes; i++) {
        value = (value << 8) | ch.dmxBuffer[i];
    }
    return value;
}

inline uint32_t calibratedPwmDacDuty(OutputChannel& ch, uint32_t value, uint32_t inputMax, uint32_t outputMax) {
    if (inputMax == 0) return 0;
    uint16_t minPct = ch.pwm_dac_min > 10000 ? 10000 : ch.pwm_dac_min;
    uint16_t maxPct = ch.pwm_dac_max > 10000 ? 10000 : ch.pwm_dac_max;
    uint32_t minDuty = ((uint64_t)outputMax * minPct) / 10000;
    uint32_t maxDuty = ((uint64_t)outputMax * maxPct) / 10000;
    if (maxDuty >= minDuty) return minDuty + ((uint64_t)value * (maxDuty - minDuty)) / inputMax;
    return minDuty - ((uint64_t)value * (minDuty - maxDuty)) / inputMax;
}

inline uint8_t segmentRouteIndex(OutputChannel& ch, uint8_t idx) {
    const OutputDefs::OutputModeDef* def = OutputDefs::modeDef(ch.type, ch.mc_mode);
    bool primaryIsSeg = def != nullptr && def->primaryRouteIsSegment;
    return primaryIsSeg ? idx : (idx + 1);
}

inline uint8_t segmentGpio(OutputChannel& ch, uint8_t idx) {
    uint8_t routeIdx = segmentRouteIndex(ch, idx);
    if (routeIdx >= 9) return 255;
    uint8_t gpio = ch.routes[routeIdx].pin;
    return (gpio > 39) ? 255 : gpio;
}

inline void setupSegmentOutput(OutputChannel& ch, uint8_t idx, bool offState) {
    uint8_t routeIdx = segmentRouteIndex(ch, idx);
    if (routeIdx >= 9) return;

    uint8_t src = ch.routes[routeIdx].source;
    bool inv = ch.routes[routeIdx].invert;
    bool active_off = offState ^ inv;
    if (OutputDefs::isPwmExpanderSource(src)) {
        pcaManager.getOrCreateDriver(ch.routes[routeIdx].addr, src);
        pcaManager.setFrequency(ch.routes[routeIdx].addr, outputCtrl.sharedPcaFrequency(ch.routes[routeIdx].addr), src);
        if (ch.routes[routeIdx].channel != 255) pcaManager.write(ch.routes[routeIdx].addr, ch.routes[routeIdx].channel, active_off ? 4095 : 0, false, src);
        return;
    }
    if (OutputDefs::isDigitalExpanderSource(src)) {
        if (ch.routes[routeIdx].channel != 255) digitalExpanderManager.write(src, ch.routes[routeIdx].addr, ch.routes[routeIdx].channel, active_off, true);
        return;
    }
    uint8_t pin = segmentGpio(ch, idx);
    if (pin != 255) {
        pinMode(pin, OUTPUT);
        digitalWrite(pin, active_off ? HIGH : LOW);
    }
}

inline void writeSegmentOutput(OutputChannel& ch, uint8_t idx, bool state) {
    uint8_t routeIdx = segmentRouteIndex(ch, idx);
    if (routeIdx >= 9) return;

    uint8_t src = ch.routes[routeIdx].source;
    bool inv = ch.routes[routeIdx].invert;
    bool active_state = state ^ inv;
    if (OutputDefs::isPwmExpanderSource(src)) {
        if (ch.routes[routeIdx].channel != 255) pcaManager.write(ch.routes[routeIdx].addr, ch.routes[routeIdx].channel, active_state ? 4095 : 0, false, src);
        return;
    }
    if (OutputDefs::isDigitalExpanderSource(src)) {
        if (ch.routes[routeIdx].channel != 255) digitalExpanderManager.write(src, ch.routes[routeIdx].addr, ch.routes[routeIdx].channel, active_state);
        return;
    }
    uint8_t pin = segmentGpio(ch, idx);
    if (pin != 255) digitalWrite(pin, active_state ? HIGH : LOW);
}

#endif
