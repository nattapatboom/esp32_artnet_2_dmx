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

inline uint8_t segmentGpio(OutputChannel& ch, uint8_t idx) {
    uint8_t gpio = (ch.seg_pins[idx] != 255) ? ch.seg_pins[idx] : (ch.pin + idx);
    return (gpio > 39) ? 255 : gpio;
}

inline void setupSegmentOutput(OutputChannel& ch, uint8_t idx, bool offState) {
    uint8_t src = ch.seg_sources[idx];
    bool inv = (ch.seg_inverts >> idx) & 1;
    bool active_off = offState ^ inv;
    if (src == 1) {
        pcaManager.getOrCreateDriver(ch.seg_addrs[idx]);
        pcaManager.setFrequency(ch.seg_addrs[idx], outputCtrl.sharedPcaFrequency(ch.seg_addrs[idx]));
        if (ch.seg_channels[idx] != 255) pcaManager.write(ch.seg_addrs[idx], ch.seg_channels[idx], active_off ? 4095 : 0);
        return;
    }
    if (src >= 2 && src <= 4) {
        if (ch.seg_channels[idx] != 255) digitalExpanderManager.write(src, ch.seg_addrs[idx], ch.seg_channels[idx], active_off, true);
        return;
    }
    uint8_t pin = segmentGpio(ch, idx);
    if (pin != 255) {
        pinMode(pin, OUTPUT);
        digitalWrite(pin, active_off ? HIGH : LOW);
    }
}

inline void writeSegmentOutput(OutputChannel& ch, uint8_t idx, bool state) {
    uint8_t src = ch.seg_sources[idx];
    bool inv = (ch.seg_inverts >> idx) & 1;
    bool active_state = state ^ inv;
    if (src == 1) {
        if (ch.seg_channels[idx] != 255) pcaManager.write(ch.seg_addrs[idx], ch.seg_channels[idx], active_state ? 4095 : 0);
        return;
    }
    if (src >= 2 && src <= 4) {
        if (ch.seg_channels[idx] != 255) digitalExpanderManager.write(src, ch.seg_addrs[idx], ch.seg_channels[idx], active_state);
        return;
    }
    uint8_t pin = segmentGpio(ch, idx);
    if (pin != 255) digitalWrite(pin, active_state ? HIGH : LOW);
}

#endif
